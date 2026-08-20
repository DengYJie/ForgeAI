#pragma once
#include "AnimatedVisualSource.h"
#include <QPainterPath>
#include <QtMath>
#include <cmath>

namespace ui::animation {
    /**
     * @brief 后退按钮专属矢量动画源
     * 对标 WinUI 3 官方 AnimatedBackVisualSource (Controls_03_Back)
     *
     * 1. 严格按照 WinUI 3 原生 48x48 矢量比例 (Chevron 开角 ~88°, 翼长 10.3px, 杆长 12px)
     * 2. 按下 (Pressed) 时：箭头向右压缩挤压 (Chevron 移动 +3.0px, 杆右端微移 +0.31px)
     * 3. 释放 (Release) 时：触发 WinUI 3 经典的弹簧弹射 (Slingshot) 效果，向左过冲 -1.34px 后平滑回弹复位
     * 4. 悬停 (Hover) 时：颜色保持 textPrimary 不变色
     */
    class AnimatedBackVisualSource : public AnimatedVisualSource {
    public:
        int duration(IconState from, IconState to, const fluent::FluentElement::Animation &anim) const override {
            if (to == IconState::Pressed) return anim.fast; // 150ms 快速按压压缩
            if (from == IconState::Pressed) return 320; // 320ms 弹簧回弹与过冲
            return anim.normal;
        }

        QEasingCurve easing(IconState from, IconState to, const fluent::FluentElement::Animation &anim) const override {
            if (to == IconState::Pressed) {
                return anim.standard;
            }
            return QEasingCurve::Linear; // 释放时内部使用 WinUI 3 精确双段贝塞尔/弹簧插值
        }

        void paint(QPainter &painter, const QRectF &rect,
                   IconState from, IconState to, qreal progress,
                   const fluent::FluentElement::Colors &colors, bool isEnabled) override {
            painter.setRenderHint(QPainter::Antialiasing);

            // WinUI 3 规范：悬停不变色，正常使用 textPrimary，禁用使用 textDisabled
            const QColor currentColor = isEnabled ? colors.textPrimary : colors.textDisabled;

            qreal chevronOffset = 0.0;
            qreal stemRightOffset = 0.0;

            if (to == IconState::Pressed) {
                // 按下过程 (0.0 -> 1.0)：向右挤压
                // WinUI 3 比例：Chevron 移动 +3.0px, 杆右端移动 +0.3125px
                const qreal t = std::sin(progress * M_PI / 2.0); // 减速按压
                chevronOffset = 3.0 * t;
                stemRightOffset = 0.3125 * t;
            } else if (from == IconState::Pressed) {
                // 释放过程 (0.0 -> 1.0)：WinUI 3 弹簧回弹 (Slingshot Overshoot)
                // 前半段 (0.0 -> 0.5)：从 +3.0px 飞速回弹并过冲至 -1.34px (杆右端过冲至 -1.25px)
                // 后半段 (0.5 -> 1.0)：从过冲位置减速复位至 0.0px
                if (progress < 0.5) {
                    const qreal u = progress * 2.0;
                    const qreal t = std::sin(u * M_PI / 2.0);
                    chevronOffset = 3.0 + (-1.34 - 3.0) * t;
                    stemRightOffset = 0.3125 + (-1.25 - 0.3125) * t;
                } else {
                    const qreal u = (progress - 0.5) * 2.0;
                    const qreal t = std::sin(u * M_PI / 2.0);
                    chevronOffset = -1.34 * (1.0 - t);
                    stemRightOffset = -1.25 * (1.0 - t);
                }
            }

            const QPointF center = rect.center();
            painter.save();
            painter.translate(center);

            // 精确坐标 (基于 WinUI 3 Geometry_0, 1, 2 缩放映射至 16px 标准网格)
            const qreal chevronX = -6.0 + chevronOffset;
            const qreal tipTopX = -0.628 + chevronOffset;
            const qreal tipBottomX = -0.628 + chevronOffset;
            const qreal stemLeftX = -4.0 + chevronOffset;
            const qreal stemRightX = 6.0 + stemRightOffset;

            QPainterPath path;

            // 1. V 形箭头 (Chevron)
            path.moveTo(tipTopX, -5.15);
            path.lineTo(chevronX, 0.0);
            path.lineTo(tipBottomX, 5.15);

            // 2. 横向箭杆 (Stem)
            path.moveTo(stemLeftX, 0.0);
            path.lineTo(stemRightX, 0.0);

            QPen pen(currentColor, 1.25, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);

            painter.restore();
        }
    };
} // namespace ui::animation
