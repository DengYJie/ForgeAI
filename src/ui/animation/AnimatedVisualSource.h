#pragma once

#include <FluentQt/Foundation.h>
#include <QEasingCurve>
#include <QPainter>

namespace ui::animation {
    enum class IconState {
        Normal,
        PointerOver,
        Pressed,
        Disabled
    };

    /**
     * @brief 动画视觉源抽象接口
     *
     * 负责根据动画进度 (0.0~1.0) 以及前置和目标状态绘制矢量动画图形。
     */
    class AnimatedVisualSource {
    public:
        virtual ~AnimatedVisualSource() = default;

        /**
         * @brief 获取状态切换的过渡动画时长
         * @param from 起始状态
         * @param to 目标状态
         * @param anim 动画配置令牌
         * @return 动画持续时间（毫秒）
         */
        virtual int duration(IconState from, IconState to, const fluent::FluentElement::Animation &anim) const {
            return 150;
        }

        /**
         * @brief 获取状态切换的缓动曲线
         * @param from 起始状态
         * @param to 目标状态
         * @param anim 动画配置令牌
         * @return 缓动曲线
         */
        virtual QEasingCurve easing(IconState from, IconState to, const fluent::FluentElement::Animation &anim) const {
            return QEasingCurve::OutCubic;
        }

        /**
         * @brief 绘制当前进度的矢量图形
         * @param painter QPainter 画笔
         * @param rect 图标绘制区域
         * @param from 起始状态
         * @param to 目标状态
         * @param progress 动画进度 [0.0, 1.0]
         * @param colors 主题色调令牌
         * @param isEnabled 组件是否启用
         */
        virtual void paint(QPainter &painter, const QRectF &rect,
                           IconState from, IconState to, qreal progress,
                           const fluent::FluentElement::Colors &colors,
                           bool isEnabled) = 0;
    };
} // namespace ui::animation
