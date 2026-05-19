#pragma once

#include <QtWidgets/QDialog>
#include <QSortFilterProxyModel>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QMessageBox>
#include "ui_OpinionViewer.h"
#include "baseWindow.h"
#include "MyTableView.h"
#include "../DbSearcher/DbWorker.h"
#include "../DocParser/ParserData.h" // 🚀 引入干净的数据契约

class OpinionViewer : public baseWindow
{
	Q_OBJECT

public:
	OpinionViewer(QWidget* parent = nullptr);
	~OpinionViewer();

private slots:
	// 搜索与检索
	void performSearch();

	// 文件导入相关
	void onSelectFiles();
	void onSelectFolder();
	void onBtnImportClicked();

	// 🚀 新增：导出到 Word/Doc 功能
	void onBtnExportClicked();

signals:
	// 跨线程通讯专用信号 (动态汇报解析进度)
	void sigUpdateProgress(int current, int total, const QString& fileName);

protected:
	// 全局拖拽拦截
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dropEvent(QDropEvent* event) override;

private:
	// 🚀 辅助函数：将路径转为文件数组
	std::vector<std::wstring> collectFilesFromPaths(const QList<QString>& paths);

	// 🚀 辅助函数：启动纯净的多线程兵团作战
	void startExtractionThread(const std::vector<std::wstring>& backendPaths, const QString& displayPath);

private:
	Ui::OpinionViewer ui;
	MyTableModel* m_model = nullptr;
	DbWorker m_dbWorker;
	QTimer* m_searchTimer = nullptr;
	QSortFilterProxyModel* m_proxyModel = nullptr;
};