#include "ui/navigation/NavigationToolButton.h"

#include <QMoveEvent>
#include <QShowEvent>
#include <QTimer>
#include <QVariantAnimation>

#include "ui/navigation/NavigationMetrics.h"

namespace ui::navigation {
    NavigationToolButton::NavigationToolButton(const QString &iconGlyph, QWidget *parent)
        : NavigationPushButton(iconGlyph, QString(), false, parent) {
        setFixedSize(kTopBarItemHeight, kTopBarItemHeight);
        setFocusPolicy(Qt::TabFocus);
    }

    void NavigationToolButton::setAnimatedMove(bool enabled) {
        if (m_animatedMove == enabled)
            return;
        m_animatedMove = enabled;
        if (enabled) {
            if (!m_moveAnimation) {
                m_moveAnimation = new QVariantAnimation(this);
                // 对齐 WinUI overflow 按钮的 Offset 隐式动画：200ms + cubic-bezier(0,0.35,0.15,1) ease-out
                QEasingCurve curve(QEasingCurve::BezierSpline);
                curve.addCubicBezierSegment(QPointF(0.0, 0.35), QPointF(0.15, 1.0), QPointF(1.0, 1.0));
                m_moveAnimation->setEasingCurve(curve);
                m_moveAnimation->setDuration(200);
                // 手动 move() 同步驱动位置：move 会立即触发 moveEvent，
                // 前后同步置位 m_animating 供其识别为动画帧，move 返回即复位、无残留
                connect(m_moveAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
                    m_animating = true;
                    move(v.toPoint());
                    m_animating = false;
                });
            }
        } else if (m_moveAnimation) {
            m_moveAnimation->stop();
        }
    }

    void NavigationToolButton::runPendingMove() {
        m_moveScheduled = false;
        // 刚显示标记仅对本次恢复后的首次排版有效，动画调度后复位
        m_justShown = false;
        if (m_pendingFrom == m_pendingTarget)
            return;
        m_moveAnimation->stop();
        m_moveAnimation->setStartValue(m_pendingFrom);
        m_moveAnimation->setEndValue(m_pendingTarget);
        m_moveAnimation->start();
    }

    void NavigationToolButton::moveEvent(QMoveEvent *event) {
        // 动画手动 move() 触发的回调（valueChanged 同步置位）：放行，不当作布局重排
        if (m_animating) {
            m_animating = false;
            QWidget::moveEvent(event);
            return;
        }

        // 未开启位移动画：直接走基类，不干预
        if (!m_animatedMove || !m_moveAnimation) {
            QWidget::moveEvent(event);
            return;
        }

        // 布局重排：记录起点(首次)与最新目标，延迟到本轮布局全部稳定后再统一动画一次，
        // 避免一次重排对同一控件多次 move 各自起动画造成来回抖动
        if (!m_moveScheduled) {
            m_moveScheduled = true;
            const QPoint from = event->oldPos();
            // 无效初始坐标（首次进布局未定义）或刚隐藏恢复：直接落位，不从历史残留坐标"倒飞"
            if (from.isNull() || from.x() < 0 || m_justShown) {
                m_pendingFrom = event->pos();
                m_pendingTarget = event->pos();
            } else {
                m_pendingFrom = from;
            }
            QTimer::singleShot(0, this, [this] { runPendingMove(); });
        }
        m_pendingTarget = event->pos();

        QWidget::moveEvent(event);
    }

    void NavigationToolButton::showEvent(QShowEvent *event) {
        QWidget::showEvent(event);
        // 从隐藏恢复时 oldPos 会残留上次可见坐标，标记本次排版起点取目标而非残留值
        m_justShown = true;
    }

    void NavigationToolButton::setCompacted(bool /*compacted*/) {
        // 纯图标按钮在 compact 下尺寸/图标位置不变
    }

    void NavigationToolButton::setOrientation(Qt::Orientation orientation) {
        NavigationPushButton::setOrientation(orientation);
        // 基类按方向 setFixedHeight 会覆盖固定尺寸，这里重设回 48x48 方形热区
        setFixedSize(kTopBarItemHeight, kTopBarItemHeight);
    }

    int NavigationToolButton::iconDrawX() const {
        return qMax(0, (width() - m_iconSize) / 2);
    }
} // namespace ui::navigation
