#pragma once

#include <QDialog>
//#include "ui_baseWindow.h"
#include "FramelessHelper.h"

class baseWindow : public QDialog
{
	Q_OBJECT

public:
	baseWindow(QWidget *parent = nullptr);
	~baseWindow();

public slots:
	void onButtonCloseClicked();

private:
	FramelessHelper* mHelper = nullptr;   //无边框窗口工具类
};
