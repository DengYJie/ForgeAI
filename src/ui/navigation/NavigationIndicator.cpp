#include "ui/navigation/NavigationIndicator.h"

#include <QDebug>
#include <QPainter>
#include <QPaintEvent>
#include <QShowEvent>
#include <QVariantAnimation>

#include "ui/navigation/NavigationMetrics.h"
#include "ui/navigation/NavigationWidget.h"

namespace {
    // WinUI 指示条前半段缓动：cubic-bezier(0.9,0.1,1.0,0.2)，快速冲刺冲过头
    QEasingCurve winuiOvershoot() {
        QEasingCurve c(QEasingCurve::BezierSpline);
        c.addCubicBezierSegment(QPointF(0.9, 0.1), QPointF(1.0, 0.2), QPointF(1.0, 1.0));
        return c;
    }

    // WinUI 指示条后半段缓动：cubic-bezier(0.1,0.9,0.2,1.0)，回弹 settle
    QEasingCurve winuiSettle() {
        QEasingCurve c(QEasingCurve::BezierSpline);
        c.addCubicBezierSegment(QPointF(0.1, 0.9), QPointF(0.2, 1.0), QPointF(1.0, 1.0));
        return c;
    }
}

namespace ui::navigation {
    fluent::FluentElement::Theme NavigationIndicator::cachedEffectiveTheme() const {
        if (!m_themeValid) {
            m_effectiveTheme = effectiveTheme();
            m_themeValid = true;
        }
        return m_effectiveTheme;
    }

    const fluent::FluentElement::Colors &NavigationIndicator::colorsRef() const {
        return fluent::ThemeRegistry::instance().colors(cachedEffectiveTheme() == fluent::FluentElement::Theme::Dark);
    }

    void NavigationIndicator::onThemeUpdated() {
        m_themeValid = false;
        update();
    }

    NavigationIndicator::NavigationIndicator(QWidget *parent)
        : QWidget(parent)
          , m_startRect(0, 0, kSelectionIndicatorWidth, kSelectionIndicatorHeight)
          , m_targetRect(m_startRect)
          , m_currentRect(m_startRect) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        hide();

        m_flightAnimation = new QVariantAnimation(this);
        m_flightAnimation->setEasingCurve(themeAnimation().decelerate);
        connect(m_flightAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            const qreal p = v.toDouble();

            if (m_animMode == AnimationMode::PortalReturn) {
                // 左固定、宽度从 0 增长到目标，模拟横向归位
                const qreal morphW = qMax(0.0, m_targetRect.width() * p);
                m_currentRect = QRectF(m_targetRect.x(), m_targetRect.y(), morphW, m_targetRect.height());
            } else if (m_animMode == AnimationMode::CrossWindowPortal) {
                // 按宿主方向分派形态：顶栏收缩、浮层生长
                if (m_orientation == Qt::Horizontal) {
                    // 左固定、右边界左移，宽度衰减到 0
                    const qreal morphW = qMax(0.0, m_startRect.width() * (1.0 - p));
                    m_currentRect = QRectF(m_startRect.x(), m_startRect.y(), morphW, m_startRect.height());
                } else {
                    // 顶部固定、高度从 0 增长到目标
                    const qreal growH = m_targetRect.height() * p;
                    m_currentRect = QRectF(m_targetRect.x(), m_targetRect.top(), m_targetRect.width(), growH);
                }
            } else if (m_orientation == Qt::Horizontal) {
                if (m_animMode == AnimationMode::Normal && m_sameAxisFlight) {
                    // WinUI 拉伸位移机制：Stretch-and-shift，位移快速到位后靠拉伸回弹
                    qreal scaleP = 0.0;
                    if (p <= 1.0 / 3.0) {
                        scaleP = winuiOvershoot().valueForProgress(p * 3.0);
                        if (m_targetRect.x() >= m_startRect.x()) {
                            // 向右移动
                            m_currentRect.setLeft(m_startRect.left());
                            m_currentRect.setRight(
                                m_startRect.right() + (m_targetRect.right() - m_startRect.right()) * scaleP);
                        } else {
                            // 向左移动
                            m_currentRect.setRight(m_startRect.right());
                            m_currentRect.setLeft(
                                m_startRect.left() + (m_targetRect.left() - m_startRect.left()) * scaleP);
                        }
                    } else {
                        scaleP = 1.0 - winuiSettle().valueForProgress((p - 1.0 / 3.0) * 1.5);
                        if (m_targetRect.x() >= m_startRect.x()) {
                            // 向右移动
                            m_currentRect.setRight(m_targetRect.right());
                            m_currentRect.setLeft(
                                m_targetRect.left() - (m_targetRect.left() - m_startRect.left()) * scaleP);
                        } else {
                            // 向左移动
                            m_currentRect.setLeft(m_targetRect.left());
                            m_currentRect.setRight(
                                m_targetRect.right() + (m_startRect.right() - m_targetRect.right()) * scaleP);
                        }
                    }
                    m_currentRect.setTop(m_targetRect.top());
                    m_currentRect.setBottom(m_targetRect.bottom());
                } else {
                    // 跨轴/降级动画：平滑移动 + 简单的弹性拉伸
                    const qreal moveP = p;
                    const qreal stretchAmp = (p <= 1.0 / 3.0) ? (p * 3.0) : ((1.0 - p) * 1.5);
                    const qreal current_x = m_startRect.x() + (m_targetRect.x() - m_startRect.x()) * moveP;
                    const qreal width = m_startRect.width() + (m_targetRect.width() - m_startRect.width()) * moveP;
                    const qreal stretch = stretchAmp * qMin(16.0, qAbs(m_targetRect.x() - m_startRect.x()) * 0.4);

                    m_currentRect.setY(m_startRect.y() + (m_targetRect.y() - m_startRect.y()) * moveP);
                    m_currentRect.setHeight(
                        m_startRect.height() + (m_targetRect.height() - m_startRect.height()) * moveP);

                    if (m_targetRect.x() >= m_startRect.x()) {
                        // 向右移动
                        m_currentRect.setLeft(current_x);
                        m_currentRect.setRight(current_x + width + stretch);
                    } else {
                        // 向左移动
                        m_currentRect.setLeft(current_x - stretch);
                        m_currentRect.setRight(current_x + width);
                    }
                }
            } else {
                // Left 模式：跨层级（X 不同）时用穿越门效果
                const bool portalMode = qAbs(m_startRect.x() - m_targetRect.x()) > 0.5;
                const bool movingUp = m_targetRect.y() <= m_startRect.y();

                if (portalMode) {
                    if (p < 0.5) {
                        const qreal lp = p * 2.0;
                        const qreal morphH = qMax(0.0, m_startRect.height() * (1.0 - lp));
                        m_currentRect.setX(m_startRect.x());
                        m_currentRect.setWidth(m_startRect.width());
                        if (movingUp) {
                            m_currentRect.setTop(m_startRect.top());
                            m_currentRect.setHeight(morphH);
                        } else {
                            m_currentRect.setTop(m_startRect.bottom() - morphH);
                            m_currentRect.setHeight(morphH);
                        }
                    } else {
                        const qreal lp = (p - 0.5) * 2.0;
                        const qreal morphH = qMax(0.0, m_targetRect.height() * lp);
                        m_currentRect.setX(m_targetRect.x());
                        m_currentRect.setWidth(m_targetRect.width());
                        if (movingUp) {
                            m_currentRect.setTop(m_targetRect.bottom() - morphH);
                            m_currentRect.setHeight(morphH);
                        } else {
                            m_currentRect.setTop(m_targetRect.top());
                            m_currentRect.setHeight(morphH);
                        }
                    }
                } else {
                    if (m_animMode == AnimationMode::Normal && m_sameAxisFlight) {
                        qreal scaleP = 0.0;
                        if (p <= 1.0 / 3.0) {
                            scaleP = winuiOvershoot().valueForProgress(p * 3.0);
                            if (m_targetRect.y() >= m_startRect.y()) {
                                // 向下移动
                                m_currentRect.setTop(m_startRect.top());
                                m_currentRect.setBottom(
                                    m_startRect.bottom() + (m_targetRect.bottom() - m_startRect.bottom()) * scaleP);
                            } else {
                                // 向上移动
                                m_currentRect.setBottom(m_startRect.bottom());
                                m_currentRect.setTop(
                                    m_startRect.top() + (m_targetRect.top() - m_startRect.top()) * scaleP);
                            }
                        } else {
                            scaleP = 1.0 - winuiSettle().valueForProgress((p - 1.0 / 3.0) * 1.5);
                            if (m_targetRect.y() >= m_startRect.y()) {
                                // 向下移动
                                m_currentRect.setBottom(m_targetRect.bottom());
                                m_currentRect.setTop(
                                    m_targetRect.top() - (m_targetRect.top() - m_startRect.top()) * scaleP);
                            } else {
                                // 向上移动
                                m_currentRect.setTop(m_targetRect.top());
                                m_currentRect.setBottom(
                                    m_targetRect.bottom() + (m_startRect.bottom() - m_targetRect.bottom()) * scaleP);
                            }
                        }
                        m_currentRect.setLeft(m_targetRect.left());
                        m_currentRect.setRight(m_targetRect.right());
                    } else {
                        const qreal moveP = p;
                        const qreal stretchAmp = (p <= 1.0 / 3.0) ? (p * 3.0) : ((1.0 - p) * 1.5);
                        const qreal current_y = m_startRect.y() + (m_targetRect.y() - m_startRect.y()) * moveP;
                        const qreal height = m_startRect.height() + (m_targetRect.height() - m_startRect.height()) *
                                             moveP;
                        const qreal stretch = stretchAmp * qMin(16.0, qAbs(m_targetRect.y() - m_startRect.y()) * 0.4);

                        m_currentRect.setX(m_startRect.x() + (m_targetRect.x() - m_startRect.x()) * moveP);
                        m_currentRect.setWidth(
                            m_startRect.width() + (m_targetRect.width() - m_startRect.width()) * moveP);

                        if (m_targetRect.y() >= m_startRect.y()) {
                            // 向下移动
                            m_currentRect.setTop(current_y);
                            m_currentRect.setBottom(current_y + height + stretch);
                        } else {
                            // 向上移动
                            m_currentRect.setTop(current_y - stretch);
                            m_currentRect.setBottom(current_y + height);
                        }
                    }
                }
            }

            // 缓冲一圈以容纳拉伸超出目标矩形的部分
            setGeometry(m_startRect.united(m_targetRect).toRect().adjusted(-20, -20, 20, 20));
            update();
        });
        connect(m_flightAnimation, &QVariantAnimation::finished, this, [this]() {
            const AnimationMode finishedMode = m_animMode;
            // 归一为目标几何，避免动画/无动画路径终点不一致导致后续 isSamePosition 误判
            m_animMode = AnimationMode::Normal;
            m_startRect = m_targetRect;
            m_currentRect = m_targetRect;
            hide();
            emit flightFinished();
        });
    }

    void NavigationIndicator::setOrientation(Qt::Orientation orientation) {
        if (m_orientation == orientation) return;
        m_orientation = orientation;
        update();
    }

    void NavigationIndicator::setInitialPosition(const QRectF &rect) {
        m_startRect = rect;
        m_targetRect = rect;
        m_currentRect = rect;
        setGeometry(rect.toRect().adjusted(-5, -5, 5, 5));
    }

    void NavigationIndicator::activateAt(const QRectF &targetRect, bool animated) {
        m_animMode = AnimationMode::Normal;
        const bool samePos = isSamePosition(targetRect);

        if (samePos) {
            // 位置未变，但仍需让 item 恢复常驻指示条（可能刚被清空）
            m_flightAnimation->stop();
            emit flightStarted();
            emit flightFinished();
            return;
        }

        raiseToTop();

        if (!animated || NavigationWidget::isReducedMotion()) {
            m_flightAnimation->stop();
            m_startRect = targetRect;
            m_targetRect = targetRect;
            m_currentRect = targetRect;
            hide();
            emit flightStarted();
            emit flightFinished();
            return;
        }

        m_startRect = m_currentRect;
        m_targetRect = targetRect;
        // 同轴平移（同 x 或同 y）用手动分段的 cubic-bezier 缓动，模拟 WinUI 的拉伸；
        // 跨轴 portal 保持平滑 decelerate 原样
        m_sameAxisFlight = qAbs(m_startRect.x() - m_targetRect.x()) < 0.5
                           || qAbs(m_startRect.y() - m_targetRect.y()) < 0.5;

        if (m_sameAxisFlight) {
            beginFlight(QEasingCurve::Linear, 600);
        } else {
            beginFlight(themeAnimation().decelerate);
        }
        setGeometry(m_startRect.united(m_targetRect).toRect().adjusted(-5, -5, 5, 5));
        show();
        emit flightStarted();
        m_flightAnimation->start();
    }

    void NavigationIndicator::playPortalReturn(const QRectF &targetRect) {
        if (NavigationWidget::isReducedMotion()) {
            m_startRect = targetRect;
            m_targetRect = targetRect;
            m_currentRect = targetRect;
            hide();
            emit flightStarted();
            emit flightFinished();
            return;
        }

        raiseToTop();
        m_animMode = AnimationMode::PortalReturn;
        // 起始位置：目标宽度为0
        m_startRect = QRectF(targetRect.x(), targetRect.y(), 0, targetRect.height());

        m_targetRect = targetRect;
        m_currentRect = m_startRect;
        beginFlight(themeAnimation().decelerate, themeAnimation().fast);
        setGeometry(targetRect.toRect().adjusted(-5, -5, 5, 5));
        show();
        emit flightStarted();
        m_flightAnimation->start();
    }

    void NavigationIndicator::playCrossWindowPortal(const QRectF &startRect, const QRectF &targetRect) {
        if (NavigationWidget::isReducedMotion()) {
            m_startRect = targetRect;
            m_targetRect = targetRect;
            m_currentRect = targetRect;
            hide();
            emit flightStarted();
            // Portal 动画不发 flightFinished（由调用方各自处理收尾）
            return;
        }

        raiseToTop();
        m_animMode = AnimationMode::CrossWindowPortal;
        m_startRect = startRect;
        m_targetRect = targetRect;
        beginFlight(themeAnimation().decelerate, themeAnimation().fast);

        // 几何缓冲与初始帧按宿主方向分派：顶栏收缩只需起始矩形，
        // 浮层向下生长（顶部固定、高度 0）只需覆盖目标矩形
        if (m_orientation == Qt::Horizontal) {
            m_currentRect = m_startRect;
            setGeometry(m_startRect.toRect().adjusted(-5, -5, 5, 5));
        } else {
            // 顶部固定、高度 0 的初始帧
            m_currentRect = QRectF(m_targetRect.x(), m_targetRect.top(), m_targetRect.width(), 0.0);
            setGeometry(m_targetRect.toRect().adjusted(-5, -5, 5, 5));
        }
        show();
        emit flightStarted();
        m_flightAnimation->start();
    }

    void NavigationIndicator::hideIndicator() {
        if (m_flightAnimation)
            m_flightAnimation->stop();
        hide();
    }

    bool NavigationIndicator::isFlying() const {
        return m_flightAnimation && m_flightAnimation->state() == QAbstractAnimation::Running;
    }

    bool NavigationIndicator::isSamePosition(const QRectF &targetRect) const {
        // 目标几何与当前几何几乎相同（<1px）时视为位置未变，短路避免无谓动画
        return qAbs(targetRect.x() - m_currentRect.x()) < 1.0
               && qAbs(targetRect.y() - m_currentRect.y()) < 1.0;
    }

    void NavigationIndicator::raiseToTop() {
        raise();
    }

    void NavigationIndicator::beginFlight(const QEasingCurve &easing, int durationMs) {
        m_flightAnimation->stop();
        m_flightAnimation->setEasingCurve(easing);
        m_flightAnimation->setStartValue(0.0);
        m_flightAnimation->setEndValue(1.0);
        m_flightAnimation->setDuration(durationMs > 0 ? durationMs : themeAnimation().normal);
    }

    void NavigationIndicator::paintEvent(QPaintEvent * /*event*/) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(colorsRef().accentDefault);

        // geometry 比实际 rect 大一圈（-5,-5,5,5），需映射回局部坐标
        const QPointF offset = -geometry().topLeft();
        const QRectF local = m_currentRect.translated(offset);

        painter.drawRoundedRect(local.adjusted(0, 0, -1, -1),
                                ::CornerRadius::Indicator, ::CornerRadius::Indicator);
    }

    void NavigationIndicator::showEvent(QShowEvent *event) {
        QWidget::showEvent(event);
        raiseToTop();
    }
} // namespace ui::navigation
