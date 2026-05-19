#pragma once
#include <QStyledItemDelegate>
#include <QPainter>
#include <QAbstractItemView>


class HighlightDelegate : public QStyledItemDelegate
{
	Q_OBJECT
public:
	explicit HighlightDelegate(QObject* parent = nullptr)
		: QStyledItemDelegate(parent),
		m_hoverRow(-1)
	{}

	void setHoverRow(int row)
	{
		if (m_hoverRow != row) {
			m_hoverRow = row;
			if (auto* view = qobject_cast<QAbstractItemView*>(parent())) {
				view->viewport()->update();
			}
		}
	}

	void setTheme(bool isDark)
	{
		if (isDark) {
			// ... Dark Mode 保持不变
			m_hoverColor = QColor("#37414F");
			m_selectionColor = QColor("#346792");
		}
		else {
			// Light Mode: 
			// 悬停色：使用 QTreeView::item:!selected:hover 的 #E0E0E0 (浅灰)
			m_hoverColor = QColor("#e8f0f8");

			// 选中色：使用 selection-background-color 的 #A7C7E7 (柔和蓝)
			m_selectionColor = QColor("#A7C7E7");
		}

		if (auto* view = qobject_cast<QAbstractItemView*>(parent())) {
			view->viewport()->update();
		}
	}

protected:
	void paint(QPainter* painter, const QStyleOptionViewItem& option,
		const QModelIndex& index) const override
	{
		QStyleOptionViewItem opt = option;
		initStyleOption(&opt, index);

		painter->save();

		// 整行 hover（无论是否选中）
		if (index.row() == m_hoverRow) {
			painter->fillRect(opt.rect, m_hoverColor);
		}
		// 整行选中（仅在非 hover 时）
		else if (opt.state & QStyle::State_Selected) {
			painter->fillRect(opt.rect, m_selectionColor);
		}

		// 调用默认绘制文本、图标等（保留 QSS 样式）
		QStyledItemDelegate::paint(painter, opt, index);

		painter->restore();
	}

private:
	int m_hoverRow;
	QColor m_hoverColor;
	QColor m_selectionColor;
};