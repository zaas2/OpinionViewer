#include "baseWindow.h"
#include <QPropertyAnimation>

baseWindow::baseWindow(QWidget *parent)
	: QDialog(parent)
{
	//ui.setupUi(this);
	mHelper = new FramelessHelper(this);
	mHelper->activateOn(this);              //激活当前窗体
	mHelper->setTitleHeight(70);            //设置窗体的标题栏高度
	mHelper->setWidgetMovable(true);        //设置窗体可移动
	mHelper->setWidgetResizable(true);      //设置窗体可缩放
}

baseWindow::~baseWindow()
{}

void baseWindow::onButtonCloseClicked()
{
	setMinimumSize(0, 0);
	QPropertyAnimation* closeAnimation = new QPropertyAnimation(this, "geometry");
	closeAnimation->setStartValue(geometry());
	closeAnimation->setEndValue(QRect(geometry().x(), geometry().y() + height() / 2, width(), 0));
	closeAnimation->setDuration(500);
	//AnimationState = false;
	connect(closeAnimation, SIGNAL(finished()), this, SLOT(close()));
	connect(closeAnimation, SIGNAL(valueChanged()), this, SLOT(isPlaying()));
	closeAnimation->start(QAbstractAnimation::DeleteWhenStopped);
}