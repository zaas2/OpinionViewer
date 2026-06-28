#include "OpinionViewer.h"
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QIcon>
#include <QSvgRenderer> 
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QMimeData>
#include <QFileInfo>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QtConcurrent/QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>
#include <QProgressDialog>
#include <QDirIterator>
#include <QMenu>
#include <QTextStream>
#include <windows.h>
#include <iostream>
#include <QStringList>
#include <QShortcut>

#include "../DocParser/DocxParser.h"
#include "../DocParser/OldDocParser.h"
#include "../DocParser/ExcelParser.h"
#include "../DocParser/TxtParser.h"
#include "../DocParser/OpinionCleaner.h"
#include "../common/FileUtil.h"

// 界面图标与样式辅助函数
QIcon createSettingsIcon() {
	QPixmap pixmap(30, 30);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	QString svgData = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#DCDCDC'>"
		"<path d='M19.43 12.98c.04-.32.07-.64.07-.98s-.03-.66-.07-.98l2.11-1.65c.19-.15.24-.42.12-.64l-2-3.46c-.12-.22-.39-.3-.61-.22l-2.49 1c-.52-.4-1.08-.73-1.69-.98l-.38-2.65C14.46 2.18 14.25 2 14 2h-4c-.25 0-.46.18-.49.42l-.38 2.65c-.61.25-1.17.59-1.69.98l-2.49-1c-.23-.09-.49 0-.61.22l-2 3.46c-.13.22-.07.49.12.64l2.11 1.65c-.04.32-.07.65-.07.98s.03.66.07.98l-2.11 1.65c-.19.15-.24.42-.12.64l2 3.46c.12.22.39.3.61.22l2.49-1c.52.4 1.08.73 1.69.98l.38 2.65c.03.24.24.42.49.42h4c.25 0 .46-.18.49-.42l-.38-2.65c.61-.25 1.17-.59 1.69-.98l2.49 1c.23.09.49 0 .61-.22l2-3.46c.12-.22.07-.49-.12-.64l-2.11-1.65zM12 15.5c-1.93 0-3-1.57-3-3.5s1.07-3.5 3-3.5 3 1.57 3 3.5-1.07 3.5-3 3.5z'/></svg>";
	QSvgRenderer renderer(svgData.toUtf8());
	renderer.render(&painter);
	return QIcon(pixmap);
}

void loadStyleSheet() {
	QString qssPath = ":/OpinionViewer/style.qss";
	QFile file(qssPath);
	if (file.open(QFile::ReadOnly | QFile::Text)) {
		qApp->setStyleSheet(QString::fromUtf8(file.readAll()));
		file.close();
	}
}

OpinionViewer::OpinionViewer(QWidget* parent) : baseWindow(parent) {
	ui.setupUi(this);
	loadStyleSheet();
	setAcceptDrops(true);
	setWindowIcon(QIcon(":/OpinionViewer/rc/chaxuan.png"));

	// 🚀 分而治之，各司其职
	initWindowStyle();
	initDatabaseWorker();
	initTableViewAndModels();
	setupConnections();

	// 延迟闪电检索
	QTimer::singleShot(50, this, &OpinionViewer::performSearch);
}

void OpinionViewer::initWindowStyle()
{
	setAttribute(Qt::WA_TranslucentBackground);
	setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);

	// 窗口阴影
	QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect(this);
	shadowEffect->setOffset(0, 0);
	shadowEffect->setColor(QColor(0, 0, 0, 160));
	shadowEffect->setBlurRadius(10);
	ui.widgetBg->setGraphicsEffect(shadowEffect);

	ui.btnSettings->setIcon(createSettingsIcon());
	ui.btnSettings->raise(); ui.btnMin->raise(); ui.btnMax->raise(); ui.btnClose->raise();

	ui.pageImport->setStyleSheet(
		"QWidget#pageImport {"
		"  border: 2px solid #E6A23C;"
		"  border-radius: 6px;"
		"  background-color: rgba(230, 162, 60, 0.05);"
		"}"
	);

	// 默认显示历史库
	ui.stackedWidget->setCurrentIndex(0);
}

void OpinionViewer::initDatabaseWorker()
{
	QString dbPath = QCoreApplication::applicationDirPath() + "/opinions.db";
	bool isDbFileExists = QFileInfo::exists(dbPath);
	m_dbWorker.initDatabase(dbPath.toStdWString());

	if (!isDbFileExists || m_dbWorker.getTotalCount() == 0) {
		ui.btnUpdateDb->hide();
	}
	else {
		ui.btnUpdateDb->show();
	}
}

void OpinionViewer::initTableViewAndModels()
{
	auto setupTableStyle = [](QTableView* tv) {
		tv->setSelectionMode(QAbstractItemView::ExtendedSelection);
		tv->setSelectionBehavior(QAbstractItemView::SelectRows);
		tv->setFont(QFont("Microsoft YaHei", 10));
		tv->setWordWrap(true);
		tv->setTextElideMode(Qt::ElideRight);
		tv->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);
		tv->verticalHeader()->setDefaultSectionSize(42);
		tv->verticalHeader()->setVisible(false);
		tv->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
		};

	setupTableStyle(ui.tableViewDB);
	setupTableStyle(ui.tableViewImport);

	QStringList headers = { "意见内容", "意见回复", "提取来源", "文件指纹" };

	// ================= A套：历史库模型 =================
	m_dbModel = new MyTableModel(this);
	m_dbModel->setHeaders(headers);

	m_dbProxyModel = new QSortFilterProxyModel(this);
	m_dbProxyModel->setSourceModel(m_dbModel);
	m_dbProxyModel->setFilterKeyColumn(0);
	m_dbProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	ui.tableViewDB->setModel(m_dbProxyModel);

	QHeaderView* pHeaderDB = ui.tableViewDB->horizontalHeader();
	pHeaderDB->setStretchLastSection(false);
	pHeaderDB->setSectionResizeMode(0, QHeaderView::Stretch);
	pHeaderDB->setSectionResizeMode(1, QHeaderView::Interactive);
	pHeaderDB->setSectionResizeMode(2, QHeaderView::Interactive);
	pHeaderDB->setSectionResizeMode(3, QHeaderView::Fixed);
	ui.tableViewDB->setColumnWidth(1, 150);
	ui.tableViewDB->setColumnWidth(2, 120);
	ui.tableViewDB->setColumnHidden(3, true);

	// ================= B套：沙盒加工模型 =================
	m_importModel = new MyTableModel(this);
	m_importModel->setHeaders(headers);

	m_importProxyModel = new QSortFilterProxyModel(this);
	m_importProxyModel->setSourceModel(m_importModel);
	m_importProxyModel->setFilterKeyColumn(0);
	m_importProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	ui.tableViewImport->setModel(m_importProxyModel);

	QHeaderView* pHeaderImport = ui.tableViewImport->horizontalHeader();
	pHeaderImport->setStretchLastSection(false);
	pHeaderImport->setSectionResizeMode(0, QHeaderView::Stretch);
	pHeaderImport->setSectionResizeMode(1, QHeaderView::Interactive);
	pHeaderImport->setSectionResizeMode(2, QHeaderView::Interactive);
	pHeaderImport->setSectionResizeMode(3, QHeaderView::Fixed);
	ui.tableViewImport->setColumnWidth(1, 150);
	ui.tableViewImport->setColumnWidth(2, 120);
	ui.tableViewImport->setColumnHidden(3, true);
}

void OpinionViewer::setupConnections()
{
	// ==============================================================
	// 1. 基础标题栏控制
	// ==============================================================
	connect(ui.btnMin, &QPushButton::clicked, this, &QWidget::showMinimized);
	connect(ui.btnClose, &QPushButton::clicked, this, &baseWindow::onButtonCloseClicked);
	connect(ui.btnMax, &QPushButton::clicked, this, [this]() {
		if (isMaximized()) {
			showNormal();
			ui.btnMax->setText("🗖");
			layout()->setContentsMargins(10, 10, 10, 10);
			ui.widgetBg->setStyleSheet("QWidget#widgetBg { background-color: #1E1E1E; border-radius: 6px; } QWidget#titleBar { border-top-left-radius: 6px; border-top-right-radius: 6px; }");
		}
		else {
			layout()->setContentsMargins(0, 0, 0, 0);
			ui.widgetBg->setStyleSheet("QWidget#widgetBg { background-color: #1E1E1E; border-radius: 0px; } QWidget#titleBar { border-top-left-radius: 0px; border-top-right-radius: 0px; }");
			showMaximized();
			ui.btnMax->setText("🗗");
		}
		});

	// ==============================================================
	// 2. 齿轮设置与关于菜单
	// ==============================================================
	QMenu* settingsMenu = new QMenu(this);
	settingsMenu->setStyleSheet("QMenu { background-color: #252526; color: #D4D4D4; border: 1px solid #3F3F46; padding: 4px; } QMenu::item { padding: 6px 20px; } QMenu::item:selected { background-color: #04395E; color: white; }");
	QAction* actEditRules = new QAction("编辑过滤规则", this);
	QAction* actAbout = new QAction("关于...", this);
	settingsMenu->addAction(actEditRules);
	settingsMenu->addAction(actAbout);

	connect(ui.btnSettings, &QPushButton::clicked, this, [this, settingsMenu]() { settingsMenu->exec(ui.btnSettings->mapToGlobal(QPoint(0, ui.btnSettings->height()))); });

	connect(actEditRules, &QAction::triggered, this, [this]() {
		QString iniPath = QCoreApplication::applicationDirPath() + "/setting.ini";
		QDesktopServices::openUrl(QUrl::fromLocalFile(iniPath));
		});

	connect(actAbout, &QAction::triggered, this, [this]() {
		QMessageBox::about(this, "关于",
			"<h2>施工图审查意见管理工具 V1.2</h2>"
			"<p><b>Code by:    zaas</b></p>"
			"<p><b>Date   :20260618</b></p>"
			"<hr>"
			"<p>一款海量工程审查意见的高效剥离、知识库沉淀的工具。</p>"
		);
		});

	// ==============================================================
	// 3. 全局核心按钮 (导出、重设库、沙盒入库与清空)
	// ==============================================================
	connect(ui.btnUpdateDb, &QPushButton::clicked, this, &OpinionViewer::onBtnResetDatabaseClicked);
	connect(ui.btnExport, &QPushButton::clicked, this, &OpinionViewer::onBtnExportClicked);
	connect(ui.btnImport, &QPushButton::clicked, this, &OpinionViewer::onBtnImportClicked);

	connect(ui.btnCancelImport, &QPushButton::clicked, this, [this]() {
		m_importModel->setDataList(QVector<MyTableModel::Row>());
		ui.stackedWidget->setCurrentIndex(0);
		});

	// ==============================================================
	// 4. 指定来源下拉菜单
	// ==============================================================
	QMenu* browseMenu = new QMenu(this);
	browseMenu->setStyleSheet("QMenu { background-color: #252526; color: #D4D4D4; border: 1px solid #3F3F46; padding: 4px; } QMenu::item { padding: 6px 20px; } QMenu::item:selected { background-color: #04395E; color: white; }");
	QAction* actionFiles = new QAction("选择审查文件", this);
	QAction* actionFolder = new QAction("选择审查意见所在的文件夹", this);
	browseMenu->addAction(actionFiles);
	browseMenu->addAction(actionFolder);
	ui.btnBrowse->setMenu(browseMenu);
	ui.btnBrowse->setText("指定来源 ▾");
	connect(actionFiles, &QAction::triggered, this, &OpinionViewer::onSelectFiles);
	connect(actionFolder, &QAction::triggered, this, &OpinionViewer::onSelectFolder);
	connect(ui.btnBrowse, &QPushButton::clicked, this, &OpinionViewer::onSelectFiles);

	// ==============================================================
	// 5. 双轨制检索与防抖
	// ==============================================================
	m_searchTimer = new QTimer(this);
	m_searchTimer->setSingleShot(true);
	m_searchTimer->setInterval(300);
	connect(m_searchTimer, &QTimer::timeout, this, &OpinionViewer::performSearch);
	connect(ui.lineEditSearchDB, &QLineEdit::textChanged, this, [this]() { if (m_searchTimer) m_searchTimer->start(); });

	connect(ui.lineEditSearchImport, &QLineEdit::textChanged, this, [this](const QString& text) {
		m_importProxyModel->setFilterWildcard("*" + text + "*");
		});

	// ==============================================================
	// 6. 🚀 快捷键与核心业务提炼 (删除/撤销的终极闭环)
	// ==============================================================

	// [搜索快捷键]
	QShortcut* shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), this);
	connect(shortcutSearch, &QShortcut::activated, this, [this]() {
		if (ui.stackedWidget->currentIndex() == 0) { ui.lineEditSearchDB->setFocus(); ui.lineEditSearchDB->selectAll(); }
		else { ui.lineEditSearchImport->setFocus(); ui.lineEditSearchImport->selectAll(); }
		});

	// [撤销快捷键]
	QShortcut* shortcutUndo = new QShortcut(QKeySequence("Ctrl+Z"), this);
	connect(shortcutUndo, &QShortcut::activated, this, &OpinionViewer::onUndoTriggered);

	// 💡 [提炼核心逻辑]：统一处理物理删库、内存弹栈、UI刷新的闭包
	auto performBusinessDelete = [this](MyTableView* tableView, int mode) {
		QList<QStringList> deletedRows = tableView->executeDelete();
		if (deletedRows.isEmpty()) return;

		std::vector<OpinionRecord> recordsToUndo;
		std::vector<std::wstring> contentsToDelete; // 🚀 新增：收集死亡名单

		recordsToUndo.reserve(deletedRows.size());
		contentsToDelete.reserve(deletedRows.size());

		for (const auto& row : deletedRows) {
			if (row.size() < 4) continue;
			OpinionRecord rec;
			rec.content = row[0].toStdWString();
			rec.reply = row[1].toStdWString();
			rec.source = row[2].toStdWString();
			rec.fileHash = row[3].toStdWString();
			recordsToUndo.push_back(rec);

			// 把要删的 content 收集起来
			if (mode == 0) {
				contentsToDelete.push_back(rec.content);
			}
		}

		// 🚀 核心性能跨越：将循环里的单条删除，改为一次性批量斩杀！
		if (mode == 0 && !contentsToDelete.empty()) {
			m_dbWorker.batchDeleteOpinionsByContent(contentsToDelete);
		}

		// 压入撤销栈
		m_undoStack.push({ mode, recordsToUndo });

		// 刷新 UI 状态
		if (mode == 0) {
			if (ui.lineEditSearchDB->text().isEmpty()) {
				ui.lineEditSearchDB->setPlaceholderText(QString("全库展现：当前共 %1 条意见，输入关键字极速过滤...").arg(m_dbWorker.getTotalCount()));
			}
			else {
				ui.lineEditSearchDB->setPlaceholderText(QString("全局检索完成：为您搜出 %1 条相关意见...").arg(m_dbProxyModel->rowCount()));
			}
		}
		};

	// [删除快捷键]
	QShortcut* shortcutDelete = new QShortcut(QKeySequence::Delete, this);
	connect(shortcutDelete, &QShortcut::activated, this, [this, performBusinessDelete]() {
		if (ui.stackedWidget->currentIndex() == 0) {
			performBusinessDelete(ui.tableViewDB, 0);
		}
		else {
			performBusinessDelete(ui.tableViewImport, 1);
		}
		});

	// ==============================================================
	// 7. 🚀 动态业务菜单注入 (剥离 View 与 Controller)
	// ==============================================================
	auto injectBusinessMenu = [this, performBusinessDelete](MyTableView* tableView, int mode) {
		connect(tableView, &MyTableView::aboutToShowMenu, this, [this, tableView, mode, performBusinessDelete](QMenu* menu, const QModelIndex& /*index*/) {
			if (!menu->isEmpty()) menu->addSeparator();

			// 注入删除菜单
			QAction* actDelete = menu->addAction("删除选中行 (Del)");
			connect(actDelete, &QAction::triggered, this, [tableView, mode, performBusinessDelete]() {
				performBusinessDelete(tableView, mode);
				});

			// 注入撤销菜单 (附带状态感知)
			QAction* actUndo = menu->addAction("撤销删除 (Ctrl+Z)");
			actUndo->setEnabled(!m_undoStack.empty());
			connect(actUndo, &QAction::triggered, this, &OpinionViewer::onUndoTriggered);
			});
		};

	// 挂载动态菜单
	injectBusinessMenu(ui.tableViewDB, 0);
	injectBusinessMenu(ui.tableViewImport, 1);
}

OpinionViewer::~OpinionViewer() {}

void OpinionViewer::performSearch()
{
	// 1. 保护性检查：只认历史库的 Model 和 搜索框
	if (!m_dbModel || !m_dbProxyModel || !ui.lineEditSearchDB) return;

	// 2. 拿到关键字
	QString text = ui.lineEditSearchDB->text().trimmed();

	// 3. 呼叫底层工人，去 SQLite 库里捞数据（底层已被净化，只搜内容）
	std::vector<OpinionRecord> dbResults = m_dbWorker.searchOpinions(text.toStdWString());

	// 4. 将真实数据转换成表格能认的格式
	QVector<MyTableModel::Row> searchRows;
	searchRows.reserve(dbResults.size());

	for (const auto& record : dbResults) {
		MyTableModel::Row row;
		row.cells.resize(4);
		row.cells[0] = QString::fromStdWString(record.content);
		row.cells[1] = QString::fromStdWString(record.reply);
		row.cells[2] = QString::fromStdWString(record.source);
		row.cells[3] = QString::fromStdWString(record.fileHash);
		searchRows.append(row);
	}

	// 5. 一把梭倒进历史库真实模型
	m_dbModel->setDataList(searchRows);

	// 🚀 6. 视口防线：强行把代理模型的过滤列死死锁在第 0 列
	m_dbProxyModel->setFilterKeyColumn(0);
	m_dbProxyModel->setFilterFixedString("");

	// 7. 动态更新搜索框提示文案
	if (text.isEmpty()) {
		ui.lineEditSearchDB->setPlaceholderText(QString("全库展现：当前共 %1 条意见，输入关键字极速过滤...").arg(m_dbWorker.getTotalCount()));
	}
	else {
		ui.lineEditSearchDB->setPlaceholderText(QString("全局检索完成：为您搜出 %1 条相关意见...").arg(searchRows.size()));
	}
}

void OpinionViewer::onBtnImportClicked()
{
	// 1. 锁定按钮，防止重复点击
	ui.btnImport->setEnabled(false);

	// 🚀 核心纠偏：现在数据全在沙盒代理模型里
	if (!m_importProxyModel || !m_importModel) {
		ui.btnImport->setEnabled(true);
		return;
	}

	// 🚀 核心纠偏：从沙盒中获取过滤/清洗后剩余的行数
	int rowCount = m_importProxyModel->rowCount();
	if (rowCount == 0) {
		QMessageBox::warning(this, "提示", "当前表格中没有有效意见，无法执行入库！");
		ui.btnImport->setEnabled(true);
		return;
	}

	// 2. 二次确认入库操作（文案完美保留）
	if (QMessageBox::question(this, "准备入库",
		QString("即将把当前的 %1 条意见沉淀至本地知识库。\n底层将自动执行指纹核对与增量去重，是否继续？").arg(rowCount),
		QMessageBox::Yes | QMessageBox::No) == QMessageBox::No) {
		ui.btnImport->setEnabled(true);
		return;
	}

	// 3. 呼叫辅助函数，剥离并清洗底层数据
	std::vector<OpinionRecord> recordsToInsert;
	std::vector<FileMetaData> metaDataToInsert;

	// 💡 提醒：你的 extractImportData 内部也要同步改为读取 m_importProxyModel
	if (!extractImportData(recordsToInsert, metaDataToInsert)) {
		QMessageBox::warning(this, "提示", "过滤后的有效入库数据为空，放弃入库！");
		ui.btnImport->setEnabled(true);
		return;
	}

	// 4. 调用底层 SQLite 工人执行入库
	m_dbWorker.beginTransaction();

	// 先写主表 (这些循环现在全部在内存中瞬间执行)
	for (const auto& meta : metaDataToInsert) {
		m_dbWorker.insertFileMetaData(meta);
	}

	// 再写从表 (把车上的意见货物挂靠上去)
	bool success = m_dbWorker.batchInsertOpinions(recordsToInsert);

	if (success) {
		// 🚀🚀🚀 统一落盘：几万条数据一次性刷入硬盘，瞬间膨胀完成！
		m_dbWorker.commitTransaction();

		QMessageBox::information(this, "入库成功", QString("共计 %1 条意见已顺利入库。").arg(recordsToInsert.size()));

		// ================= 🚀 v1.2 宇宙闭环逻辑 =================
		// 1. 清空沙盒加工区的数据，防止脏数据滞留
		m_importModel->setDataList(QVector<MyTableModel::Row>());
		if (ui.lineEditSearchImport) {
			ui.lineEditSearchImport->clear();
		}

		// 2. 功成身退：退掉橙色结界，自动丝滑切回主页历史库
		ui.stackedWidget->setCurrentIndex(0);

		// 3. 强制回读刷新：让主页历史库立马去加载刚写进去的最新真理数据
		refreshTableView();

		// 让重设库按钮亮起来
		ui.btnUpdateDb->show();
	}
	else {
		// 如果中间发生任何错误，直接撤销刚才的所有操作，保证数据库不被污染
		m_dbWorker.rollbackTransaction();
		QMessageBox::critical(this, "写入失败", "入库失败！请检查数据库文件状态。");
	}

	ui.btnImport->setEnabled(true);
}

bool OpinionViewer::extractImportData(std::vector<OpinionRecord>& outRecords, std::vector<FileMetaData>& outMetaData)
{
	// 🚀 核心防卫：确保沙盒模型存在
	if (!m_importProxyModel) return false;

	int rowCount = m_importProxyModel->rowCount();
	if (rowCount == 0) return false;

	outRecords.reserve(rowCount);
	std::unordered_set<std::wstring> seenHashes;

	for (int i = 0; i < rowCount; ++i) {
		// 🚀 核心纠偏：全部从 m_importProxyModel 提取数据
		QModelIndex indexContent = m_importProxyModel->index(i, 0);
		QModelIndex indexReply = m_importProxyModel->index(i, 1);
		QModelIndex indexSource = m_importProxyModel->index(i, 2);
		QModelIndex indexHash = m_importProxyModel->index(i, 3);

		OpinionRecord rec;
		rec.content = m_importProxyModel->data(indexContent, Qt::DisplayRole).toString().trimmed().toStdWString();
		rec.reply = m_importProxyModel->data(indexReply, Qt::DisplayRole).toString().trimmed().toStdWString();
		rec.source = m_importProxyModel->data(indexSource, Qt::DisplayRole).toString().trimmed().toStdWString();
		rec.fileHash = m_importProxyModel->data(indexHash, Qt::DisplayRole).toString().trimmed().toStdWString();

		// 跳过数据无效或缺失关联指纹的记录
		if (rec.content.empty() || rec.fileHash.empty()) continue;

		outRecords.push_back(rec);

		if (seenHashes.find(rec.fileHash) == seenHashes.end()) {
			seenHashes.insert(rec.fileHash);

			// 从扫描阶段生成的内存缓存中提取真实文件元数据
			auto it = m_currentScanMetaCache.find(rec.source);
			if (it != m_currentScanMetaCache.end()) {
				outMetaData.push_back(it->second);
			}
		}
	}

	// 如果清洗后仍有有效数据，返回 true
	return !outRecords.empty();
}

void OpinionViewer::onBtnExportClicked()
{
	// 🚀 1. 智能识别当前舞台，确定我们要压榨的数据源
	QSortFilterProxyModel* currentProxy = nullptr;
	int currentMode = ui.stackedWidget->currentIndex();

	if (currentMode == 0) {
		currentProxy = m_dbProxyModel;
	}
	else if (currentMode == 1) {
		currentProxy = m_importProxyModel;
	}

	// 安全防卫
	if (!currentProxy || currentProxy->rowCount() == 0) {
		QMessageBox::warning(this, "提示", "当前表格为空，没有可导出的数据！");
		return;
	}

	// 获取存储路径 (保持原汁原味)
	QString savePath = QFileDialog::getSaveFileName(this, "导出到 Word", "", "Word 文档 (*.doc)");
	if (savePath.isEmpty()) return;

	if (!savePath.toLower().endsWith(".doc")) {
		savePath += ".doc";
	}

	// 1. 组装表头
	std::vector<std::wstring> headers = { L"意见内容", L"意见回复", L"提取来源" };

	// 2. 🚀 从动态匹配的代理模型中榨取数据矩阵
	std::vector<std::vector<std::wstring>> tableData;
	int rowCount = currentProxy->rowCount();
	tableData.reserve(rowCount);

	for (int i = 0; i < rowCount; ++i) {
		std::vector<std::wstring> rowCells;
		// 精准抓取当前视图下的前 3 列有效文本
		rowCells.push_back(currentProxy->data(currentProxy->index(i, 0)).toString().toStdWString());
		rowCells.push_back(currentProxy->data(currentProxy->index(i, 1)).toString().toStdWString());
		rowCells.push_back(currentProxy->data(currentProxy->index(i, 2)).toString().toStdWString());
		tableData.push_back(rowCells);
	}

	// 3. 呼叫底层纯 C++ 引擎落地成盒！(保持原汁原味)
	bool success = OldDocParser::exportToWordDoc(savePath.toStdWString(), headers, tableData);

	if (success) {
		QMessageBox::information(this, "导出成功", "表格数据已成功导出为 Word 兼容文档！");
	}
	else {
		QMessageBox::critical(this, "导出失败", "底层引擎写入失败，请检查文件是否被占用！");
	}
}

void OpinionViewer::onBtnResetDatabaseClicked()
{
	// 1. 🚀 终极防误触拦截：高危操作，默认选 No
	if (QMessageBox::warning(this, "高危操作",
		"⚠️ 警告：即将彻底抹除本地知识库中的【所有】审查意见！\n\n此操作不可逆！是否确认清空重置？",
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No)
	{
		return;
	}

	if (!m_dbWorker.clearDatabase()) {
		QMessageBox::critical(this, "致命错误", "数据库清空失败！文件可能被占用，请重启软件后重试。");
		return;
	}

	// 清空历史库表格内容
	if (m_dbModel) {
		m_dbModel->setDataList(QVector<MyTableModel::Row>());
	}

	// 清空搜索框文字并重置提示
	if (ui.lineEditSearchDB) {
		ui.lineEditSearchDB->clear();
		ui.lineEditSearchDB->setPlaceholderText("全库展现：当前共 0 条意见，输入关键字极速过滤...");
	}

	// 既然库都炸平了，绝对不能允许用户按 Ctrl+Z 把刚才删掉的数据又塞回废墟里
	while (!m_undoStack.empty()) {
		m_undoStack.pop();
	}

	QMessageBox::information(this, "重置成功", "历史库已清空，所有数据已归零！\n现在您可以开始建立全新的知识库了。");
}

void OpinionViewer::dragEnterEvent(QDragEnterEvent* event) { if (event->mimeData()->hasUrls()) event->acceptProposedAction(); }

void OpinionViewer::dropEvent(QDropEvent* event) {
	QStringList droppedPaths;
	for (const QUrl& url : event->mimeData()->urls()) {
		if (url.isLocalFile()) droppedPaths.append(url.toLocalFile());
	}

	if (!droppedPaths.isEmpty()) {
		QString firstPath = droppedPaths.first();
		QFileInfo fi(firstPath);
		QString defaultDir = fi.isDir() ? firstPath : fi.absolutePath();

		updateDefaultScanPathInIni(defaultDir);
		startExtractionThread(collectFilesFromPaths(droppedPaths), firstPath);
	}
}

void OpinionViewer::onExtractionProgress(int current, int total, const QString& fileName, const QString& statusText)
{
	// 🚀 核心纠偏：更新主界面的搜索框状态
	if (ui.lineEditSearchDB) {
		ui.lineEditSearchDB->setPlaceholderText(QString("[%1] %2 (%3/%4)").arg(statusText).arg(fileName).arg(current).arg(total));
	}
}

void OpinionViewer::onExtractionFinished(int parsedCount, int skippedCount, int movedCount, double costSeconds)
{
	QString report = QString(
		"统计结果\n\n"
		"✔️ 全新解析文件：%1 个\n"
		"⏭️ 零秒跳过未改动：%2 个\n"
		"🔄 静默修正已移动：%3 个\n\n"
		"⏱️ 耗时：%4 秒"
	).arg(parsedCount).arg(skippedCount).arg(movedCount).arg(costSeconds, 0, 'f', 2);

	QMessageBox::information(this, "提取完成", report);
}

void OpinionViewer::refreshTableView()
{
	// 1. 恢复外围 UI 状态
	ui.lineEditPath->clear();
	ui.btnBrowse->setEnabled(true);

	// 🚀 2. 核心纠偏：清理主搜索框，并确保界面处于 Page 0
	ui.lineEditSearchDB->clear();
	if (ui.stackedWidget->currentIndex() != 0) {
		ui.stackedWidget->setCurrentIndex(0);
	}

	// 3. 触发底层全量查库并渲染
	performSearch();
}

void OpinionViewer::onSelectFiles() {
	QStringList files = QFileDialog::getOpenFileNames(this, "选择审查文件", "", "所有支持格式 (*.docx *.doc *.xlsx *.xls *.txt)");
	if (!files.isEmpty()) startExtractionThread(collectFilesFromPaths(files), files.first());
}

void OpinionViewer::onSelectFolder() {
	QString dir = QFileDialog::getExistingDirectory(this, "选择包含审查文档的文件夹");
	if (!dir.isEmpty()) startExtractionThread(collectFilesFromPaths(QStringList() << dir), dir);
}

std::vector<std::wstring> OpinionViewer::collectFilesFromPaths(const QList<QString>& paths) {
	std::vector<std::wstring> backendPaths;
	for (const QString& path : paths) {
		QFileInfo info(path);
		if (info.isDir()) {
			QDirIterator it(path, QStringList() << "*.docx" << "*.doc" << "*.xlsx" << "*.xls" << "*.txt", QDir::Files, QDirIterator::Subdirectories);
			while (it.hasNext()) backendPaths.push_back(it.next().toStdWString());
		}
		else if (info.isFile()) {
			backendPaths.push_back(path.toStdWString());
		}
	}
	return backendPaths;
}

void OpinionViewer::startExtractionThread(const std::vector<std::wstring>& backendPaths, const QString& displayPath, bool isOverwriteMode)
{
	// 🚀 彻底干掉黑名单清空，直接初始化元数据缓存
	m_currentScanMetaCache.clear();

	if (backendPaths.empty()) {
		QMessageBox::warning(this, "提示", "未能识别到任何有效的审查文档！");
		return;
	}

	ui.lineEditPath->setText(displayPath + (backendPaths.size() > 1 ? " (等多目标)" : ""));
	ui.btnBrowse->setEnabled(false);

	// 1. 初始化进度条对话框 (保持原汁原味)
	QProgressDialog* progressDlg = new QProgressDialog("正在启动指纹核对引擎...", "取消", 0, static_cast<int>(backendPaths.size()), this);
	progressDlg->setWindowTitle(isOverwriteMode ? "强制全量重设中..." : "审查意见提取中...");
	progressDlg->setWindowModality(Qt::WindowModal);
	progressDlg->setMinimumDuration(0);
	progressDlg->setValue(0);
	progressDlg->setAutoClose(false);
	progressDlg->setAutoReset(false);
	progressDlg->setStyleSheet(qApp->styleSheet());

	auto isCanceled = std::make_shared<std::atomic<bool>>(false);
	connect(progressDlg, &QProgressDialog::canceled, this, [isCanceled]() { isCanceled->store(true); });

	connect(this, &OpinionViewer::sigUpdateProgress, progressDlg, [progressDlg](int current, int total, const QString& fileName, const QString& statusText) {
		if (progressDlg->wasCanceled()) return;
		if (current == total) {
			progressDlg->setRange(0, 0); progressDlg->setLabelText("指纹核对完毕，正在装配数据队列，请稍候...");
		}
		else {
			progressDlg->setLabelText(QString("%1\n文件: %2\n进度: %3 / %4").arg(statusText).arg(fileName).arg(current).arg(total));
			progressDlg->setValue(current);
		}
		});

	// 2. 配置异步监听与计时器
	QElapsedTimer* stopWatch = new QElapsedTimer();
	stopWatch->start();
	auto* watcher = new QFutureWatcher<ExtractionResult>(this);

	// 3. 挂载线程完成后的 UI 收尾工作 (高潮降临)
	connect(watcher, &QFutureWatcher<ExtractionResult>::finished, this, [this, watcher, progressDlg, stopWatch, isCanceled]() {
		bool userCanceled = isCanceled->load();
		progressDlg->close();
		progressDlg->deleteLater();

		if (userCanceled) {
			delete stopWatch;
			watcher->deleteLater();
			ui.btnBrowse->setEnabled(true);
			return;
		}

		ExtractionResult extResult = watcher->result();

		// 同步内存元数据，供后续入库使用
		m_currentScanMetaCache = extResult.metaCache;

		// 🚀 计算耗时
		double costSeconds = stopWatch->elapsed() / 1000.0;
		delete stopWatch;

		// 🚀 核心纠偏：装载到专属沙盒预览表格 (4列架构)
		QVector<MyTableModel::Row> previewRows;
		previewRows.reserve(extResult.newRecords.size());

		for (const auto& record : extResult.newRecords) {
			MyTableModel::Row row;
			row.cells.resize(4);
			row.cells[0] = QString::fromStdWString(record.content);
			row.cells[1] = QString::fromStdWString(record.reply);
			row.cells[2] = QString::fromStdWString(record.source);
			row.cells[3] = QString::fromStdWString(record.fileHash);
			previewRows.append(row);
		}

		// 直接灌入沙盒加工模型，并确保隐藏哈希列
		if (m_importModel) {
			m_importModel->setDataList(previewRows);
		}

		// 全屏切换到带有橙色呼吸边框的【沙盒加工 Page】
		if (ui.stackedWidget) {
			ui.stackedWidget->setCurrentIndex(1);
		}

		// 如果这时候输入框里有旧文字，清空它，防止沙盒表一进来就是错的
		if (ui.lineEditSearchImport) {
			ui.lineEditSearchImport->clear();
		}

		ui.btnBrowse->setEnabled(true);
		watcher->deleteLater();

		onExtractionFinished(extResult.parsedCount, extResult.skippedCount, extResult.movedCount, costSeconds);

		});

	// 4. 启动后台线程，呼叫纯粹运算函数 (保持原状)
	QFuture<ExtractionResult> future = QtConcurrent::run(
		&OpinionViewer::executeScanAndParse,
		this,
		backendPaths,
		isOverwriteMode,
		isCanceled
	);

	watcher->setFuture(future);
}

void OpinionViewer::updateDefaultScanPathInIni(const QString& newPath)
{
	// 1. 读文件：把所有行原封不动灌进 QStringList
	QFile file("setting.ini");
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return;
	}

	QStringList lines;
	QTextStream in(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	in.setCodec("UTF-8"); // Qt5 兼容
#else
	in.setEncoding(QStringConverter::Utf8); // Qt6 标准
#endif

	while (!in.atEnd()) {
		lines.append(in.readLine());
	}
	file.close();

	// 2. 写文件：精准狙击目标行，其余行原样吐回
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return;
	}

	QTextStream out(&file);
	out.setEncoding(QStringConverter::Utf8);
	out.setGenerateByteOrderMark(true); // 🚀 核心防线：写入 UTF-8 BOM 头，捍卫底层纯 C++ 引擎的尊严

	for (const QString& line : lines) {
		// 过滤掉首尾空白后判断，防止前面有空格干扰
		if (line.trimmed().startsWith("DefaultScanPath=")) {
			out << "DefaultScanPath=" << newPath << "\n";
		}
		else {
			out << line << "\n";
		}
	}
	file.close();
}

void OpinionViewer::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_Escape) {
		// 🚀 核心改造：动态判断当前在哪个模式，清空对应的搜索框
		if (ui.stackedWidget->currentIndex() == 0) {
			if (!ui.lineEditSearchDB->text().isEmpty()) {
				ui.lineEditSearchDB->clear();
			}
		}
		else if (ui.stackedWidget->currentIndex() == 1) {
			if (!ui.lineEditSearchImport->text().isEmpty()) {
				ui.lineEditSearchImport->clear();
			}
		}

		event->accept();
		return;
	}

	baseWindow::keyPressEvent(event);
}

ExtractionResult OpinionViewer::executeScanAndParse(const std::vector<std::wstring>& backendPaths, bool isOverwriteMode, std::shared_ptr<std::atomic<bool>> isCanceled)
{
	ExtractionResult result;
	std::vector<std::wstring> docxGroup, txtGroup, docGroup, excelGroup;
	std::unordered_map<std::wstring, std::wstring> path2HashCache;

	DbWorker localThreadDbWorker; // 子线程专属的工人
	localThreadDbWorker.initDatabase(QCoreApplication::applicationDirPath().toStdWString() + L"/opinions.db");

	int total = static_cast<int>(backendPaths.size());
	int current = 0;

	// --- 阶段 1：增量预检（极速过滤层） ---
	for (const auto& wPath : backendPaths) {
		if (isCanceled->load()) break;

		QString qPath = QString::fromStdWString(wPath);
		long long size = FileUtil::getFileSize(qPath);
		long long modTime = FileUtil::getLastModifiedTime(qPath).toSecsSinceEpoch();

		QString shortName = QFileInfo(qPath).fileName();
		std::wstring wShortName = shortName.toStdWString();

		emit sigUpdateProgress(current, total, qPath, "⚡ 正在核对文件时间戳...");

		// 第一道防线：只看大小和时间。匹配直接跳过，耗时 0 毫秒
		if (!isOverwriteMode && localThreadDbWorker.isFileUnchanged(wShortName, size, modTime)) {
			result.skippedCount++;
			current++;
			continue;
		}

		emit sigUpdateProgress(current, total, qPath, "🔍 正在核对底层 MD5 指纹...");
		std::wstring wHash = FileUtil::calculateFileHash(qPath).toStdWString();

		// 第二道防线：哈希存在，说明换了目录或改了名
		if (!isOverwriteMode && localThreadDbWorker.isHashExists(wHash)) {
			localThreadDbWorker.updateFilePath(wHash, wShortName);
			result.movedCount++;
			current++;
			continue;
		}

		// 防御击穿：确认为需要解析的野生文件
		result.parsedCount++;

		// 存入缓存供解析和入库使用
		FileMetaData meta{ wHash, wShortName, size, modTime };
		result.metaCache[wShortName] = meta;
		path2HashCache[wShortName] = wHash;

		QString ext = QFileInfo(qPath).suffix().toLower();
		if (ext == "docx") docxGroup.push_back(wPath);
		else if (ext == "txt") txtGroup.push_back(wPath);
		else if (ext == "doc") docGroup.push_back(wPath);
		else if (ext == "xlsx" || ext == "xls") excelGroup.push_back(wPath);

		current++;
	}

	// --- 阶段 2：重火炮解析层 ---
	int parseCurrent = 0;
	auto processParsedDocs = [&](const std::vector<ParsedDoc>& docs) {
		auto cleanedDocs = OpinionCleaner::cleanBatch(docs);
		for (const auto& doc : cleanedDocs) {
			std::wstring docHash = path2HashCache[doc.fileName];
			for (const auto& line : doc.lines) {
				OpinionRecord rec;
				rec.content = line;
				rec.source = doc.fileName;
				rec.fileHash = docHash;
				result.newRecords.push_back(rec);
			}
		}
		};

	auto progressCb = [&](int subCur, int subTotal, const std::wstring& fname) {
		if (!isCanceled->load()) {
			emit sigUpdateProgress(result.skippedCount + result.movedCount + parseCurrent + subCur, total, QString::fromStdWString(fname), "正在解析文档内容...");
		}
		};

	if (!isCanceled->load() && !docxGroup.empty()) { processParsedDocs(DocxParser::extractTextBatch(docxGroup, progressCb)); parseCurrent += static_cast<int>(docxGroup.size()); }
	if (!isCanceled->load() && !txtGroup.empty()) { processParsedDocs(TxtParser::extractTextBatch(txtGroup, progressCb));  parseCurrent += static_cast<int>(txtGroup.size()); }
	if (!isCanceled->load() && !docGroup.empty()) { processParsedDocs(OldDocParser::extractTextBatch(docGroup, progressCb));  parseCurrent += static_cast<int>(docGroup.size()); }
	if (!isCanceled->load() && !excelGroup.empty()) { processParsedDocs(ExcelParser::extractTextBatch(excelGroup, progressCb)); }

	return result;
}

void OpinionViewer::onUndoTriggered()
{
	if (m_undoStack.empty()) return; // 栈里没东西，静默返回

	// 1. 拿取最近一次被删除的数据，并弹栈
	UndoAction action = m_undoStack.top();
	m_undoStack.pop();

	if (action.records.empty()) return;

	if (action.mode == 0) {
		// ================= 历史库：尸骨还魂 =================
		m_dbWorker.beginTransaction();
		if (m_dbWorker.batchInsertOpinions(action.records)) {
			m_dbWorker.commitTransaction();
		}
		else {
			m_dbWorker.rollbackTransaction();
		}

		// 只有当前页面留在历史库时，才刷新 UI，防止惊动沙盒
		if (ui.stackedWidget->currentIndex() == 0) {
			performSearch();
		}
	}
	else if (action.mode == 1) {
		// ================= 沙盒区：回滚内存 =================
		if (m_importModel) {
			QVector<MyTableModel::Row> restoredRows;
			restoredRows.reserve(action.records.size());

			for (const auto& rec : action.records) {
				MyTableModel::Row row;
				row.cells << QString::fromStdWString(rec.content)
					<< QString::fromStdWString(rec.reply)
					<< QString::fromStdWString(rec.source)
					<< QString::fromStdWString(rec.fileHash);
				restoredRows.append(row);
			}

			// 直接插回表格顶部
			m_importModel->prependRows(restoredRows);
		}
	}
}