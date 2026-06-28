#include "UpdateManager.h"
#include <QFont>
#include <QPushButton>

UpdateManager::UpdateManager(QObject* parent)
	: QObject(parent)
{
	m_process = new QProcess(this);
	connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &UpdateManager::onProcessFinished);
}

void UpdateManager::checkUpdate(const QString& currentVersion, const QString& repoName)
{
	// 🚀 防抖保护：如果上一个检查进程还在跑，直接忽略，防止进程冲突
	if (m_process->state() != QProcess::NotRunning) {
		return;
	}

	m_currentVersion = currentVersion;

	// GitHub API 地址
	QString url = QString("https://api.github.com/repos/zaas2/%1/releases/latest").arg(repoName);

	// PowerShell 命令：强制 TLS 1.2，解决部分老系统请求 GitHub 失败的问题
	QString psCommand = QString(
		"[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; "
		"[Console]::OutputEncoding = [System.Text.Encoding]::UTF8; "
		"$response = Invoke-WebRequest -Uri '%1' -Headers @{'User-Agent'='App-Updater'} -UseBasicParsing; "
		"Write-Output $response.Content"
	).arg(url);

	QStringList params;
	params << "-NoProfile" << "-Command" << psCommand;

	// 隐藏控制台黑框启动
	m_process->start("powershell", params);
}

void UpdateManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	// 网络错误或 PowerShell 崩溃，作为静默检查，直接退出不打扰用户
	if (exitStatus != QProcess::NormalExit || exitCode != 0)
	{
		return;
	}

	QByteArray data = m_process->readAllStandardOutput();
	QJsonDocument doc = QJsonDocument::fromJson(data);

	if (!doc.isObject()) return;

	QJsonObject obj = doc.object();
	QString remoteTag = obj["tag_name"].toString();

	if (remoteTag.isEmpty()) return;

	// --- 版本号清洗逻辑 ---
	QString cleanRemoteVersion = remoteTag;
	if (cleanRemoteVersion.startsWith("v", Qt::CaseInsensitive))
	{
		cleanRemoteVersion = cleanRemoteVersion.mid(1);
	}

	QString cleanLocalVersion = m_currentVersion;
	if (cleanLocalVersion.startsWith("v", Qt::CaseInsensitive))
	{
		cleanLocalVersion = cleanLocalVersion.mid(1);
	}

	QVersionNumber local = QVersionNumber::fromString(cleanLocalVersion);
	QVersionNumber remote = QVersionNumber::fromString(cleanRemoteVersion);

	// --- 发现新版本 ---
	if (local < remote)
	{
		QString downloadUrl = obj["html_url"].toString();

		QMessageBox msgBox(QApplication::activeWindow());
		QFont font("Microsoft YaHei", 10);
		msgBox.setFont(font);

		msgBox.setWindowTitle(tr("发现新版本"));
		msgBox.setText(tr("检查到新版本: %1\n\n是否前往 GitHub 下载最新版？").arg(remoteTag));
		msgBox.setIcon(QMessageBox::Information); // 换成 Information 更友好一点

		// 优化按钮文案
		QAbstractButton* btnYes = msgBox.addButton(tr("前往下载"), QMessageBox::YesRole);
		msgBox.addButton(tr("暂不更新"), QMessageBox::NoRole);

		msgBox.exec();

		// 处理点击事件
		if (msgBox.clickedButton() == btnYes)
		{
			QDesktopServices::openUrl(QUrl(downloadUrl));
		}
	}
}