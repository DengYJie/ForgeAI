#pragma once
#include "AnimatedVisualSource.h"
#include <QPainterPath>

namespace ui::animation {
    /**
     * @brief 全局导航菜单（汉堡按钮）专属矢量动画源
     * 对标 WinUI 3 官方 AnimatedGlobalNavigationButtonVisualSource (Controls_02_Hamburger)
     *
     * 在 Normal 状态下绘制等长三条横线；
     * 在 Pressed 状态下三条横线同步产生横向内缩挤压，配合 Fluent 减速曲线实现弹性回弹。
     */
    class AnimatedGlobalNavVisualSource : public AnimatedVisualSource {
    public:
        int duration(IconState from, IconState to, const fluent::FluentElement::Animation &anim) const override {
            if (to == IconState::Pressed) return anim.fast;
            return anim.normal;
        }

        QEasingCurve easing(IconState from, IconState to, const fluent::FluentElement::Animation &anim) const override {
            if (to == IconState::Pressed) {
                return anim.standard;
            }
            return anim.decelerate; // 释放时平滑减速
        }

        void paint(QPainter &painter, const QRectF &rect,
                   IconState from, IconState to, qreal progress,
                   const fluent::FluentElement::Colors &colors, bool isEnabled) override {
            painter.setRenderHint(QPainter::Antialiasing);

            // WinUI 3 规范：悬停不变色，正常使用 textPrimary，禁用使用 textDisabled
            const QColor currentColor = isEnabled ? colors.textPrimary : colors.textDisabled;

            // 计算当前按压插值 (0.0=正常, 1.0=完全按下)
            qreal pressRatio = 0.0;
            if (to == IconState::Pressed) {
                pressRatio = progress;
            } else if (from == IconState::Pressed) {
                pressRatio = 1.0 - progress;
            }

            const QPointF center = rect.center();
            painter.save();
            painter.translate(center);

            // WinUI 3 Controls_02_Hamburger 官方精确数学参数：
            // - 画布 48x48，等效到 16px 视口下横线全长 15.0px (halfWidth = 7.5px)
            // - 线条间距 (Group Offset) 为 4.85px (Y = -4.85, 0.0, +4.85)
            // - 按下时 Offset 从 37.5/10.5 压缩至 27.5/20.5，压缩比严格为 (10.0 / 24.0) = 41.67%
            //   即每端向内收缩 7.5 * 0.4167 = 3.125px，总长度由 15.0px 压缩至 8.75px (原长的 58.33%)
            const qreal baseHalfWidth = 7.5;
            const qreal shrink = 3.125 * pressRatio;
            const qreal currentHalfWidth = baseHalfWidth - shrink;
            const qreal lineSpacing = 4.85;

            QPainterPath path;

            // 顶线
            path.moveTo(-currentHalfWidth, -lineSpacing);
            path.lineTo(currentHalfWidth, -lineSpacing);

            // 中线
            path.moveTo(-currentHalfWidth, 0.0);
            path.lineTo(currentHalfWidth, 0.0);

            // 底线
            path.moveTo(-currentHalfWidth, lineSpacing);
            path.lineTo(currentHalfWidth, lineSpacing);

            QPen pen(currentColor, 1.25, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);

            painter.restore();
        }
    };
} // namespace ui::animation
