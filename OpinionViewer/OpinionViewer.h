#pragma once

// --- Qt Includes ---
#include <QtWidgets/QDialog>
#include <QSortFilterProxyModel>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QProgressDialog>
#include <QTimer>
#include <QKeyEvent>

// --- STL Includes ---
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <atomic>
#include <stack>

// --- Custom Includes ---
#include "ui_OpinionViewer.h"
#include "baseWindow.h"
#include "MyTableView.h" 
#include "../DbSearcher/DbWorker.h"
#include "../DocParser/ParserData.h"

// 多线程解析结果数据包
struct ExtractionResult {
	std::vector<OpinionRecord> newRecords;
	std::unordered_map<std::wstring, FileMetaData> metaCache; // 缓存文件的真实元数据
	int skippedCount = 0;
	int movedCount = 0;
	int parsedCount = 0;
};

class OpinionViewer : public baseWindow
{
	Q_OBJECT

public:
	explicit OpinionViewer(QWidget* parent = nullptr);
	~OpinionViewer();

protected:
	// 拖拽与全局事件过滤
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dropEvent(QDropEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;

private slots:
	// 检索与显示刷新
	void performSearch();

	// 来源选择
	void onSelectFiles();
	void onSelectFolder();
	void onUndoTriggered();

	// 数据导出与同步
	void onBtnImportClicked();        // 执行增量入库
	void onBtnResetDatabaseClicked(); // 执行全量覆写
	void onBtnExportClicked();        // 导出至 Word

	// 多线程状态回调
	void onExtractionProgress(int current, int total, const QString& fileName, const QString& statusText);
	void onExtractionFinished(int parsedCount, int skippedCount, int movedCount, double costSeconds);

signals:
	// 跨线程 UI 通讯信号
	void sigUpdateProgress(int current, int total, const QString& fileName, const QString& statusText);
	void sigExtractionFinished(int parsedCount, int skippedCount, int movedCount, double costSeconds);

private:
	// --- 初始化阶段辅助函数 ---
	void initWindowStyle();
	void initDatabaseWorker();
	void initTableViewAndModels();
	void setupConnections();

	std::vector<std::wstring> collectFilesFromPaths(const QList<QString>& paths);
	void startExtractionThread(const std::vector<std::wstring>& backendPaths, const QString& displayPath, bool isOverwriteMode = false);
	ExtractionResult executeScanAndParse(const std::vector<std::wstring>& backendPaths, bool isOverwriteMode, std::shared_ptr<std::atomic<bool>> isCanceled);
	bool extractImportData(std::vector<OpinionRecord>& outRecords, std::vector<FileMetaData>& outMetaData);

	// --- 配置与状态维护 ---
	void updateDefaultScanPathInIni(const QString& newPath);
	void refreshTableView();

private:
	// UI 与数据模型组件
	Ui::OpinionViewer ui;

	MyTableModel* m_dbModel = nullptr;
	QSortFilterProxyModel* m_dbProxyModel = nullptr;

	MyTableModel* m_importModel = nullptr;
	QSortFilterProxyModel* m_importProxyModel = nullptr;

	QTimer* m_searchTimer = nullptr;

	struct UndoAction {
		int mode; // 0 代表历史库(DB)，1 代表沙盒区(Import)
		std::vector<OpinionRecord> records;
	};
	std::stack<UndoAction> m_undoStack;

	// 数据库工作流与扫描状态缓存
	DbWorker m_dbWorker;
	std::unordered_map<std::wstring, FileMetaData> m_currentScanMetaCache;
};