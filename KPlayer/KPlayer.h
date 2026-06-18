#pragma once
#include <QObject>
#include <atomic>
#include "VideoParameters.h"
#include "RTSPClient.h"
#include <QImage>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/dict.h>
#include <libswscale/swscale.h>
}

class KPlayer : public QObject
{
    Q_OBJECT

public:
    explicit KPlayer();
    ~KPlayer();

    bool initialize(VideoParameters params);
    bool connectToStream();
    bool processStream();

public slots:
    void slot_Play(VideoParameters params);
    void slot_Stop();   // safe to call from any thread — only touches atomic flag

signals:
    void sigVideoFrame(QImage frame);
    void sigError(QString message);

private:
    void closeStream(); // releases all FFmpeg resources — always called on worker thread

    // FFmpeg interrupt callback — called by FFmpeg during blocking I/O
    // Returns 1 to abort, 0 to continue
    static int interruptCallback(void* ctx);

    RTSPClient*           m_test_client  = nullptr;
    AVFormatContext*      m_fmtCtx       = nullptr;
    AVCodecContext*       m_codecCtx     = nullptr;
    AVPacket*             m_pkt          = nullptr;
    AVFrame*              m_frame        = nullptr;
    SwsContext*           m_swsCtx       = nullptr;
    int                   m_videoStream  = -1;

    // Written from UI thread (slot_Stop via DirectConnection), read from worker thread
    // and from AVIOInterruptCB — must be atomic
    std::atomic<bool>     m_running      { false };
};

