#include "OpinionViewer.h"
#include <QtWidgets/QApplication>
#include <QDir>
#include <objbase.h>
#include <QThreadPool>
#include "../DocParser/OpinionCleaner.h"

int main(int argc, char* argv[])
{
	QThreadPool::globalInstance()->setMaxThreadCount(16);
	QApplication app(argc, argv);

	QString qtIniPath = QCoreApplication::applicationDirPath() + QDir::separator() + "setting.ini";

	std::wstring stdIniPath = qtIniPath.toStdWString();
	OpinionCleaner::loadConfig(stdIniPath);
	OpinionViewer window;
	window.show();

	return app.exec();
}
