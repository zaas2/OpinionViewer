#pragma once
#include <QWidget>
#include <QFrame>
#include <QVBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>

class HoverInfoWidget : public QWidget
{
	Q_OBJECT
public:
	explicit HoverInfoWidget(QWidget* parent = nullptr)
		: QWidget(parent, Qt::FramelessWindowHint | Qt::Window)
	{
		// 1. 外层完全透明，用于透出阴影
		setAttribute(Qt::WA_TranslucentBackground);
		setAttribute(Qt::WA_TransparentForMouseEvents);
		setAttribute(Qt::WA_ShowWithoutActivating);

		// 2. 布局留白给阴影
		QVBoxLayout* mainLayout = new QVBoxLayout(this);
		mainLayout->setContentsMargins(10, 10, 10, 10);
		mainLayout->setSpacing(0);

		// 3. 内容容器
		m_container = new QFrame(this);
		m_container->setObjectName("HoverContentFrame");

		// 【关键修复 1】确保容器本身不透明，使用主题背景色
		// 这样 Checkbox 之间的缝隙会有颜色，不会透到桌面
		m_container->setAutoFillBackground(true);

		m_container->setStyleSheet(
			"#HoverContentFrame {"
			"    background-color: #2D2D30;"   /* 极暗灰背景，完美融入你的主 UI */
			"    border: 1px solid #3E3E42;"   /* 极其克制的边框 */
			"    border-radius: 4px;"
			"}"
			"#HoverContentFrame QCheckBox {"
			"    color: #E0E0E0;"              /* 字体变成柔和的浅灰白 */
			"    padding: 4px 6px;"            /* 稍微撑开一点，别太挤 */
			"    outline: none;"
			"}"
		);

		// 4. 阴影
		QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(this);
		shadow->setOffset(0, 2);
		shadow->setColor(QColor(0, 0, 0, 60));
		shadow->setBlurRadius(15);
		m_container->setGraphicsEffect(shadow);

		mainLayout->addWidget(m_container);

		// 5. 内部布局
		m_contentLayout = new QVBoxLayout(m_container);
		// 这里设置边距，让 Checkbox 不要紧贴着阴影框边缘
		m_contentLayout->setContentsMargins(6, 6, 6, 6);
		// 【关键修复 3】如果不想有空隙，设为0；或者因为上面修好了背景色，这里设为2也可以
		m_contentLayout->setSpacing(0);

		setWindowOpacity(0.0);
	}

	void setContent(const QList<QPair<QString, bool>>& items)
	{
		// 1. 先清空旧内容
		QLayoutItem* child;
		while ((child = m_contentLayout->takeAt(0)) != nullptr) {
			if (child->widget()) delete child->widget();
			delete child;
		}

		// 🚀 2. 【核心装甲】：给悬浮窗设定一个“最大物理物理宽度”（比如 500 像素）
		// 没有这个最大宽度，哪怕开启了 wordWrap，系统也不知道该在多宽的地方把文字折断！
		m_container->setMaximumWidth(500);

		for (const auto& pair : items)
		{
			// 创建一个水平容器，把 Checkbox 和文字拼在一起
			QWidget* itemWidget = new QWidget(m_container);
			QHBoxLayout* hLayout = new QHBoxLayout(itemWidget);
			hLayout->setContentsMargins(0, 0, 0, 0);
			hLayout->setSpacing(8); // 框和字的间距

			// 只有框，不要字
			QCheckBox* cb = new QCheckBox(itemWidget);
			cb->setChecked(pair.second);
			cb->setAttribute(Qt::WA_TransparentForMouseEvents);
			// 防止文字换行导致复选框被拉伸变胖，强制它靠上对齐
			cb->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			hLayout->setAlignment(cb, Qt::AlignTop);

			// 🚀 3. 【换行真神】：把文字交给 QLabel，并开启自动换行
			QLabel* textLabel = new QLabel(pair.first, itemWidget);
			textLabel->setWordWrap(true);
			textLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
			textLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);

			// 继承你之前的暗黑风文字颜色
			textLabel->setStyleSheet("color: #E0E0E0; line-height: 1.5;");

			hLayout->addWidget(cb);
			hLayout->addWidget(textLabel);

			m_contentLayout->addWidget(itemWidget);
		}

		m_container->adjustSize();
		adjustSize();
	}

	void showAt(const QPoint& pos)
	{
		move(pos - QPoint(10, 10));

		if (isVisible() && windowOpacity() > 0.9) return;

		show();

		QPropertyAnimation* anim = new QPropertyAnimation(this, "windowOpacity");
		anim->setDuration(200);
		anim->setStartValue(0.0);
		anim->setEndValue(1.0);
		anim->setEasingCurve(QEasingCurve::OutCubic);
		anim->start(QAbstractAnimation::DeleteWhenStopped);
	}

	void hideWidget()
	{
		if (!isVisible()) return;
		hide();
		setWindowOpacity(0.0);
	}

private:
	QFrame* m_container = nullptr;
	QVBoxLayout* m_contentLayout = nullptr;
};