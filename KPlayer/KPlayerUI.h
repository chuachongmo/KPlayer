#pragma once

#include <QtWidgets/QWidget>
#include <QThread>
#include <QLabel>
#include "VideoParameters.h"
#include "ui_KPlayer.h"

class KPlayer;

class KPlayerUI : public QWidget
{
    Q_OBJECT

public:
    explicit KPlayerUI(QWidget* parent = nullptr);
    ~KPlayerUI();

signals:
    void sigPlay(VideoParameters params);
    void sigStop();

	void sigAftPlay(VideoParameters params);
	void sigAftStop();

public slots:
	void slot_pushButton_Play_clicked(bool clicked);
	void slot_pushButton_Stop_clicked(bool clicked);

	void slot_pushButton_PlayAft_clicked(bool clicked);
	void slot_pushButton_StopAft_clicked(bool clicked);

	void slot_VideoFrame(QImage frame);
	void slot_Error(QString message);

	void slot_AftVideoFrame(QImage frame);
	void slot_AftError(QString message);

private:
	Ui::KPlayerClass  ui;
	KPlayer* m_instance = nullptr;
	QThread* m_thread = nullptr;
	KPlayer* m_Aft_instance = nullptr;
	QThread* m_Aft_thread = nullptr;
};

