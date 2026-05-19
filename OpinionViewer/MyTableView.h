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
#include <algorithm>
#include "HoverInfoWidget.h"
#include "HighlightDelegate.h"

class QHexEdit;

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

		// 1. 画选中状态的背景 (保持 Qt 原生选中效果)
		QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

		// 2. 设置超链接字体和颜色
		QFont font = opt.font;
		font.setUnderline(true); // 加下划线
		painter->setFont(font);
		painter->setPen(QColor(0, 102, 204)); // 经典的超链接蓝色

		// 3. 画出固定的文字
		painter->drawText(opt.rect, Qt::AlignCenter, m_linkText);

		painter->restore();
	}

private:
	QString m_linkText;
};

// =========================================================================
// 1. MyTableModel: 数据模型 (补齐所有 getCell, appendRows 等接口)
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

    // --- 业务接口 (绝不删减) ---
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
// 2. MyTableView: 视图控件 (核心修复：setModel 时重新强制应用多选)
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
        
        // [关键] 必须设置为 StrongFocus，否则组合控件中键盘修饰键(Shift)可能失效
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

		// 🚀 1. 视口也必须开启鼠标追踪（QTableView 的特殊机制）
		viewport()->setMouseTracking(true);

		// 1. 初始化延迟定时器 (400毫秒延迟，极度舒适)
		m_hoverTimer = new QTimer(this);
		m_hoverTimer->setSingleShot(true);
		m_hoverTimer->setInterval(400);

		// 2. 定时器到了才干活
		connect(m_hoverTimer, &QTimer::timeout, this, [this]() {
			if (!m_currentHoverIndex.isValid()) return;

			QString cellText = m_currentHoverIndex.data().toString().trimmed();
			if (cellText.isEmpty()) return;

			// 🚀 核心逻辑：精准计算文字宽度！
			QFontMetrics fm(this->font());
			// 注意：Qt 5.11 以前用 fm.width(cellText)
			int textWidth = fm.horizontalAdvance(cellText);
			int colWidth = this->columnWidth(m_currentHoverIndex.column());

			// 如果字体的物理宽度还没有列宽大（减去20像素的边距容差），直接 return，短行坚决不弹！
			if (textWidth <= colWidth - 20) {
				return;
			}

			// 组装并弹出
			QList<QPair<QString, bool>> hoverData;
			hoverData.append(qMakePair(cellText, true));

			m_hoverWidget->setContent(hoverData);
			// 稍微往下偏移 20 像素，不要把原来的单元格完全挡死
			m_hoverWidget->showAt(QCursor::pos() + QPoint(15, 20));
			});

		// 3. 鼠标一动，重新计时
		connect(this, &QTableView::entered, this, [this](const QModelIndex& index) {
			m_currentHoverIndex = index;
			m_hoverWidget->hideWidget(); // 鼠标一换格子，马上把旧的藏起来
			m_hoverTimer->start();       // 重新开始 400ms 倒计时
			});
    }

    inline void setMultiSelectEnabled(bool enabled)
    {
        m_multiSelectEnabled = enabled;
        setSelectionMode(enabled ? QAbstractItemView::ExtendedSelection : QAbstractItemView::SingleSelection);
    }

    void setModel(QAbstractItemModel* model) override
    {
        // 1. 调用基类设置模型
        QTableView::setModel(model);

        // 2. [核心修复逻辑] 重新强制设置一遍 SelectionMode
        // 因为 QTableView::setModel 会导致内部 SelectionModel 被重置，可能冲掉之前的配置
        setSelectionMode(m_multiSelectEnabled ? QAbstractItemView::ExtendedSelection : QAbstractItemView::SingleSelection);

        // 3. 重新连接信号
        if (this->selectionModel())
        {
            connect(this->selectionModel(), &QItemSelectionModel::currentRowChanged, 
                this, [this](const QModelIndex& current) 
            {
                if (current.isValid()) emit rowClicked(current.row());
            });
        }

        // 4. 穿透 Proxy 获取源模型
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

        // 菜单分流逻辑
        if (selectedCount <= 1)
        {
            emit aboutToShowMenu(&menu, idx);
            if (!menu.isEmpty()) menu.addSeparator();

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

		// 新增：删除选中行（支持单选和多选）
		menu.addSeparator();
		QAction* actDelete = menu.addAction(tr("删除选中行"));
		connect(actDelete, &QAction::triggered, this, &MyTableView::deleteSelectedRows);

        menu.exec(viewport()->mapToGlobal(pos));
    }

    // 各种事件处理...
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
    // --- 复制接口实现 ---
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

		// 1. 获取表头 (Horizontal Header)
		QStringList headerList;
		for (int c = 0; c < colCount; ++c)
		{
			// 获取水平表头的显示文字
			headerList << model()->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
		}
		result += headerList.join("\t") + "\n";

		// 2. 获取行数据
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

	void deleteSelectedRows()
	{
		if (!model()) return;
		QModelIndexList selectedRows = selectionModel()->selectedRows();
		if (selectedRows.isEmpty()) return;

		// 必须按行号从大到小（从下往上）排序！
		// 如果从上往下删，删掉第 1 行后，原来的第 2 行变成了第 1 行，索引全盘错乱！
		std::sort(selectedRows.begin(), selectedRows.end(), [](const QModelIndex& a, const QModelIndex& b) {
			return a.row() > b.row();
			});

		for (const QModelIndex& index : selectedRows)
		{
			// 因为挂载了 QSortFilterProxyModel，调用代理的 removeRow 会自动映射到底层模型删除
			model()->removeRow(index.row());
		}
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