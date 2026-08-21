#include "CollapsibleSplitView.h"

#include <QDataStream>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <FluentQt/Design.h>

namespace {
    constexpr int kOpenDurationMs = 350;
    constexpr int kCloseDurationMs = 150;

    constexpr int kDefaultCompactLength = 48;
    constexpr int kDefaultOpenLength = 320;
    constexpr int kDefaultMinOpenLength = 120;
    constexpr int kDefaultMaxOpenLength = 16777215;

    constexpr quint32 kStateMagic = 0x53504C56;
    constexpr quint32 kStateVersion = 1;
} // namespace

namespace ui::widget {

class LightDismissOverlay : public QWidget, public fluent::FluentElement {
    Q_OBJECT

public:
    explicit LightDismissOverlay(QWidget *parent = nullptr)
        : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
        setAutoFillBackground(false);
        hide();
    }

    void setOpacity(qreal opacity) {
        m_opacity = qBound(0.0, opacity, 1.0);
        update();
    }

    qreal opacity() const { return m_opacity; }

    void onThemeUpdated() override {
        update();
    }

Q_SIGNALS:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override {
        event->accept();
        emit clicked();
    }

    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);
        if (m_opacity <= 0.0)
            return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);

        const auto &smoke = themeSmoke();
        QColor maskColor = smoke.baseColor;
        maskColor.setAlphaF(smoke.opacity * m_opacity);
        painter.fillRect(rect(), maskColor);
    }

private:
    qreal m_opacity = 0.0;
};

class SlideViewportContainer : public QWidget, public fluent::FluentElement {
    Q_OBJECT

public:
    explicit SlideViewportContainer(QWidget *content, QWidget *parent = nullptr)
        : QWidget(parent), m_content(content) {
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        setAutoFillBackground(false);
        setCursor(Qt::ArrowCursor);
        if (content) {
            content->setParent(this);
            content->show();
        }
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    }

    void setCommittedLength(int length, Qt::Orientation orientation) {
        m_committedLength = length;
        m_orientation = orientation;
        updateContentGeometry();
    }

    void setOverlayActive(bool active) {
        if (m_isOverlayActive == active)
            return;
        m_isOverlayActive = active;
        update();
    }

    QSize sizeHint() const override {
        return m_content ? m_content->sizeHint() : QSize(kDefaultOpenLength, 400);
    }

    void onThemeUpdated() override {
        if (m_content) {
            if (auto *elem = dynamic_cast<fluent::FluentElement *>(m_content.data())) {
                elem->onThemeUpdated();
            }
        }
        update();
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        updateContentGeometry();
    }

    void paintEvent(QPaintEvent *event) override {
        QWidget::paintEvent(event);
        if (!m_isOverlayActive)
            return;

        // 悬浮态下绘制边缘分隔线
        QPainter painter(this);
        painter.setPen(themeColorsRef().strokeDivider);
        if (m_orientation == Qt::Horizontal) {
            painter.drawLine(width() - 1, 0, width() - 1, height());
        } else {
            painter.drawLine(0, height() - 1, width(), height() - 1);
        }
    }

private:
    void updateContentGeometry() {
        if (!m_content)
            return;

        // 保持逻辑尺寸不变，超出的部分由父级视口自然裁剪（零重排）
        if (m_orientation == Qt::Horizontal) {
            m_content->setGeometry(0, 0, m_committedLength, height());
        } else {
            m_content->setGeometry(0, 0, width(), m_committedLength);
        }
    }

    QPointer<QWidget> m_content;
    Qt::Orientation m_orientation = Qt::Horizontal;
    int m_committedLength = kDefaultOpenLength;
    bool m_isOverlayActive = false;
};

CollapsibleSplitView::CollapsibleSplitView(QWidget *parent)
    : fluent::collections::SplitView(parent) {
    setHandleVisualThickness(1);
    setHandleWidth(8);

    m_lightDismissOverlay = new LightDismissOverlay(this);
    connect(m_lightDismissOverlay.data(), &LightDismissOverlay::clicked,
            this, &CollapsibleSplitView::handleLightDismissClicked);

    connect(this, &fluent::collections::SplitView::paneSizeChanged,
            this, &CollapsibleSplitView::onPaneSizeChanged);
}

CollapsibleSplitView::~CollapsibleSplitView() {
    for (auto &st : m_paneStates) {
        if (st.animation) {
            st.animation->stop();
        }
        if (st.viewportWrapper) {
            disconnect(st.viewportWrapper.data(), nullptr, this, nullptr);
        }
        if (st.userWidget) {
            disconnect(st.userWidget.data(), nullptr, this, nullptr);
        }
    }
    m_paneStates.clear();
}

int CollapsibleSplitView::addCollapsiblePane(
    QWidget *pane,
    SplitPaneDisplayMode mode,
    int compactLength,
    bool startExpanded,
    int initialOpenLength,
    const fluent::collections::SplitViewPaneOptions &options) {

    auto *wrapper = new SlideViewportContainer(pane, this);
    const int effectiveMinOpen = options.minimumSize > 0 ? options.minimumSize : qMax(kDefaultMinOpenLength, compactLength);
    const int effectiveMaxOpen = options.maximumSize > 0 ? options.maximumSize : kDefaultMaxOpenLength;
    const int effectiveOpenLength = qBound(effectiveMinOpen, qMax(initialOpenLength, compactLength), effectiveMaxOpen);
    wrapper->setCommittedLength(effectiveOpenLength, orientation());

    fluent::collections::SplitViewPaneOptions effectiveOptions = options;
    if (isOverlayMode(mode)) {
        // 悬浮模式下底层分栏槽位尺寸固定，展开时不挤压主工作区
        const int fixedBaseSize = (mode == SplitPaneDisplayMode::CompactOverlay) ? compactLength : 0;
        effectiveOptions.preferredSize = fixedBaseSize;
        effectiveOptions.minimumSize = fixedBaseSize;
        effectiveOptions.maximumSize = fixedBaseSize;
    } else {
        effectiveOptions.preferredSize = startExpanded ? effectiveOpenLength : (mode == SplitPaneDisplayMode::CompactInline ? compactLength : 0);
        effectiveOptions.minimumSize = 0;
        effectiveOptions.maximumSize = startExpanded ? effectiveMaxOpen : (mode == SplitPaneDisplayMode::CompactInline ? compactLength : 0);
    }

    const int index = addPane(wrapper, effectiveOptions);

    PaneState st;
    st.userWidget = pane;
    st.viewportWrapper = wrapper;
    st.mode = mode;
    st.compactLength = compactLength;
    st.openLength = effectiveOpenLength;
    st.minOpenLength = effectiveMinOpen;
    st.maxOpenLength = effectiveMaxOpen;
    st.isExpanded = startExpanded;
    st.currentAnimatedLength = startExpanded ? effectiveOpenLength : collapsedLength(st);

    st.animation = new QVariantAnimation(this);
    st.animation->setEasingCurve(themeAnimation().decelerate);

    connect(st.animation, &QVariantAnimation::valueChanged, this,
            [this, wrapper](const QVariant &value) {
                const int idx = indexForPane(wrapper);
                if (idx < 0)
                    return;

                auto *state = stateForIndex(idx);
                if (!state)
                    return;

                const int currentLength = value.toInt();
                state->currentAnimatedLength = currentLength;

                if (isOverlayMode(state->mode)) {
                    updateOverlayLayout();

                    if (m_lightDismissOverlay) {
                        const qreal progress = static_cast<qreal>(currentLength - state->compactLength) /
                                               qMax(1, state->openLength - state->compactLength);
                        m_lightDismissOverlay->setOpacity(qBound(0.0, progress, 1.0));
                    }
                } else {
                    ++m_suppressSizeMemory;
                    setPanePreferredSize(idx, currentLength);
                    --m_suppressSizeMemory;
                }
            });

    connect(st.animation, &QVariantAnimation::finished, this,
            [this, wrapper]() {
                const int idx = indexForPane(wrapper);
                if (idx >= 0)
                    finishPaneAnimation(idx);
            });

    m_paneStates.insert(wrapper, st);

    connect(wrapper, &QObject::destroyed,
            this, &CollapsibleSplitView::onPaneDestroyed,
            Qt::UniqueConnection);

    if (pane) {
        connect(pane, &QObject::destroyed,
                this, &CollapsibleSplitView::onPaneDestroyed,
                Qt::UniqueConnection);
    }

    if (!startExpanded) {
        const int target = collapsedLength(st);
        if (!isOverlayMode(mode)) {
            ++m_suppressSizeMemory;
            setPanePreferredSize(index, target);
            --m_suppressSizeMemory;
        }
        wrapper->setCommittedLength(target, orientation());
        if (target == 0) {
            wrapper->hide();
        }
    }

    updatePaneConstraints(index);
    updateOverlayLayout();
    return index;
}

void CollapsibleSplitView::removeCollapsiblePane(int index) {
    if (index < 0 || index >= paneCount())
        return;

    QWidget *wrapper = paneAt(index);
    if (!wrapper)
        return;

    if (m_paneStates.contains(wrapper)) {
        auto &st = m_paneStates[wrapper];
        if (st.animation) {
            st.animation->stop();
        }
        m_paneStates.remove(wrapper);
    }

    removePaneAt(index);
    wrapper->deleteLater();
    updateOverlayLayout();
}

bool CollapsibleSplitView::isPaneExpanded(int index) const {
    const auto *st = stateForIndex(index);
    return st ? st->isExpanded : false;
}

bool CollapsibleSplitView::isPaneAnimating(int index) const {
    const auto *st = stateForIndex(index);
    return st && st->animation && st->animation->state() == QAbstractAnimation::Running;
}

SplitPaneDisplayMode CollapsibleSplitView::paneDisplayMode(int index) const {
    const auto *st = stateForIndex(index);
    return st ? st->mode : SplitPaneDisplayMode::Inline;
}

int CollapsibleSplitView::paneOpenLength(int index) const {
    const auto *st = stateForIndex(index);
    return st ? st->openLength : 0;
}

int CollapsibleSplitView::paneCompactLength(int index) const {
    const auto *st = stateForIndex(index);
    return st ? st->compactLength : 0;
}

int CollapsibleSplitView::paneMinOpenLength(int index) const {
    const auto *st = stateForIndex(index);
    return st ? st->minOpenLength : 0;
}

int CollapsibleSplitView::paneMaxOpenLength(int index) const {
    const auto *st = stateForIndex(index);
    return st ? st->maxOpenLength : kDefaultMaxOpenLength;
}

void CollapsibleSplitView::togglePane(int index, bool animated) {
    const auto *st = stateForIndex(index);
    if (st) {
        setPaneExpanded(index, !st->isExpanded, animated);
    }
}

void CollapsibleSplitView::setPaneExpanded(int index, bool expanded, bool animated) {
    auto *st = stateForIndex(index);
    if (!st || st->isExpanded == expanded)
        return;

    st->isExpanded = expanded;

    const int target = expanded ? st->openLength : collapsedLength(*st);

    if (expanded) {
        if (auto *w = qobject_cast<SlideViewportContainer *>(st->viewportWrapper.data())) {
            w->setCommittedLength(st->openLength, orientation());
        }
    }

    if (isOverlayMode(st->mode) && expanded) {
        if (st->viewportWrapper) {
            st->viewportWrapper->setOverlayActive(true);
            st->viewportWrapper->show();
        }
        if (m_lightDismissOverlay && m_lightDismissEnabled) {
            m_lightDismissOverlay->setGeometry(rect());
            m_lightDismissOverlay->show();
            m_lightDismissOverlay->raise();
        }
        if (st->viewportWrapper) {
            st->viewportWrapper->raise();
        }
    }

    if (animated && shouldAnimate()) {
        if (!isOverlayMode(st->mode)) {
            // 动画期间临时放开边界约束，允许从 260 平滑过渡到 0，避免被提前截断
            setPaneMinimumSize(index, 0);
            setPaneMaximumSize(index, st->maxOpenLength);
        }

        if (expanded)
            emit paneOpening(index);
        else
            emit paneClosing(index);

        animateTo(index, target, expanded);
    } else {
        if (st->animation && st->animation->state() == QAbstractAnimation::Running) {
            st->animation->stop();
        }

        st->currentAnimatedLength = target;

        if (!isOverlayMode(st->mode)) {
            ++m_suppressSizeMemory;
            setPanePreferredSize(index, target);
            --m_suppressSizeMemory;
        }

        if (auto *w = qobject_cast<SlideViewportContainer *>(st->viewportWrapper.data())) {
            w->setCommittedLength(target, orientation());
            if (expanded) {
                w->show();
            } else if (target == 0) {
                w->hide();
            }
        }

        updatePaneConstraints(index);
        finishPaneAnimation(index);
    }
}

void CollapsibleSplitView::setPaneDisplayMode(int index, SplitPaneDisplayMode mode) {
    auto *st = stateForIndex(index);
    if (!st || st->mode == mode)
        return;

    st->mode = mode;
    updatePaneConstraints(index);

    if (!st->isExpanded) {
        const int target = collapsedLength(*st);
        st->currentAnimatedLength = target;

        if (!isOverlayMode(mode)) {
            ++m_suppressSizeMemory;
            setPanePreferredSize(index, target);
            --m_suppressSizeMemory;
        }

        if (auto *w = qobject_cast<SlideViewportContainer *>(st->viewportWrapper.data()))
            w->setCommittedLength(target, orientation());
    }

    updateOverlayLayout();
    emit paneDisplayModeChanged(index, mode);
}

void CollapsibleSplitView::setPaneCompactLength(int index, int length) {
    auto *st = stateForIndex(index);
    if (!st || st->compactLength == length)
        return;

    st->compactLength = length;
    st->minOpenLength = qMax(kDefaultMinOpenLength, length);
    if (st->openLength < length)
        st->openLength = length;

    if (!st->isExpanded && (st->mode == SplitPaneDisplayMode::CompactInline || st->mode == SplitPaneDisplayMode::CompactOverlay)) {
        if (!isOverlayMode(st->mode)) {
            ++m_suppressSizeMemory;
            setPanePreferredSize(index, length);
            --m_suppressSizeMemory;
        }

        if (auto *w = qobject_cast<SlideViewportContainer *>(st->viewportWrapper.data()))
            w->setCommittedLength(length, orientation());
    }

    updatePaneConstraints(index);
    updateOverlayLayout();
    emit paneCompactLengthChanged(index, length);
}

void CollapsibleSplitView::setPaneOpenLength(int index, int length) {
    auto *st = stateForIndex(index);
    if (!st || st->openLength == length)
        return;

    st->openLength = qBound(st->minOpenLength, length, st->maxOpenLength);

    if (st->isExpanded) {
        if (!isOverlayMode(st->mode)) {
            ++m_suppressSizeMemory;
            setPanePreferredSize(index, st->openLength);
            --m_suppressSizeMemory;
        }

        if (auto *w = qobject_cast<SlideViewportContainer *>(st->viewportWrapper.data()))
            w->setCommittedLength(st->openLength, orientation());
    }

    updatePaneConstraints(index);
    updateOverlayLayout();
}

void CollapsibleSplitView::setPaneMinOpenLength(int index, int minLength) {
    auto *st = stateForIndex(index);
    if (!st || st->minOpenLength == minLength)
        return;

    st->minOpenLength = minLength;
    if (st->openLength < minLength)
        st->openLength = minLength;

    updatePaneConstraints(index);
}

void CollapsibleSplitView::setPaneMaxOpenLength(int index, int maxLength) {
    auto *st = stateForIndex(index);
    if (!st || st->maxOpenLength == maxLength)
        return;

    st->maxOpenLength = maxLength;
    if (st->openLength > maxLength)
        st->openLength = maxLength;

    updatePaneConstraints(index);
}

void CollapsibleSplitView::setLightDismissEnabled(bool enabled) {
    m_lightDismissEnabled = enabled;
    if (!enabled && m_lightDismissOverlay) {
        m_lightDismissOverlay->hide();
    }
}

QByteArray CollapsibleSplitView::saveCollapsibleState() const {
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << kStateMagic;
    stream << kStateVersion;
    stream << saveState();

    const int count = paneCount();
    stream << qint32(count);
    for (int i = 0; i < count; ++i) {
        QWidget *w = paneAt(i);
        if (m_paneStates.contains(w)) {
            const auto &st = m_paneStates[w];
            stream << true;
            stream << static_cast<qint32>(st.mode);
            stream << st.isExpanded;
            stream << qint32(st.compactLength);
            stream << qint32(st.openLength);
            stream << qint32(st.minOpenLength);
            stream << qint32(st.maxOpenLength);
        } else {
            stream << false;
        }
    }
    return data;
}

bool CollapsibleSplitView::restoreCollapsibleState(const QByteArray &state) {
    if (state.isEmpty())
        return false;

    QDataStream stream(state);
    quint32 magic = 0, version = 0;
    stream >> magic >> version;
    if (magic != kStateMagic || version != kStateVersion)
        return false;

    QByteArray baseState;
    stream >> baseState;
    restoreState(baseState);

    qint32 count = 0;
    stream >> count;
    for (int i = 0; i < count && i < paneCount(); ++i) {
        bool isCollapsible = false;
        stream >> isCollapsible;
        if (isCollapsible) {
            qint32 modeVal = 0, compactLen = kDefaultCompactLength, openLen = kDefaultOpenLength, minOpenLen = kDefaultMinOpenLength, maxOpenLen = kDefaultMaxOpenLength;
            bool isExp = true;
            stream >> modeVal >> isExp >> compactLen >> openLen >> minOpenLen;
            if (!stream.atEnd()) {
                stream >> maxOpenLen;
            }

            QWidget *w = paneAt(i);
            if (m_paneStates.contains(w)) {
                auto &st = m_paneStates[w];
                st.mode = static_cast<SplitPaneDisplayMode>(modeVal);
                st.compactLength = compactLen;
                st.openLength = openLen;
                st.minOpenLength = minOpenLen;
                st.maxOpenLength = maxOpenLen;
                setPaneExpanded(i, isExp, false);
            }
        }
    }
    return true;
}

CollapsibleSplitView::PaneState *CollapsibleSplitView::stateForIndex(int index) {
    if (index < 0 || index >= paneCount())
        return nullptr;
    QWidget *w = paneAt(index);
    if (!w)
        return nullptr;
    auto it = m_paneStates.find(w);
    if (it != m_paneStates.end()) {
        return &it.value();
    }
    return nullptr;
}

const CollapsibleSplitView::PaneState *CollapsibleSplitView::stateForIndex(int index) const {
    if (index < 0 || index >= paneCount())
        return nullptr;
    QWidget *w = paneAt(index);
    if (!w)
        return nullptr;
    auto it = m_paneStates.find(w);
    if (it != m_paneStates.end()) {
        return &it.value();
    }
    return nullptr;
}

int CollapsibleSplitView::indexForPane(QWidget *wrapper) const {
    if (!wrapper)
        return -1;
    return indexOf(wrapper);
}

bool CollapsibleSplitView::isOverlayMode(SplitPaneDisplayMode mode) const {
    return mode == SplitPaneDisplayMode::Overlay || mode == SplitPaneDisplayMode::CompactOverlay;
}

int CollapsibleSplitView::collapsedLength(const PaneState &st) const {
    return (st.mode == SplitPaneDisplayMode::CompactInline || st.mode == SplitPaneDisplayMode::CompactOverlay)
               ? st.compactLength
               : 0;
}

bool CollapsibleSplitView::shouldAnimate() const {
    return isVisible();
}

void CollapsibleSplitView::updatePaneConstraints(int index) {
    auto *st = stateForIndex(index);
    if (!st)
        return;

    if (isOverlayMode(st->mode)) {
        const int fixedBase = (st->mode == SplitPaneDisplayMode::CompactOverlay) ? st->compactLength : 0;
        setPaneMinimumSize(index, fixedBase);
        setPaneMaximumSize(index, fixedBase);
    } else {
        if (st->isExpanded) {
            setPaneMinimumSize(index, qMax(st->minOpenLength, st->compactLength));
            setPaneMaximumSize(index, st->maxOpenLength);
        } else {
            const int c = collapsedLength(*st);
            setPaneMinimumSize(index, c);
            setPaneMaximumSize(index, c);
        }
    }
}

void CollapsibleSplitView::updateOverlayLayout() {
    bool hasActiveOverlay = false;

    for (int i = 0; i < paneCount(); ++i) {
        auto *st = stateForIndex(i);
        if (!st || !isOverlayMode(st->mode))
            continue;

        if (st->isExpanded || (st->animation && st->animation->state() == QAbstractAnimation::Running)) {
            hasActiveOverlay = true;
            if (st->viewportWrapper) {
                st->viewportWrapper->setOverlayActive(true);
                const int w = st->currentAnimatedLength;
                st->viewportWrapper->setGeometry(0, 0, w, height());
                st->viewportWrapper->raise();
            }
        } else {
            if (st->viewportWrapper) {
                st->viewportWrapper->setOverlayActive(false);
                if (st->mode == SplitPaneDisplayMode::Overlay) {
                    st->viewportWrapper->hide();
                } else {
                    st->viewportWrapper->setGeometry(0, 0, st->compactLength, height());
                }
            }
        }
    }

    if (m_lightDismissOverlay) {
        if (hasActiveOverlay && m_lightDismissEnabled) {
            m_lightDismissOverlay->setGeometry(rect());
            m_lightDismissOverlay->show();
            m_lightDismissOverlay->raise();

            // 确保浮层面板始终位于遮罩之上
            for (int i = 0; i < paneCount(); ++i) {
                auto *st = stateForIndex(i);
                if (st && isOverlayMode(st->mode) && st->viewportWrapper &&
                    (st->isExpanded || (st->animation && st->animation->state() == QAbstractAnimation::Running))) {
                    st->viewportWrapper->raise();
                }
            }
        } else {
            m_lightDismissOverlay->hide();
        }
    }
}

void CollapsibleSplitView::animateTo(int index, int targetLength, bool opening) {
    auto *st = stateForIndex(index);
    if (!st || !st->animation)
        return;

    if (opening && st->viewportWrapper) {
        st->viewportWrapper->show();
    }

    st->animationOpening = opening;
    st->animation->stop();

    st->animation->setDuration(opening ? kOpenDurationMs : kCloseDurationMs);

    const int startVal = isOverlayMode(st->mode) ? st->currentAnimatedLength : panePreferredSize(index);
    st->animation->setStartValue(startVal);
    st->animation->setEndValue(targetLength);
    st->animation->start();
}

void CollapsibleSplitView::finishPaneAnimation(int index) {
    auto *st = stateForIndex(index);
    if (!st)
        return;

    const int finalSize = isOverlayMode(st->mode)
                              ? (st->isExpanded ? st->openLength : collapsedLength(*st))
                              : panePreferredSize(index);

    st->currentAnimatedLength = finalSize;

    if (auto *w = qobject_cast<SlideViewportContainer *>(st->viewportWrapper.data())) {
        w->setCommittedLength(finalSize, orientation());
        if (!st->isExpanded && collapsedLength(*st) == 0) {
            w->hide();
        }
    }

    if (isOverlayMode(st->mode)) {
        updateOverlayLayout();
        if (!st->isExpanded && m_lightDismissOverlay) {
            m_lightDismissOverlay->setOpacity(0.0);
            m_lightDismissOverlay->hide();
        }
    }

    updatePaneConstraints(index);

    if (st->animationOpening)
        emit paneOpened(index);
    else
        emit paneClosed(index);

    st->animationOpening = false;
}

void CollapsibleSplitView::onPaneSizeChanged(int index, int size) {
    if (m_suppressSizeMemory > 0)
        return;

    auto *st = stateForIndex(index);
    if (!st || isOverlayMode(st->mode))
        return;

    if (st->animation && st->animation->state() == QAbstractAnimation::Running)
        return;

    // 仅在非悬浮展开态下记忆用户手动拖拽的新宽度
    if (st->isExpanded && size > st->compactLength) {
        st->openLength = qBound(st->minOpenLength, size, st->maxOpenLength);
        st->currentAnimatedLength = st->openLength;
        if (auto *w = qobject_cast<SlideViewportContainer *>(st->viewportWrapper.data())) {
            w->setCommittedLength(st->openLength, orientation());
        }
    }
}

void CollapsibleSplitView::onPaneDestroyed(QObject *obj) {
    for (auto it = m_paneStates.begin(); it != m_paneStates.end(); ++it) {
        if (it.key() == obj || it.value().viewportWrapper == obj || it.value().userWidget == obj) {
            if (it.value().animation) {
                it.value().animation->stop();
            }
            m_paneStates.erase(it);
            break;
        }
    }
    updateOverlayLayout();
}

void CollapsibleSplitView::handleLightDismissClicked() {
    if (!m_lightDismissEnabled)
        return;

    // 点击外部空白遮罩，自动收起处于打开态的悬浮面板
    for (int i = 0; i < paneCount(); ++i) {
        auto *st = stateForIndex(i);
        if (st && isOverlayMode(st->mode) && st->isExpanded) {
            setPaneExpanded(i, false, true);
        }
    }
}

bool CollapsibleSplitView::eventFilter(QObject *watched, QEvent *event) {
    if (!resizing() && watched != this && watched->isWidgetType()) {
        const auto type = event->type();
        if (type == QEvent::MouseMove || type == QEvent::Enter) {
            // 当鼠标移入子面板内部时，通知底层 SplitView 进行碰撞检测并清除手柄 hover 状态与光标
            const QPoint localPos = mapFromGlobal(QCursor::pos());
            QMouseEvent syntheticEvent(
                QEvent::MouseMove,
                localPos,
                QCursor::pos(),
                Qt::NoButton,
                Qt::NoButton,
                Qt::NoModifier);
            fluent::collections::SplitView::mouseMoveEvent(&syntheticEvent);
        }
    }
    return fluent::collections::SplitView::eventFilter(watched, event);
}

void CollapsibleSplitView::leaveEvent(QEvent *event) {
    fluent::collections::SplitView::leaveEvent(event);
    if (!resizing()) {
        unsetCursor();
    }
}

void CollapsibleSplitView::resizeEvent(QResizeEvent *event) {
    fluent::collections::SplitView::resizeEvent(event);
    updateOverlayLayout();
}

} // namespace ui::widget

#include "CollapsibleSplitView.moc"
