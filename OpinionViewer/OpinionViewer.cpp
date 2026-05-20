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
#include <QTimer>
#include <QMenu>
#include <QTextStream>
#include <windows.h>
#include <iostream>
#include <QStringList>

#include "../DocParser/DocxParser.h"
#include "../DocParser/OldDocParser.h"
#include "../DocParser/ExcelParser.h"
#include "../DocParser/TxtParser.h"
#include "../DocParser/OpinionCleaner.h"

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

	QString dbPath = QCoreApplication::applicationDirPath() + "/opinions.db";
	bool isDbFileExists = QFileInfo::exists(dbPath);
	m_dbWorker.initDatabase(dbPath.toStdWString());

	if (!isDbFileExists || m_dbWorker.getTotalCount() == 0) {
		ui.btnUpdateDb->hide(); // 也可以用 setVisible(false);
	}
	else {
		ui.btnUpdateDb->show();
	}

	// 视窗样式设定
	setAttribute(Qt::WA_TranslucentBackground);
	setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);

	QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect(this);
	shadowEffect->setOffset(0, 0);
	shadowEffect->setColor(QColor(0, 0, 0, 160));
	shadowEffect->setBlurRadius(10);
	ui.widgetBg->setGraphicsEffect(shadowEffect);

	ui.btnSettings->setIcon(createSettingsIcon());
	QMenu* settingsMenu = new QMenu(this);
	settingsMenu->setStyleSheet(
		"QMenu { background-color: #252526; color: #D4D4D4; border: 1px solid #3F3F46; padding: 4px; }"
		"QMenu::item { padding: 6px 20px; }"
		"QMenu::item:selected { background-color: #04395E; color: white; }"
	);

	QAction* actEditRules = new QAction("编辑过滤规则", this);
	QAction* actAbout = new QAction("关于...", this);
	settingsMenu->addAction(actEditRules);
	settingsMenu->addAction(actAbout);

	connect(ui.btnSettings, &QPushButton::clicked, this, [this, settingsMenu]() {
		settingsMenu->exec(ui.btnSettings->mapToGlobal(QPoint(0, ui.btnSettings->height())));
		});

	connect(actEditRules, &QAction::triggered, this, [this]() {
		QString iniPath = QCoreApplication::applicationDirPath() + "/setting.ini";
		if (!QFileInfo::exists(iniPath)) {
			// 贴心防护：如果没有配置文件，自动生成一个标准模板并打开
			QFile file(iniPath);
			if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
				file.write("[OpinionFormat]\nStripNumberingRegex=^\\\\s*([A-Za-z]+\\\\d+|\\\\d+)[\\\\.\\\\u3001\\\\:\\\\uff1a]\\\\s*(?!\\\\d)\n\n[CleanRules]\nExactBlacklist=合格|无\nPrefixBlacklist=\n");
				file.close();
			}
		}
		QDesktopServices::openUrl(QUrl::fromLocalFile(iniPath));
		});

	connect(actAbout, &QAction::triggered, this, [this]() {
		QMessageBox::about(this, "关于",
			"<h2>施工图审查意见管理工具 V1.0</h2>"
			"<p><b>Code by:    zaas</b></p>"
			"<p><b>Date   :20260520</b></p>"
			"<hr>"
			"<p>一款海量工程审查意见的高效剥离、知识库沉淀的工具。</p>"
		);
		});

	ui.btnSettings->raise(); ui.btnMin->raise(); ui.btnMax->raise(); ui.btnClose->raise();

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

	// 🚀 绑定全量更新按钮
	connect(ui.btnUpdateDb, &QPushButton::clicked, this, [this]() {
		if (!m_proxyModel || !m_model) return;
		int rowCount = m_proxyModel->rowCount();

		if (QMessageBox::question(this, "全量同步",
			QString("即将用当前表格中的 %1 条数据【全量覆盖】本地知识库。\n这将会抹除数据库中原本存在、但当前被你删除或过滤掉的数据，是否继续？").arg(rowCount)) == QMessageBox::No) {
			return;
		}

		std::vector<OpinionRecord> recordsToSync;
		recordsToSync.reserve(rowCount);

		for (int i = 0; i < rowCount; ++i) {
			QString content = m_proxyModel->data(m_proxyModel->index(i, 0)).toString().trimmed();
			QString source = m_proxyModel->data(m_proxyModel->index(i, 2)).toString().trimmed();
			if (!content.isEmpty()) {
				OpinionRecord record;
				record.content = content.toStdWString();
				record.source = source.toStdWString();
				recordsToSync.push_back(record);
			}
		}

		// 直接呼叫底层放核弹
		if (m_dbWorker.overwriteOpinions(recordsToSync)) {
			QMessageBox::information(this, "同步成功", QString("极速全量同步完成！库中现存 %1 条意见。").arg(recordsToSync.size()));
			ui.lineEditSearch->setPlaceholderText(QString("全库同步完成：当前库中共 %1 条意见...").arg(recordsToSync.size()));
		}
		else {
			QMessageBox::critical(this, "致命错误", "全量覆盖失败！请检查数据库状态！");
		}
		});

	// 表格与模型设定
	ui.tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	ui.tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui.tableView->setFont(QFont("Microsoft YaHei", 10));
	ui.tableView->setWordWrap(true);
	ui.tableView->setTextElideMode(Qt::ElideRight);
	ui.tableView->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);
	ui.tableView->verticalHeader()->setDefaultSectionSize(42);
	ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
	ui.tableView->horizontalHeader()->setStretchLastSection(true);

	m_model = new MyTableModel(this);
	m_model->setHeaders({ "意见内容", "意见回复", "提取来源" });

	m_proxyModel = new QSortFilterProxyModel(this);
	m_proxyModel->setSourceModel(m_model);
	m_proxyModel->setFilterKeyColumn(0);
	m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	ui.tableView->setModel(m_proxyModel);

	QHeaderView* pHeader = ui.tableView->horizontalHeader();
	pHeader->setStretchLastSection(false);
	pHeader->setSectionResizeMode(0, QHeaderView::Stretch);
	pHeader->setSectionResizeMode(1, QHeaderView::Interactive);
	pHeader->setSectionResizeMode(2, QHeaderView::Interactive);

	ui.tableView->setColumnWidth(1, 150); // 1列回复：只留小部分写备注
	ui.tableView->setColumnWidth(2, 120); // 2列来源：极限压缩，只够看个后缀和文件尾巴

	// 菜单与操作连接
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
	connect(ui.btnExport, &QPushButton::clicked, this, &OpinionViewer::onBtnExportClicked);

	m_searchTimer = new QTimer(this);
	m_searchTimer->setSingleShot(true);
	m_searchTimer->setInterval(300);
	connect(m_searchTimer, &QTimer::timeout, this, &OpinionViewer::performSearch);
	connect(ui.lineEditSearch, &QLineEdit::textChanged, this, [this]() { if (m_searchTimer) m_searchTimer->start(); });
	connect(ui.btnImport, &QPushButton::clicked, this, &OpinionViewer::onBtnImportClicked);

	QTimer::singleShot(50, this, &OpinionViewer::performSearch);
}

OpinionViewer::~OpinionViewer() {}

// =========================================================================================
// 🚀 检索与展示核心
// =========================================================================================
void OpinionViewer::performSearch()
{
	if (!m_proxyModel || !m_model || !ui.lineEditSearch) return;

	QString text = ui.lineEditSearch->text().trimmed();
	bool isPreviewMode = !ui.lineEditPath->text().isEmpty();

	if (isPreviewMode) {
		m_proxyModel->setFilterFixedString(text);
		int total = m_model->rowCount();
		int visible = m_proxyModel->rowCount();
		ui.lineEditSearch->setPlaceholderText(text.isEmpty() ? QString("智能提取完成：共洗出 %1 条意见，可输入关键字过滤...").arg(total) : QString("内存过滤中：当前匹配到 %1 / %2 条意见...").arg(visible).arg(total));
		//已彻底铲除 ui.tableView->resizeRowsToContents(); 
	}
	else {
		m_proxyModel->setFilterFixedString("");
		std::vector<OpinionRecord> dbResults = m_dbWorker.searchOpinions(text.toStdWString());

		QVector<MyTableModel::Row> searchRows;
		searchRows.reserve(dbResults.size());

		for (const auto& record : dbResults) {
			MyTableModel::Row row;
			// 🚀 装甲升级：强行锁死大小为 3，拒绝底层动态扩容带来的内存碎片爆炸！
			row.cells.resize(3);
			row.cells[0] = QString::fromStdWString(record.content);
			row.cells[1] = "";
			row.cells[2] = QString::fromStdWString(record.source);
			searchRows.append(row);
		}

		m_model->setDataList(searchRows);
		ui.lineEditSearch->setPlaceholderText(text.isEmpty() ? QString("全库展现：当前共 %1 条意见，输入关键字闪电检索...").arg(searchRows.size()) : QString("全局检索完成：为您搜出 %1 条相关意见...").arg(searchRows.size()));
	}
}

// =========================================================================================
// 🚀 入库逻辑
// =========================================================================================
void OpinionViewer::onBtnImportClicked()
{
	// 🚀 彻底重构判定：不管路径框有没有字，只要当前前端表格里有数据，就说明有待入库的意见，直接放行！
	if (!m_proxyModel || !m_model) return;
	int rowCount = m_proxyModel->rowCount();

	if (rowCount == 0) {
		QMessageBox::warning(this, "提示", "当前表格中没有任何可见的有效意见，无法执行入库！");
		return;
	}

	// 2. 二次确认防手抖
	QMessageBox::StandardButton reply;
	reply = QMessageBox::question(this, "准备入库",
		QString("即将把当前的 %1 条意见（包含可能重复的历史意见）沉淀至本地知识库。\n底层将自动执行增量去重，是否继续？").arg(rowCount),
		QMessageBox::Yes | QMessageBox::No);
	if (reply == QMessageBox::No) return;

	// 3. 剥离数据准备输送给 DbWorker
	// 🚀 核心修正：用回我们清爽的 OpinionRecord 结构体，彻底抛弃 std::pair！
	std::vector<OpinionRecord> recordsToInsert;
	recordsToInsert.reserve(rowCount);

	for (int i = 0; i < rowCount; ++i) {
		// 确保从当前的代理视图中提取最新的、没被删除的干净数据
		QModelIndex proxyIndex0 = m_proxyModel->index(i, 0);
		QModelIndex proxyIndex2 = m_proxyModel->index(i, 2);

		QString content = m_proxyModel->data(proxyIndex0, Qt::DisplayRole).toString().trimmed();
		QString source = m_proxyModel->data(proxyIndex2, Qt::DisplayRole).toString().trimmed();

		if (!content.isEmpty()) {
			OpinionRecord record;
			record.content = content.toStdWString();
			record.source = source.toStdWString();
			recordsToInsert.push_back(record);
		}
	}

	if (recordsToInsert.empty()) {
		QMessageBox::warning(this, "提示", "过滤后的有效入库数据为空，放弃入库！");
		return;
	}

	// 4. 调用底层 SQLite 工人执行盲录增量入库
	bool success = m_dbWorker.batchInsertOpinions(recordsToInsert);

	if (success) {
		QMessageBox::information(this, "入库成功", QString("%1 条意见已顺利通过去重并沉淀至本地知识库！").arg(recordsToInsert.size()));

		// 🚀 状态重置：清空路径框，平滑切换回历史库检索状态
		ui.lineEditPath->clear();
		ui.lineEditSearch->clear();
		ui.btnUpdateDb->show();
		performSearch();
	}
	else {
		QMessageBox::critical(this, "致命错误", "入库失败！请检查数据库文件是否被独占锁定！");
	}
}

void OpinionViewer::onBtnExportClicked()
{
	if (!m_proxyModel || m_proxyModel->rowCount() == 0) {
		QMessageBox::warning(this, "提示", "当前表格为空，没有可导出的数据！");
		return;
	}

	QString savePath = QFileDialog::getSaveFileName(this, "导出到 Word", "", "Word 文档 (*.doc)");
	if (savePath.isEmpty()) return;

	if (!savePath.toLower().endsWith(".doc")) {
		savePath += ".doc";
	}

	// 1. 组装表头
	std::vector<std::wstring> headers = { L"意见内容", L"意见回复", L"提取来源" };

	// 2. 榨取数据矩阵
	std::vector<std::vector<std::wstring>> tableData;
	int rowCount = m_proxyModel->rowCount();
	tableData.reserve(rowCount);

	for (int i = 0; i < rowCount; ++i) {
		std::vector<std::wstring> rowCells;
		rowCells.push_back(m_proxyModel->data(m_proxyModel->index(i, 0)).toString().toStdWString());
		rowCells.push_back(m_proxyModel->data(m_proxyModel->index(i, 1)).toString().toStdWString());
		rowCells.push_back(m_proxyModel->data(m_proxyModel->index(i, 2)).toString().toStdWString());
		tableData.push_back(rowCells);
	}

	// 3. 呼叫底层纯 C++ 引擎落地成盒！
	bool success = OldDocParser::exportToWordDoc(savePath.toStdWString(), headers, tableData);

	if (success) {
		QMessageBox::information(this, "导出成功", "表格数据已成功导出为 Word 兼容文档！");
	}
	else {
		QMessageBox::critical(this, "导出失败", "底层引擎写入失败，请检查文件是否被占用！");
	}
}

// =========================================================================================
// 🚀 多线程提交流水线
// =========================================================================================
void OpinionViewer::dragEnterEvent(QDragEnterEvent* event) { if (event->mimeData()->hasUrls()) event->acceptProposedAction(); }

void OpinionViewer::dropEvent(QDropEvent* event) {
	QStringList droppedPaths;
	for (const QUrl& url : event->mimeData()->urls()) {
		if (url.isLocalFile()) droppedPaths.append(url.toLocalFile());
	}

	if (!droppedPaths.isEmpty()) {
		// 🚀 【新增记忆装甲】：智能抓取默认扫描文件夹
		QString firstPath = droppedPaths.first();
		QFileInfo fi(firstPath);

		// 如果拖进来的是文件，取它所在的文件夹路径；如果是文件夹，直接用它
		QString defaultDir = fi.isDir() ? firstPath : fi.absolutePath();

		updateDefaultScanPathInIni(defaultDir);

		// 🚀 保持你原有的硬核多线程提取逻辑纹丝不动
		startExtractionThread(collectFilesFromPaths(droppedPaths), firstPath);
	}
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

void OpinionViewer::startExtractionThread(const std::vector<std::wstring>& backendPaths, const QString& displayPath)
{
	if (backendPaths.empty()) {
		QMessageBox::warning(this, "提示", "未能识别到任何有效的审查文档！");
		return;
	}

	ui.lineEditPath->setText(displayPath + (backendPaths.size() > 1 ? " (等多目标)" : ""));
	ui.btnBrowse->setEnabled(false);

	QProgressDialog* progressDlg = new QProgressDialog("正在启动底层全能解析引擎...", "取消", 0, static_cast<int>(backendPaths.size()), this);
	progressDlg->setWindowTitle("审查意见提取中，请耐心等待");
	progressDlg->setWindowModality(Qt::WindowModal);
	progressDlg->setMinimumDuration(0);
	progressDlg->setValue(0);
	progressDlg->setAutoClose(false);
	progressDlg->setAutoReset(false);
	progressDlg->setStyleSheet(qApp->styleSheet());

	auto isCanceled = std::make_shared<std::atomic<bool>>(false);
	connect(progressDlg, &QProgressDialog::canceled, this, [isCanceled]() { isCanceled->store(true); });

	connect(this, &OpinionViewer::sigUpdateProgress, progressDlg, [progressDlg](int current, int total, const QString& fileName) {
		if (progressDlg->wasCanceled()) return;
		if (current == total) {
			progressDlg->setRange(0, 0); progressDlg->setLabelText("文档剥离完毕，正在进行数据装配，请稍候...");
		}
		else {
			progressDlg->setLabelText(QString("正在解析: %1\n进度: %2 / %3").arg(fileName).arg(current).arg(total));
			progressDlg->setValue(current);
		}
		});

	QElapsedTimer* stopWatch = new QElapsedTimer(); stopWatch->start();
	auto* watcher = new QFutureWatcher<std::vector<OpinionRecord>>(this);

	connect(watcher, &QFutureWatcher<std::vector<OpinionRecord>>::finished, this, [this, watcher, progressDlg, stopWatch, isCanceled]() {
		bool userCanceled = isCanceled->load();

		progressDlg->close();
		progressDlg->deleteLater();

		if (userCanceled) { delete stopWatch; watcher->deleteLater(); ui.btnBrowse->setEnabled(true); return; }

		auto cleanedData = watcher->result();
		QVector<MyTableModel::Row> previewRows;
		previewRows.reserve(cleanedData.size());

		for (const auto& record : cleanedData) {
			MyTableModel::Row row;
			row.cells.append(QString::fromStdWString(record.content));
			row.cells.append("");
			row.cells.append(QString::fromStdWString(record.source));
			previewRows.append(row);
		}
		m_model->setDataList(previewRows);

		// 彻底删除了 ui.tableView->resizeRowsToContents();

		ui.btnBrowse->setEnabled(true);
		performSearch();

		qint64 elapsedMs = stopWatch->elapsed();
		delete stopWatch;
		QString timeCostStr = QString("%1 秒 %2 毫秒").arg((elapsedMs / 1000) % 60).arg(elapsedMs % 1000);
		QMessageBox::information(this, "提取战报", QString("解析装配完毕！\n提取总条数: %1 条\n耗时: %2").arg(cleanedData.size()).arg(timeCostStr));
		watcher->deleteLater();
		});


	QFuture<std::vector<OpinionRecord>> future = QtConcurrent::run([this, backendPaths, isCanceled]() {
		std::vector<OpinionRecord> finalResults;

		std::vector<std::wstring> docxGroup, txtGroup, docGroup, excelGroup;
		for (const auto& wPath : backendPaths) {
			QString ext = QFileInfo(QString::fromStdWString(wPath)).suffix().toLower();
			if (ext == "docx") docxGroup.push_back(wPath);
			else if (ext == "txt") txtGroup.push_back(wPath);
			else if (ext == "doc") docGroup.push_back(wPath);
			else if (ext == "xlsx" || ext == "xls") excelGroup.push_back(wPath);
		}

		int total = static_cast<int>(backendPaths.size());
		int current = 0;

		auto processParsedDocs = [&](const std::vector<ParsedDoc>& docs) {
			auto cleanedDocs = OpinionCleaner::cleanBatch(docs); // 一波全洗完

			for (const auto& doc : cleanedDocs) {
				for (const auto& line : doc.lines) {
					OpinionRecord rec;
					rec.content = line;
					rec.source = doc.fileName;
					finalResults.push_back(rec);
				}
			}
			};

		auto progressCb = [&](int subCur, int subTotal, const std::wstring& fname) {
			if (!isCanceled->load()) emit sigUpdateProgress(current + subCur, total, QString::fromStdWString(fname));
			};

		if (!isCanceled->load() && !docxGroup.empty()) { processParsedDocs(DocxParser::extractTextBatch(docxGroup, progressCb)); current += static_cast<int>(docxGroup.size()); }
		if (!isCanceled->load() && !txtGroup.empty()) { processParsedDocs(TxtParser::extractTextBatch(txtGroup, progressCb)); current += static_cast<int>(txtGroup.size()); }
		if (!isCanceled->load() && !docGroup.empty()) { processParsedDocs(OldDocParser::extractTextBatch(docGroup, progressCb)); current += static_cast<int>(docGroup.size()); }
		if (!isCanceled->load() && !excelGroup.empty()) { processParsedDocs(ExcelParser::extractTextBatch(excelGroup, progressCb)); }

		return finalResults;
		});

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