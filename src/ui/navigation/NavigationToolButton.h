#pragma once

#include "ui/navigation/NavigationPushButton.h"
#include <QPoint>
#include <QString>

class QVariantAnimation;
class QMoveEvent;
class QShowEvent;

namespace ui::navigation {
    /**
 * @brief 导航纯图标工具按钮
 *
 * 专用于顶部汉堡与返回等动作按钮，尺寸固定（48x48），图标始终几何居中。
 * 支持在导航栏排版变化时启用平滑位移动画（setAnimatedMove）。
 */
    class NavigationToolButton : public NavigationPushButton {
        Q_OBJECT

    public:
        explicit NavigationToolButton(const QString &iconGlyph, QWidget *parent = nullptr);

        ~NavigationToolButton() override = default;

        bool isSelectable() const { return false; }

        void setCompacted(bool compacted) override;

        /// 方向切换后保持 48x48 固定尺寸（基类按行高 setFixedHeight 会破坏锁定尺寸）
        void setOrientation(Qt::Orientation orientation) override;

        /// 开启后，布局重排导致的本控件位移会以隐式动画平滑过渡（对齐 WinUI overflow 按钮 200ms 规范）
        void setAnimatedMove(bool enabled);

        bool isAnimatedMove() const { return m_animatedMove; }

    protected:
        int iconDrawX() const override;

        void showEvent(QShowEvent *event) override;

        void moveEvent(QMoveEvent *event) override;

    private:
        /// 布局稳定后执行一次动画过渡，避免一次重排的多次 move 各自起动画
        void runPendingMove();

    private:
        bool m_animatedMove = false;
        /// 标记是否是从隐藏状态刚恢复显示
        bool m_justShown = false;
        /// 标记当前 moveEvent 由动画手动 move 触发（同步设/清，无残留）
        bool m_animating = false;
        /// 布局重排是否已登记延迟动画
        bool m_moveScheduled = false;
        /// 布局重排前的稳定位置，作为动画起点
        QPoint m_pendingFrom;
        /// 布局最终目标位置（多次 move 取最后一次）
        QPoint m_pendingTarget;
        QVariantAnimation *m_moveAnimation = nullptr;
    };
} // namespace ui::navigation
