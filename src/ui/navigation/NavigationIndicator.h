#pragma once

#include <QWidget>
#include <QRectF>

#include <FluentQt/Design.h>
#include <FluentQt/Foundation.h>

#include "NavigationMetrics.h"

class QVariantAnimation;

namespace ui::navigation {
    /**
 * @brief 导航选中指示胶囊（仅切换动画用）
 *
 * 作为独立悬浮部件，只在选中项切换瞬间从旧位置飞向新位置播放过渡动画，
 * 动画结束即隐藏；常驻指示条由 NavigationTreeItem 自身绘制。
 */
    class NavigationIndicator : public QWidget, public fluent::FluentElement {
        Q_OBJECT

    public:
        explicit NavigationIndicator(QWidget *parent = nullptr);

        ~NavigationIndicator() override = default;

        Qt::Orientation orientation() const { return m_orientation; }

        void setOrientation(Qt::Orientation orientation);

        void setInitialPosition(const QRectF &rect);

        /**
     * @brief 从当前位置飞向目标几何，动画结束后自动隐藏
     * @param targetRect 宿主坐标系下的目标矩形
     */
        void activateAt(const QRectF &targetRect, bool animated = true);

        /// 立即隐藏（停止动画）
        void hideIndicator();

        bool isFlying() const;

        /**
     * @brief Portal Return: 浮层关闭后，水平指示条从左向右展开恢复。
     * @param targetRect 宿主坐标系下的目标矩形（恢复后的最终位置）
     */
        void playPortalReturn(const QRectF &targetRect);

        /**
     * @brief Cross Window Portal: 跨窗口（Horizontal 栏到 Flyout 浮层）同步组合动效，形态按宿主方向分派。
     * @param startRect 宿主坐标系下的起始矩形（水平条收缩起点）
     * @param targetRect 宿主坐标系下的目标矩形（浮层垂直条滑入终点）
     *
     * Horizontal 宿主（顶栏）：水平条向左收缩消失（左固定、右边界左移、宽度衰减到 0）；
     * Vertical 宿主（浮层）：垂直条从顶部生成向下生长（顶部固定、高度从 0 增长）。
     */
        void playCrossWindowPortal(const QRectF &startRect, const QRectF &targetRect);

        QRectF currentRect() const { return m_currentRect; }

        signals:
        /// 飞行动画开始、悬浮指示条接管呈现时发出：宿主收到后清空常驻指示条所有权




        void flightStarted();

        /// 飞行动画结束、指示条已到达目标位置时发出：宿主收到后让对应 item 绘制常驻指示条
        void flightFinished();

    protected:
        void paintEvent(QPaintEvent *event) override;

        void showEvent(QShowEvent *event) override;

        void onThemeUpdated() override;

    private:
        /// 目标与当前几何几乎重合时短路，避免无谓动画
        bool isSamePosition(const QRectF &targetRect) const;

        /// 确保 Z 序在滚动区之上
        void raiseToTop();

        /// 配置飞行动画通用属性（缓动 + 0→1 + 时长），各入口传入自身缓动；durationMs 缺省时取主题标准时长
        void beginFlight(const QEasingCurve &easing, int durationMs = -1);

        /// 首次或失效时沿父链解析一次并缓存
        fluent::FluentElement::Theme cachedEffectiveTheme() const;

        /// 绕开 themeColorsRef() 每次沿父链遍历 effectiveTheme() 的开销
        const fluent::FluentElement::Colors &colorsRef() const;

    private:
        QRectF m_startRect;
        QRectF m_targetRect;
        QRectF m_currentRect;

        mutable fluent::FluentElement::Theme m_effectiveTheme = fluent::FluentElement::Theme::Light;
        mutable bool m_themeValid = false;

        QVariantAnimation *m_flightAnimation = nullptr;
        Qt::Orientation m_orientation = Qt::Vertical;

        /// 当前动画模式：Normal 标准飞行，PortalReturn 展开恢复，CrossWindowPortal 完全映射插值飞跃
        enum class AnimationMode { Normal, PortalReturn, CrossWindowPortal };

        AnimationMode m_animMode = AnimationMode::Normal;
        /// 本次飞行为同轴平移（同 x 或同 y），用 WinUI 分段缓动而非单一 decelerate
        bool m_sameAxisFlight = false;
    };
} // namespace ui::navigation
