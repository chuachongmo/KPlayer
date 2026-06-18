#include "KPlayer.h"

KPlayer::KPlayer()
    : QObject(nullptr)
{}

KPlayer::~KPlayer()
{
    m_running = false;
    closeStream();
}

// ─── AVIOInterruptCB ───────────────────────────────────────────────────────
// Called by FFmpeg periodically during blocking I/O (av_read_frame, avformat_open_input, etc.)
// Must be a static function — FFmpeg calls it as a plain C function pointer
// Returns: 1 = abort the operation, 0 = continue
int KPlayer::interruptCallback(void* ctx)
{
    KPlayer* self = static_cast<KPlayer*>(ctx);
    return self->m_running.load() ? 0 : 1;
}

// ─── Slots ─────────────────────────────────────────────────────────────────
void KPlayer::slot_Play(VideoParameters params)
{
    if (m_running)
    {
        return;
    }
    
    m_running = true;


    if (!initialize(params))
    {
        emit sigError("Initialization failed");
        return;
    }

    if (!connectToStream())
    {
        emit sigError("Failed to connect to stream");
        return;
    }

    // Blocking — returns only when m_running = false or stream ends
    processStream();
}

// ⚠️ Connected via Qt::DirectConnection — runs on the CALLER'S thread (UI thread)
// Only touches m_running (atomic) — no FFmpeg or Qt object access here
// The interrupt callback picks up m_running = false and unblocks av_read_frame
void KPlayer::slot_Stop()
{
    m_running = false;
    // AVIOInterruptCB will fire inside av_read_frame and return AVERROR_EXIT
    // processStream() loop will exit and call closeStream() on the worker thread
}

// ─── Pipeline ──────────────────────────────────────────────────────────────
bool KPlayer::initialize(VideoParameters params)
{
    m_test_client = new RTSPClient(
        params.IP.toStdString(),
        params.port,
		params.stream_description.toStdString(),
        params.URL.toStdString(),
        params.username.toStdString(),
        params.password.toStdString()
    );
    m_test_client->initialize_socket();
    m_test_client->initiate_handshake();
    return true;
}

bool KPlayer::connectToStream()
{
    avformat_network_init();

    // Allocate context first so we can set the interrupt callback before
    // avformat_open_input — this makes even the initial connection interruptible
    m_fmtCtx = avformat_alloc_context();

    // Register interrupt callback — FFmpeg polls this during ALL blocking I/O
    m_fmtCtx->interrupt_callback.callback = &KPlayer::interruptCallback;
    m_fmtCtx->interrupt_callback.opaque   = this;

    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "protocol_whitelist", "file,udp,rtp,crypto,data", 0);
    av_dict_set(&opts, "fflags",    "nobuffer",  0);
    av_dict_set(&opts, "flags",     "low_delay", 0);
    av_dict_set(&opts, "max_delay", "0",         0);

    int ret = avformat_open_input(&m_fmtCtx, "stream.sdp", nullptr, &opts);
    av_dict_free(&opts);

    if (ret < 0)
    {
        char err[256];
        av_strerror(ret, err, sizeof(err));
        std::cerr << "Failed to open SDP: " << err << std::endl;
        return false;
    }

    if (avformat_find_stream_info(m_fmtCtx, nullptr) < 0)
        return false;

    for (unsigned int i = 0; i < m_fmtCtx->nb_streams; i++)
    {
        if (m_fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            m_videoStream = static_cast<int>(i);
            break;
        }
    }

    if (m_videoStream == -1)
        return false;

    const AVCodec* codec = avcodec_find_decoder(
        m_fmtCtx->streams[m_videoStream]->codecpar->codec_id
    );

    if (!codec)
        return false;

    m_codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(m_codecCtx, m_fmtCtx->streams[m_videoStream]->codecpar);

    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0)
        return false;

    m_pkt   = av_packet_alloc();
    m_frame = av_frame_alloc();
    return true;
}

bool KPlayer::processStream()
{
    if (!m_fmtCtx || !m_codecCtx || !m_pkt || !m_frame)
        return false;

    m_running = true;
    bool firstKeyframeReceived = false;

    // av_read_frame blocks until a packet arrives OR interruptCallback returns 1
    while (m_running && av_read_frame(m_fmtCtx, m_pkt) >= 0)
    {
        if (m_pkt->stream_index != m_videoStream)
        {
            av_packet_unref(m_pkt);
            continue;
        }

        // Skip until first IDR keyframe — avoids co-located POC warnings
        if (!firstKeyframeReceived)
        {
            if (!(m_pkt->flags & AV_PKT_FLAG_KEY))
            {
                av_packet_unref(m_pkt);
                continue;
            }
            firstKeyframeReceived = true;
        }

        int ret = avcodec_send_packet(m_codecCtx, m_pkt);
        av_packet_unref(m_pkt);

        if (ret < 0)
            continue;

        while (m_running)
        {
            ret = avcodec_receive_frame(m_codecCtx, m_frame);

            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;

            if (ret < 0)
                break;

            if (!m_swsCtx)
            {
                m_swsCtx = sws_getContext(
                    m_frame->width, m_frame->height,
                    static_cast<AVPixelFormat>(m_frame->format),
                    m_frame->width, m_frame->height,
                    AV_PIX_FMT_RGB24,
                    SWS_BILINEAR, nullptr, nullptr, nullptr
                );

                if (!m_swsCtx)
                    break;
            }

            QImage image(m_frame->width, m_frame->height, QImage::Format_RGB888);
            uint8_t* dest[1]  = { image.bits() };
            int destStride[1] = { m_frame->width * 3 };

            sws_scale(
                m_swsCtx,
                m_frame->data, m_frame->linesize, 0, m_frame->height,
                dest, destStride
            );

            emit sigVideoFrame(image.copy());
            av_frame_unref(m_frame);
        }
    }

    // Always clean up on the worker thread after loop exits
    closeStream();
    return true;
}

// ─── Resource cleanup — must always run on the worker thread ───────────────
void KPlayer::closeStream()
{
    if (m_swsCtx)     { sws_freeContext(m_swsCtx);        m_swsCtx    = nullptr; }
    if (m_frame)      { av_frame_free(&m_frame);           m_frame     = nullptr; }
    if (m_pkt)        { av_packet_free(&m_pkt);            m_pkt       = nullptr; }
    if (m_codecCtx)   { avcodec_free_context(&m_codecCtx); m_codecCtx  = nullptr; }
    if (m_fmtCtx)     { avformat_close_input(&m_fmtCtx);   m_fmtCtx    = nullptr; }
    if (m_test_client){ delete m_test_client;               m_test_client = nullptr; }
    m_videoStream = -1;
}

