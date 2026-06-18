#include "KPlayerUI.h"
#include "KPlayer.h"
#include <QMessageBox>
#include <QDebug>

KPlayerUI::KPlayerUI(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);




	// UI → Worker (QueuedConnection: crosses thread boundary safely)
	connect(this, &KPlayerUI::sigPlay, m_instance, &KPlayer::slot_Play);
	connect(this, &KPlayerUI::sigStop, m_instance, &KPlayer::slot_Stop);

	// Worker → UI (auto-resolved to QueuedConnection across threads)
	connect(m_instance, &KPlayer::sigVideoFrame, this, &KPlayerUI::slot_VideoFrame);
	connect(m_instance, &KPlayer::sigError, this, &KPlayerUI::slot_Error);

	// Cleanup: when thread finishes, delete KPlayer safely on its own thread
	connect(m_thread, &QThread::finished, m_instance, &QObject::deleteLater);
	// Cleanup: when thread finishes, delete KPlayer safely on its own thread
	connect(m_Aft_thread, &QThread::finished, m_Aft_instance, &QObject::deleteLater);
	connect(this, &KPlayerUI::sigAftPlay, m_Aft_instance, &KPlayer::slot_Play);
	connect(this, &KPlayerUI::sigAftStop, m_Aft_instance, &KPlayer::slot_Stop);


	// Worker → UI (auto-resolved to QueuedConnection across threads)
	connect(m_Aft_instance, &KPlayer::sigVideoFrame, this, &KPlayerUI::slot_AftVideoFrame);
	connect(m_Aft_instance, &KPlayer::sigError, this, &KPlayerUI::slot_AftError);

	// UI button wiring
	connect(ui._pushButton_Play, &QPushButton::clicked, this, &KPlayerUI::slot_pushButton_Play_clicked);
	connect(ui._pushButton_Stop, &QPushButton::clicked, this, &KPlayerUI::slot_pushButton_Stop_clicked);

	// UI button wiring
	connect(ui._pushButton_AftPlay, &QPushButton::clicked, this, &KPlayerUI::slot_pushButton_PlayAft_clicked);
	connect(ui._pushButton_AftStop, &QPushButton::clicked, this, &KPlayerUI::slot_pushButton_StopAft_clicked);

	m_thread->start();
	m_Aft_thread->start();
}

KPlayerUI::~KPlayerUI()
{
	emit sigStop();
	m_thread->quit();         // ask thread event loop to stop
	m_thread->wait(3000);     // give it 3 seconds to clean up
	delete m_thread;           // delete thread (also deletes m_instance via deleteLater)
}

void KPlayerUI::slot_pushButton_Play_clicked(bool clicked)
{
	VideoParameters params;

	params.IP = "10.51.12.21";
	params.port = 554;
	params.stream_description = "Front SVS Stream";
	params.username = "test";
	params.password = "Password123!";
	// 	params.URL = "rtsp://10.51.12.21:554/live/fc89be67-10d0-4c7d-8d51-66cb958ab128";
	params.URL = "rtsp://10.51.12.21:554/live/5ec91a58-2229-499a-9185-e3f6d86c6cb5"; //SEOS

	if(m_thread || m_instance)
	{
		qDebug() << "Thread and instance already exist, cannot start a new stream.";
		return;
	}
	else
	{
		// Worker thread setup — KPlayer lives entirely on m_thread
		m_thread = new QThread(this);
		m_instance = new KPlayer();           // no parent → required for moveToThread
		m_instance->moveToThread(m_thread);



		// Crosses thread boundary — KPlayer::slot_Play runs on m_thread
		emit sigPlay(params);
	}

}

void KPlayerUI::slot_pushButton_Stop_clicked(bool /*clicked*/)
{
	// Sets m_running = false atomically on the worker thread
	emit sigStop();

}

void KPlayerUI::slot_pushButton_PlayAft_clicked(bool clicked)
{
	VideoParameters params;

	params.IP = "10.51.12.21";
	params.port = 554;
	params.stream_description = "Aft SVS Stream";
	params.username = "test";
	params.password = "Password123!";
	// 	params.URL = "rtsp://10.51.12.21:554/live/fc89be67-10d0-4c7d-8d51-66cb958ab128";
	params.URL = "rtsp://10.51.12.21:554/live/5ec91a58-2229-499a-9185-e3f6d86c6cb5"; //SEOS

	if (m_Aft_thread || m_Aft_thread)
	{
		qDebug() << "Thread and instance already exist, cannot start a new stream.";
		return;
	}
	else
	{
		// Worker thread setup — KPlayer lives entirely on m_thread
		m_Aft_thread = new QThread(this);
		m_Aft_instance = new KPlayer();           // no parent → required for moveToThread
		m_Aft_instance->moveToThread(m_Aft_thread);



		// Crosses thread boundary — KPlayer::slot_Play runs on m_thread
		emit sigAftPlay(params);
	}
}

void KPlayerUI::slot_pushButton_StopAft_clicked(bool clicked)
{
	// Sets m_running = false atomically on the worker thread
	emit sigAftStop();
}

void KPlayerUI::slot_VideoFrame(QImage frame)
{
	// Runs on the UI thread — safe to update widgets here
	qDebug() << "Received video frame of size" << frame.size();

	ui._label_Video->setPixmap(
		QPixmap::fromImage(frame).scaled(
			ui._label_Video->size(),
			Qt::KeepAspectRatio,
			Qt::SmoothTransformation
		)
	);
}

void KPlayerUI::slot_Error(QString message)
{
	QMessageBox::critical(this, "KPlayer Error", message);
}

void KPlayerUI::slot_AftVideoFrame(QImage frame)
{
	// Runs on the UI thread — safe to update widgets here
	qDebug() << "Received video frame of size" << frame.size();

	ui._label_AftVideo->setPixmap(
		QPixmap::fromImage(frame).scaled(
			ui._label_AftVideo->size(),
			Qt::KeepAspectRatio,
			Qt::SmoothTransformation
		)
	);
}

void KPlayerUI::slot_AftError(QString message)
{
	QMessageBox::critical(this, "Aft KPlayer Error", message);

}

