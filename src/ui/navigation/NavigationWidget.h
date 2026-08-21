#pragma once

#include <QWidget>
#include <QColor>
#include <QMargins>
#include <QRectF>
#include <QVariantAnimation>
#include <FluentQt/Design.h>
#include <FluentQt/Foundation.h>

#include "ui/navigation/NavigationFocusHost.h"
#include "ui/navigation/NavigationMetrics.h"

namespace ui::navigation {
    /**
 * @brief 导航项基类
 *
 * 封装选中态、悬停过渡、无障碍语义、键盘焦点环与层级缩进，
 * 为所有导航条目提供统一的 Fluent 交互基线。
 */
    class NavigationWidget : public QWidget, public fluent::FluentElement {
        Q_OBJECT
        Q_PROPERTY(float hoverProgress READ hoverProgress WRITE setHoverProgress)

    public:
        explicit NavigationWidget(bool isSelectable = true, QWidget *parent = nullptr);

        ~NavigationWidget() override = default;

        /**
     * @brief 检测系统是否启用无障碍减弱动态效果
     * @return true 若用户在系统级设置中关闭了动画
     */
        static bool isReducedMotion();

        /**
     * @brief 全局输入模态感知：当前是否处于物理键盘导航模式
     */
        static bool isKeyboardMode();

        bool isCompacted() const { return m_orientation == Qt::Vertical && m_isCompacted; }

        virtual void setCompacted(bool compacted);

        bool isSelected() const { return m_isSelected; }

        virtual void setSelected(bool selected);

        bool isSelectable() const { return m_isSelectable; }
        void setSelectable(bool selectable) { m_isSelectable = selectable; }

        NavigationItemPosition itemPosition() const { return m_itemPosition; }
        void setItemPosition(NavigationItemPosition pos) { m_itemPosition = pos; }
        bool isFooterItem() const { return m_itemPosition == NavigationItemPosition::Bottom; }

        int nodeDepth() const { return m_nodeDepth; }

        void setNodeDepth(int depth);

        float hoverProgress() const { return m_hoverProgress; }

        void setHoverProgress(float progress);

        Qt::Orientation orientation() const { return m_orientation; }

        virtual void setOrientation(Qt::Orientation orientation);

        virtual QRectF indicatorRect() const;

        void click();

        /**
     * @brief 沿父链查找焦点宿主
     * @return 实现了 INavigationFocusHost 的宿主指针，用于向上转发方向键焦点移动
     */
        INavigationFocusHost *navigationFocusHost() const;

        void setAccessibleItemName(const QString &name);

        void onThemeUpdated() override;

    protected:
        /// 首次或失效时沿父链解析一次并缓存
        fluent::FluentElement::Theme cachedEffectiveTheme() const;

        /// 绕开 themeColorsRef() 每次沿父链遍历 effectiveTheme() 的开销；引用归 ThemeRegistry 所有，仅单次 paint() 内使用
        const fluent::FluentElement::Colors &colorsRef() const;

        signals:




        void clicked(bool triggerByUser);

    protected:
        void enterEvent(FluentEnterEvent *event) override;

        void leaveEvent(QEvent *event) override;

        void mousePressEvent(QMouseEvent *event) override;

        void mouseReleaseEvent(QMouseEvent *event) override;

        void keyPressEvent(QKeyEvent *event) override;

        void focusInEvent(QFocusEvent *event) override;

        void focusOutEvent(QFocusEvent *event) override;

        QColor currentBackgroundColor() const;

        virtual float currentTextAlpha() const;

        /// 绘制双层实线高对比度键盘焦点环
        void drawFocusVisual(QPainter &painter, const QRectF &rect) const;

    protected:
        bool m_isCompacted = false;
        bool m_isSelected = false;
        bool m_isPressed = false;
        bool m_isHovered = false;
        /// 分类项与操作按钮设为 false
        bool m_isSelectable = true;
        NavigationItemPosition m_itemPosition = NavigationItemPosition::Top;
        /// 用于阶梯缩进
        int m_nodeDepth = 0;

        float m_hoverProgress = 0.0f;
        Qt::Orientation m_orientation = Qt::Vertical;

        QVariantAnimation *m_hoverAnimation = nullptr;

        mutable fluent::FluentElement::Theme m_effectiveTheme = fluent::FluentElement::Theme::Light;
        /// 主题切换后失效
        mutable bool m_themeValid = false;
    };
} // namespace ui::navigation
