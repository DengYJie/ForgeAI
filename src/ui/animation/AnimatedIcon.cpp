#include "AnimatedIcon.h"
#include <FluentQt/Design.h>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QVariantAnimation>

namespace ui::animation {
    AnimatedIcon::AnimatedIcon(QWidget *parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        m_animation = new QVariantAnimation(this);
        connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &val) {
            m_progress = val.toReal();
            update();
        });

        if (parent) {
            parent->installEventFilter(this);
            setGeometry(0, 0, parent->width(), parent->height());
        }
    }

    AnimatedIcon::~AnimatedIcon() = default;

    void AnimatedIcon::setSource(std::shared_ptr<AnimatedVisualSource> source) {
        m_source = std::move(source);
        update();
    }

    void AnimatedIcon::setState(IconState newState) {
        if (m_currentState == newState || !m_source) return;

        qreal startProgress = 0.0;
        bool isReversing = false;
        const bool wasRunning = (m_animation->state() == QAbstractAnimation::Running);

        // 如果之前的动画尚未完成，且新状态等于起始状态（即原路返回被打断）
        if (wasRunning) {
            if (newState == m_previousState) {
                isReversing = true;
                // 反向插值：剩下的过渡路程比例刚好是 1.0 - 当前进度
                startProgress = 1.0 - m_progress;
            }
        }

        m_previousState = m_currentState;
        m_currentState = newState;

        m_animation->stop();
        int duration = m_source->duration(m_previousState, m_currentState, themeAnimation());

        if (isReversing) {
            // 根据剩余的进度比例等比缩短总时长，保证过渡速度一致性
            duration = qMax(1, static_cast<int>(duration * (1.0 - startProgress)));
        }

        m_animation->setDuration(duration);
        m_animation->setEasingCurve(m_source->easing(m_previousState, m_currentState, themeAnimation()));
        m_animation->setStartValue(startProgress);
        m_animation->setEndValue(1.0);
        m_animation->start();
    }

    qreal AnimatedIcon::progress() const {
        return m_progress;
    }

    void AnimatedIcon::setProgress(qreal p) {
        m_progress = p;
        update();
    }

    void AnimatedIcon::paintEvent(QPaintEvent *) {
        if (!m_source) return;
        QPainter painter(this);

        m_source->paint(painter, rect(), m_previousState, m_currentState, m_progress, themeColorsRef(), isEnabled());
    }

    bool AnimatedIcon::eventFilter(QObject *watched, QEvent *event) {
        if (watched == parentWidget()) {
            switch (event->type()) {
                case QEvent::Enter:
                    setState(IconState::PointerOver);
                    break;
                case QEvent::Leave:
                    setState(IconState::Normal);
                    break;
                case QEvent::MouseButtonPress:
                    if (static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
                        setState(IconState::Pressed);
                    }
                    break;
                case QEvent::MouseButtonRelease:
                    if (static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
                        const bool under = (parentWidget() && parentWidget()->underMouse());
                        setState(under ? IconState::PointerOver : IconState::Normal);
                    }
                    break;
                case QEvent::Resize:
                    if (parentWidget() && m_autoFillParent) {
                        setGeometry(0, 0, parentWidget()->width(), parentWidget()->height());
                    }
                    break;
                case QEvent::Hide:
                    // 控件隐藏时重置动画并复位至 Normal
                    m_animation->stop();
                    m_currentState = IconState::Normal;
                    m_previousState = IconState::Normal;
                    m_progress = 1.0;
                    break;
                case QEvent::EnabledChange:
                    update();
                    break;
                default:
                    break;
            }
        }
        return QWidget::eventFilter(watched, event);
    }
} // namespace ui::animation
