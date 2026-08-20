#pragma once

#include "ui/navigation/NavigationPushButton.h"

#include <QAccessible>
#include <QString>

class QVariantAnimation;
class QMouseEvent;

namespace ui::navigation {
    class NavigationTreeWidget;

    /**
     * @brief 导航树条目
     *
     * 支持叶子路由项（路由切换与指示条定位）与分类项（手风琴展开与 Chevron 旋转动画）。
     */
    class NavigationTreeItem : public NavigationPushButton {
        Q_OBJECT
        Q_PROPERTY(float arrowAngle READ arrowAngle WRITE setArrowAngle)

    public:
        /**
         * @brief 导航项类型
         */
        enum class Kind {
            Leaf, // 可激活并切换页面的叶子路由项
            Category, // 仅用于组织和展开/收起子项的分类项
        };

        NavigationTreeItem(const QString &routeKey,
                           const QString &iconGlyph,
                           const QString &text,
                           const QString &tooltipText,
                           Kind kind,
                           int depth,
                           bool selectable = true,
                           QWidget *parent = nullptr,
                           std::shared_ptr<ui::animation::AnimatedVisualSource> visualSource = nullptr);

        ~NavigationTreeItem() override = default;

        QString tooltipText() const { return m_tooltipText; }

        void setCompacted(bool compacted) override;

        QString routeKey() const { return m_routeKey; }
        Kind kind() const { return m_kind; }

        void setKind(Kind kind) {
            if (m_kind != kind) {
                m_kind = kind;
                update();
            }
        }

        bool isCategory() const { return m_kind == Kind::Category; }

        /// 所属节点容器（flyout 中的叶子项为 nullptr）
        NavigationTreeWidget *treeParent() const { return m_treeParent; }
        void setTreeParent(NavigationTreeWidget *node) { m_treeParent = node; }

        QString accessibleItemName() const { return accessibleName(); }

        void setAccessibleItemName(const QString &name);

        QAccessible::Role accessibleRole() const;

        /**
         * @brief 设置分类展开状态
         * @param animated 是否启用箭头平滑旋转动画
         */
        void setExpanded(bool expanded, bool animated = true);

        void setSelected(bool selected) override;

        void setOrientation(Qt::Orientation orientation) override;

        float arrowAngle() const { return m_arrowAngle; }

        void setArrowAngle(float angle);

        /// 仅在指示条飞行到达目标后开启，避免飞行途中提前绘制常驻指示条
        void setShowIndicator(bool show);

        // 播放 chevron 旋转到 target 的动画；系统开启 reduced-motion 时直接跳到位
        void animateChevron(float target);

        QRectF indicatorRect() const override;

        signals:
        /// 主体/chevron/键盘左右键的统一点击源；chevronClicked 区分是否为 chevron 热区触发




        void itemClicked(const QString &routeKey, bool chevronClicked);

    protected:
        void paintEvent(QPaintEvent *event) override;

        void keyPressEvent(QKeyEvent *event) override;

        void mousePressEvent(QMouseEvent *event) override;

        void mouseReleaseEvent(QMouseEvent *event) override;

        float currentTextAlpha() const override;

        int iconDrawX() const override;

        int textRightOffset() const override;

        QSize sizeHint() const override;

        QRectF chevronRect() const;

        bool chevronVisible() const;

    private:
        QString m_tooltipText;

        void onClicked();

        void notifyAccessibleState(const QAccessible::State &state);

        void notifyAccessibleRoleChange();

    private:
        QString m_routeKey;
        Kind m_kind;
        NavigationTreeWidget *m_treeParent = nullptr;
        bool m_showIndicator = false;
        bool m_chevronPressActive = false;
        float m_arrowAngle = 0.0f;
        QVariantAnimation *m_rotateAnimation = nullptr;
    };
} // namespace ui::navigation
