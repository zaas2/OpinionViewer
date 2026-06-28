#pragma once
#include <QTableView>
#include <QHeaderView>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QMouseEvent>
#include <QLineEdit>
#include <QDebug>
#include <QRegularExpression>
#include <QFontMetrics>
#include <QAbstractTableModel>
#include <QMap>
#include <QScrollBar>
#include <QAbstractProxyModel>
#include <QVector>
#include <QTimer>
#include <QPainter>
#include <QStyledItemDelegate>
#include <algorithm>

// 假设你的这些自定义类在这些头文件里，保持不变
#include "HoverInfoWidget.h"
#include "HighlightDelegate.h"

class ActionLinkDelegate : public QStyledItemDelegate
{
public:
	ActionLinkDelegate(const QString& linkText, QObject* parent = nullptr)
		: QStyledItemDelegate(parent), m_linkText(linkText) {
	}

	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
	{
		QStyleOptionViewItem opt = option;
		initStyleOption(&opt, index);
		painter->save();

		// 1. 画选中状态的背景
		QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

		// 2. 设置超链接字体和颜色
		QFont font = opt.font;
		font.setUnderline(true);
		painter->setFont(font);
		painter->setPen(QColor(0, 102, 204));

		// 3. 画出固定的文字
		painter->drawText(opt.rect, Qt::AlignCenter, m_linkText);
		painter->restore();
	}

private:
	QString m_linkText;
};

// =========================================================================
// 1. MyTableModel: 数据模型
// =========================================================================
class MyTableModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	struct Row { QVector<QString> cells; };
	QVector<Row> m_rows;
	QStringList m_headers;

	explicit MyTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

	int rowCount(const QModelIndex& parent = QModelIndex()) const override { return m_rows.size(); }
	int columnCount(const QModelIndex& parent = QModelIndex()) const override { return m_headers.size(); }

	QVariant data(const QModelIndex& index, int role) const override
	{
		if (!index.isValid() || index.row() >= m_rows.size()) return {};
		if (role == Qt::DisplayRole || role == Qt::EditRole)
		{
			const auto& row = m_rows[index.row()];
			if (index.column() < row.cells.size()) return row.cells.value(index.column());
		}
		return {};
	}

	bool setData(const QModelIndex& index, const QVariant& value, int role) override
	{
		if (index.isValid() && (role == Qt::EditRole || role == Qt::DisplayRole))
		{
			int r = index.row();
			int c = index.column();
			if (r < 0 || r >= m_rows.size() || c < 0 || c >= m_headers.size()) return false;
			if (c >= m_rows[r].cells.size()) m_rows[r].cells.resize(m_headers.size());
			m_rows[r].cells[c] = value.toString();
			emit dataChanged(index, index, { role });
			return true;
		}
		return false;
	}

	Qt::ItemFlags flags(const QModelIndex& index) const override
	{
		if (!index.isValid()) return Qt::NoItemFlags;
		return QAbstractTableModel::flags(index) | Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	}

	QVariant headerData(int section, Qt::Orientation orientation, int role) const override
	{
		if (orientation == Qt::Horizontal && role == Qt::DisplayRole) return m_headers.value(section);
		return {};
	}

	bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override
	{
		if (row < 0 || count <= 0 || row + count > m_rows.size()) return false;
		beginRemoveRows(parent, row, row + count - 1);
		m_rows.remove(row, count);
		endRemoveRows();
		return true;
	}

	// --- 业务接口 ---
	inline QString getCell(int row, int col) const
	{
		if (row < 0 || row >= m_rows.size() || col < 0 || col >= m_headers.size()) return {};
		if (col >= m_rows[row].cells.size()) return {};
		return m_rows[row].cells[col];
	}

	void setHeaders(const QStringList& headers) { beginResetModel(); m_headers = headers; endResetModel(); }
	void setDataList(const QVector<Row>& rows) { beginResetModel(); m_rows = rows; endResetModel(); }
	void appendData(const QVector<Row>& rows) { appendRows(rows); }

	void prependRows(const QVector<Row>& newRows)
	{
		if (newRows.isEmpty()) return;
		beginInsertRows(QModelIndex(), 0, (int)newRows.size() - 1);
		m_rows = newRows + m_rows;
		endInsertRows();
	}

	void appendRows(const QVector<Row>& newRows)
	{
		if (newRows.isEmpty()) return;
		int start = (int)m_rows.size();
		beginInsertRows(QModelIndex(), start, start + (int)newRows.size() - 1);
		m_rows.append(newRows);
		endInsertRows();
	}

	void appendRow(const Row& newRow)
	{
		int start = m_rows.size();
		beginInsertRows(QModelIndex(), start, start);
		m_rows.append(newRow);
		endInsertRows();
	}

	void beginBulkUpdate() { emit layoutAboutToBeChanged(); }
	void endBulkUpdate() { emit layoutChanged(); }
};

// =========================================================================
// 2. MyTableView: 视图控件
// =========================================================================
class MyTableView : public QTableView
{
	Q_OBJECT
public:
	explicit MyTableView(QWidget* parent = nullptr) : QTableView(parent)
	{
		setAlternatingRowColors(true);
		setSelectionBehavior(QAbstractItemView::SelectRows);
		setSelectionMode(QAbstractItemView::ExtendedSelection);
		setFocusPolicy(Qt::StrongFocus);
		setMouseTracking(true);

		horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
		horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		horizontalHeader()->setStretchLastSection(true);
		verticalHeader()->setVisible(false);
		verticalHeader()->setDefaultSectionSize(24);
		verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

		auto* delegate = new HighlightDelegate(this);
		setItemDelegate(delegate);

		setContextMenuPolicy(Qt::CustomContextMenu);
		connect(this, &QTableView::customContextMenuRequested, this, &MyTableView::showContextMenu);

		m_hoverWidget = new HoverInfoWidget(this);
		viewport()->setMouseTracking(true);

		m_hoverTimer = new QTimer(this);
		m_hoverTimer->setSingleShot(true);
		m_hoverTimer->setInterval(400);

		connect(m_hoverTimer, &QTimer::timeout, this, [this]() {
			if (!m_currentHoverIndex.isValid()) return;

			QString cellText = m_currentHoverIndex.data().toString().trimmed();
			if (cellText.isEmpty()) return;

			QFontMetrics fm(this->font());
			int textWidth = fm.horizontalAdvance(cellText);
			int colWidth = this->columnWidth(m_currentHoverIndex.column());

			if (textWidth <= colWidth - 20) return;

			QList<QPair<QString, bool>> hoverData;
			hoverData.append(qMakePair(cellText, true));

			m_hoverWidget->setContent(hoverData);
			m_hoverWidget->showAt(QCursor::pos() + QPoint(15, 20));
			});

		connect(this, &QTableView::entered, this, [this](const QModelIndex& index) {
			m_currentHoverIndex = index;
			m_hoverWidget->hideWidget();
			m_hoverTimer->start();
			});
	}

	inline void setMultiSelectEnabled(bool enabled)
	{
		m_multiSelectEnabled = enabled;
		setSelectionMode(enabled ? QAbstractItemView::ExtendedSelection : QAbstractItemView::SingleSelection);
	}

	void setModel(QAbstractItemModel* model) override
	{
		QTableView::setModel(model);
		setSelectionMode(m_multiSelectEnabled ? QAbstractItemView::ExtendedSelection : QAbstractItemView::SingleSelection);

		if (this->selectionModel())
		{
			connect(this->selectionModel(), &QItemSelectionModel::currentRowChanged,
				this, [this](const QModelIndex& current)
				{
					if (current.isValid()) emit rowClicked(current.row());
				});
		}

		m_model = qobject_cast<MyTableModel*>(model);
		if (!m_model)
		{
			if (auto* proxy = qobject_cast<QAbstractProxyModel*>(model))
				m_model = qobject_cast<MyTableModel*>(proxy->sourceModel());
		}
	}

	inline void setFixedColumnWidth(int col, int width)
	{
		horizontalHeader()->setSectionResizeMode(col, QHeaderView::Fixed);
		horizontalHeader()->resizeSection(col, width);
	}

	inline void setCellHoverContent(int row, int col, const QList<QPair<QString, bool>>& items)
	{
		m_hoverContents[{row, col}] = items;
	}

	inline void onThemeChanged(bool isDark)
	{
		if (auto* delegate = qobject_cast<HighlightDelegate*>(itemDelegate()))
			delegate->setTheme(isDark);
	}

signals:
	void rowClicked(int row);
	void aboutToShowMenu(QMenu* menu, const QModelIndex& index);

protected:
	void showContextMenu(const QPoint& pos)
	{
		if (!model()) return;
		QModelIndex idx = indexAt(pos);
		if (!idx.isValid()) return;

		QMenu menu(this);
		menu.setFont(QFont("Microsoft YaHei", 9));

		int selectedCount = selectionModel()->selectedRows().count();

		// 1. 组装组件自带的通用数据菜单 (复制等)
		if (selectedCount <= 1)
		{
			QAction* actCopyCell = menu.addAction(tr("复制单元格"));
			connect(actCopyCell, &QAction::triggered, this, &MyTableView::copyCell);

			QAction* actCopyRow = menu.addAction(tr("复制整行"));
			connect(actCopyRow, &QAction::triggered, this, &MyTableView::copyRow);
		}
		else
		{
			QAction* actCopySelected = menu.addAction(tr("复制选中行 (%1)").arg(selectedCount));
			connect(actCopySelected, &QAction::triggered, this, &MyTableView::copySelectedRows);
		}

		menu.addAction(tr("复制整个表"), this, &MyTableView::copyAll);

		emit aboutToShowMenu(&menu, idx);
		menu.exec(viewport()->mapToGlobal(pos));
	}

	void mouseMoveEvent(QMouseEvent* event) override
	{
		QModelIndex idx = indexAt(event->pos());
		int hoverRow = idx.isValid() ? idx.row() : -1;
		if (hoverRow != m_lastHoveredRow)
		{
			m_lastHoveredRow = hoverRow;
			if (auto* d = qobject_cast<HighlightDelegate*>(itemDelegate()))
			{
				d->setHoverRow(m_lastHoveredRow);
				viewport()->update();
			}
		}
		QTableView::mouseMoveEvent(event);
	}

	void leaveEvent(QEvent* event) override
	{
		m_lastHoveredRow = -1;
		if (auto* d = qobject_cast<HighlightDelegate*>(itemDelegate())) {
			d->setHoverRow(-1);
		}
		viewport()->update();

		if (m_hoverWidget) {
			m_hoverWidget->hideWidget();
		}
		QTableView::leaveEvent(event);
	}

public:
	void copyCell()
	{
		QModelIndex idx = currentIndex();
		if (!idx.isValid()) return;
		QApplication::clipboard()->setText(idx.data(Qt::DisplayRole).toString());
	}

	void copyRow()
	{
		QModelIndex idx = currentIndex();
		if (!idx.isValid()) return;
		QStringList cells;
		for (int c = 0; c < model()->columnCount(); ++c)
			cells << model()->index(idx.row(), c).data(Qt::DisplayRole).toString();
		QApplication::clipboard()->setText(cells.join("\t"));
	}

	void copySelectedRows()
	{
		QModelIndexList selectedRows = selectionModel()->selectedRows();
		if (selectedRows.isEmpty()) return;
		std::sort(selectedRows.begin(), selectedRows.end());
		QString result;
		for (const QModelIndex& index : selectedRows)
		{
			QStringList line;
			for (int c = 0; c < model()->columnCount(); ++c)
				line << model()->index(index.row(), c).data(Qt::DisplayRole).toString();
			result += line.join("\t") + "\n";
		}
		QApplication::clipboard()->setText(result);
	}

	void copyAll()
	{
		if (!model()) return;

		QString result;
		int rowCount = model()->rowCount();
		int colCount = model()->columnCount();

		QStringList headerList;
		for (int c = 0; c < colCount; ++c)
		{
			headerList << model()->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
		}
		result += headerList.join("\t") + "\n";

		for (int r = 0; r < rowCount; ++r)
		{
			QStringList line;
			for (int c = 0; c < colCount; ++c)
			{
				line << model()->index(r, c).data(Qt::DisplayRole).toString();
			}
			result += line.join("\t") + "\n";
		}
		QApplication::clipboard()->setText(result);
	}

// 	// 🚀 【核心纠偏】绝对线程安全、防错位、原汁原味的删除逻辑
// 	void deleteSelectedRows()
// 	{
// 		if (!model()) return;
// 		QModelIndexList selectedRows = selectionModel()->selectedRows();
// 		if (selectedRows.isEmpty()) return;
// 
// 		QStringList deletedContents;
// 		QList<int> sourceRowsToDelete;
// 		auto* proxyModel = qobject_cast<QAbstractProxyModel*>(model());
// 
// 		// 1. 收集要删的文本，和它在【底层真实模型】中的物理行号
// 		for (const QModelIndex& index : selectedRows)
// 		{
// 			// ⚠️ 绝不能用 trimmed()！必须原汁原味提取，否则 SQLite 严格匹配会找不到数据！
// 			QModelIndex contentIdx = model()->index(index.row(), 0);
// 			QString contentText = model()->data(contentIdx, Qt::DisplayRole).toString();
// 
// 			if (!contentText.isEmpty()) {
// 				deletedContents << contentText;
// 			}
// 
// 			// 剥离代理层，拿到最底层的真实物理行号
// 			if (proxyModel) {
// 				QModelIndex sourceIndex = proxyModel->mapToSource(index);
// 				if (sourceIndex.isValid()) {
// 					sourceRowsToDelete.append(sourceIndex.row());
// 				}
// 			}
// 			else {
// 				sourceRowsToDelete.append(index.row());
// 			}
// 		}
// 
// 		// 2. 将底层真实行号【从大到小】排序！
// 		// 只有从底部往上删，才绝对不会导致上面的行号发生错乱塌陷！
// 		std::sort(sourceRowsToDelete.begin(), sourceRowsToDelete.end(), std::greater<int>());
// 
// 		// 3. 执行内存模型物理切除
// 		for (int r : sourceRowsToDelete) {
// 			if (m_model) {
// 				m_model->removeRow(r);
// 			}
// 		}
// 
// 		// 4. 发射信号，把原汁原味的文本交给 OpinionViewer 去执行 SQL 删库
// 		if (!deletedContents.isEmpty()) {
// 			emit sigRowsDeleted(deletedContents);
// 		}
// 	}

	QList<QStringList> executeDelete()
	{
		QList<QStringList> deletedRowsData;
		if (!model()) return deletedRowsData;
		QModelIndexList selectedRows = selectionModel()->selectedRows();
		if (selectedRows.isEmpty()) return deletedRowsData;

		QList<int> sourceRowsToDelete;
		auto* proxyModel = qobject_cast<QAbstractProxyModel*>(model());

		// 💡 动态获取当前模型的总列数，从此 MyTableView 彻底变成通用类！
		int colCount = model()->columnCount();

		for (const QModelIndex& index : selectedRows)
		{
			QStringList rowData;
			// 遍历动态列数
			for (int c = 0; c < colCount; ++c) {
				QModelIndex cellIdx = model()->index(index.row(), c);
				rowData << model()->data(cellIdx, Qt::DisplayRole).toString();
			}

			// 我们约定业务上第 0 列是核心内容，如果内容不为空，则记录这行
			if (!rowData.isEmpty() && !rowData[0].isEmpty()) {
				deletedRowsData.append(rowData);
			}

			if (proxyModel) {
				QModelIndex sourceIndex = proxyModel->mapToSource(index);
				if (sourceIndex.isValid()) sourceRowsToDelete.append(sourceIndex.row());
			}
			else {
				sourceRowsToDelete.append(index.row());
			}
		}

		std::sort(sourceRowsToDelete.begin(), sourceRowsToDelete.end(), std::greater<int>());

		for (int r : sourceRowsToDelete) {
			if (m_model) m_model->removeRow(r);
		}

		return deletedRowsData;
	}

private:
	MyTableModel* m_model = nullptr;
	HoverInfoWidget* m_hoverWidget = nullptr;
	int m_lastHoveredRow = -1;
	QPair<int, int> m_lastHoverCell = { -1, -1 };
	QMap<QPair<int, int>, QList<QPair<QString, bool>>> m_hoverContents;
	bool m_multiSelectEnabled = true;
	QString m_originalTextOnEdit;
	QTimer* m_hoverTimer = nullptr;
	QModelIndex m_currentHoverIndex;
};