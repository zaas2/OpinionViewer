#pragma once

#include <QObject>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QVersionNumber>
#include <QDebug>
#include <QApplication>

class UpdateManager : public QObject
{
	Q_OBJECT

public:
	explicit UpdateManager(QObject* parent = nullptr);

	void checkUpdate(const QString& currentVersion, const QString& repoName);

private slots:
	// 进程结束时的回调
	void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
	QString m_currentVersion;
	QProcess* m_process;
};