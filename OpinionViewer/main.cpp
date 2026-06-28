#include "OpinionViewer.h"
#include <QtWidgets/QApplication>
#include <QDir>
#include <objbase.h>
#include <QThreadPool>
#include "../DocParser/OpinionCleaner.h"
#include "UpdateManager.h" // 🚀 引入更新管理器

int main(int argc, char* argv[])
{
	// 提升线程池上限，榨干多核 CPU 性能
	QThreadPool::globalInstance()->setMaxThreadCount(16);
	QApplication app(argc, argv);

	// 初始化清洗引擎的配置
	QString qtIniPath = QCoreApplication::applicationDirPath() + QDir::separator() + "setting.ini";
	std::wstring stdIniPath = qtIniPath.toStdWString();
	OpinionCleaner::loadConfig(stdIniPath);

	// 启动主窗口
	OpinionViewer window;
	window.show();

	// 🚀 挂载静默更新检查 
	// 将当前版本号和 GitHub 仓库名传入，它会在后台静默请求，如果有新版本会自动弹窗
	UpdateManager updater;
	updater.checkUpdate("1.2.0", "OpinionViewer");

	return app.exec();
}