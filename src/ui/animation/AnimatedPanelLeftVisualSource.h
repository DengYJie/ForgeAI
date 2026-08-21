#pragma once
#include "AnimatedVisualSource.h"
#include <QPainterPath>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QtMath>
#include <functional>

namespace ui::animation {
    /**
     * @brief 左侧边栏专属矢量动画源
     * 在两种静态形态之间平滑形变（点击时触发展开/收起过渡）：
     * - 打开侧边栏 (Expanded): 外框 + 顶底相连的标准纵向分割线 (Media 1)
     * - 关闭侧边栏 (Collapsed): 外框 + 内部左侧独立浮动的圆角短胶囊竖条 (Media 2)
     */
    class AnimatedPanelLeftVisualSource : public QObject, public AnimatedVisualSource {
        Q_OBJECT

    public:
        explicit AnimatedPanelLeftVisualSource(QObject *parent = nullptr)
            : QObject(parent) {
            m_expandAnim = new QVariantAnimation(this);
            m_expandAnim->setDuration(220);
            m_expandAnim->setEasingCurve(QEasingCurve::OutCubic);
            connect(m_expandAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &val) {
                m_expandProgress = val.toReal();
                if (m_repaintCallback) {
                    m_repaintCallback();
                }
            });
        }

        ~AnimatedPanelLeftVisualSource() override = default;

        void setRepaintCallback(std::function<void()> cb) {
            m_repaintCallback = std::move(cb);
        }

        void setExpanded(bool expanded, bool animated = true) {
            m_isExpanded = expanded;
            const qreal target = expanded ? 1.0 : 0.0;

            if (animated) {
                m_expandAnim->stop();
                m_expandAnim->setStartValue(m_expandProgress);
                m_expandAnim->setEndValue(target);
                m_expandAnim->start();
            } else {
                m_expandAnim->stop();
                m_expandProgress = target;
                if (m_repaintCallback) {
                    m_repaintCallback();
                }
            }
        }

        bool isExpanded() const {
            return m_isExpanded;
        }

        qreal expandProgress() const {
            return m_expandProgress;
        }

        int duration(IconState from, IconState to, const fluent::FluentElement::Animation &anim) const override {
            Q_UNUSED(from);
            Q_UNUSED(to);
            return anim.normal;
        }

        QEasingCurve easing(IconState from, IconState to, const fluent::FluentElement::Animation &anim) const override {
            Q_UNUSED(from);
            Q_UNUSED(to);
            return anim.decelerate;
        }

        void paint(QPainter &painter, const QRectF &rect,
                   IconState from, IconState to, qreal progress,
                   const fluent::FluentElement::Colors &colors, bool isEnabled) override {
            Q_UNUSED(from);
            Q_UNUSED(to);
            Q_UNUSED(progress);
            painter.setRenderHint(QPainter::Antialiasing, true);

            const QColor currentColor = isEnabled ? colors.textPrimary : colors.textDisabled;
            const QPointF center = rect.center();

            painter.save();
            painter.translate(center);

            // 1. 基础尺寸与外框几何参数 (16px 标准网格)
            constexpr qreal halfW = 6.5;
            constexpr qreal halfH = 5.5;
            constexpr qreal cornerRadius = 2.2;

            // 2. 绘制外层圆角矩形外框 (Outer Rounded Frame)
            const QRectF frameRect(-halfW, -halfH, halfW * 2.0, halfH * 2.0);
            QPen strokePen(currentColor, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(strokePen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(frameRect, cornerRadius, cornerRadius);

            // 3. 计算开合进度 t (0.0 = 关闭/短胶囊, 1.0 = 打开/全高分割线)
            const qreal t = m_expandProgress;

            // 4. 内部竖条在两种形态间的精确几何插值 (Morphing)
            // - 打开态 (t=1.0): X = -2.2, topY = -5.5, bottomY = +5.5 (贯穿顶底)
            // - 关闭态 (t=0.0): X = -2.8, topY = -2.6, bottomY = +2.6 (短胶囊)
            const qreal currentX = (1.0 - t) * (-2.8) + t * (-2.2);
            const qreal currentTopY = (1.0 - t) * (-2.6) + t * (-halfH);
            const qreal currentBottomY = (1.0 - t) * (+2.6) + t * (+halfH);

            // 当完全展开时使用平头线帽贴合外框，折叠态使用圆角线帽
            const Qt::PenCapStyle capStyle = (t > 0.9) ? Qt::SquareCap : Qt::RoundCap;
            QPen innerPen(currentColor, 1.2, Qt::SolidLine, capStyle, Qt::RoundJoin);
            painter.setPen(innerPen);

            QPainterPath innerPath;
            innerPath.moveTo(currentX, currentTopY);
            innerPath.lineTo(currentX, currentBottomY);
            painter.drawPath(innerPath);

            painter.restore();
        }

    private:
        bool m_isExpanded = true;
        qreal m_expandProgress = 1.0;
        QVariantAnimation *m_expandAnim = nullptr;
        std::function<void()> m_repaintCallback;
    };
} // namespace ui::animation
