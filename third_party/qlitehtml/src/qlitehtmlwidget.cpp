#include "qlitehtmlwidget.h"

#include "container/container_qpainter.h"

#include <litehtml/master_css.h>

#include <QClipboard>
#include <QDebug>
#include <QGuiApplication>
#include <QMimeData>
#include <QPaintEvent>
#include <QPainter>
#include <QRegion>
#include <QScrollBar>
#include <QScroller>
#include <QStyle>
#include <QTimer>
#include <QVariantAnimation>
#include <QWheelEvent>

const int kScrollBarStep = 40;

// Master stylesheet: litehtml v0.10 ships its own built-in master CSS
// (litehtml::master_css), used by document::createFromString when the
// container context has no custom master style sheet.
class QLiteHtmlWidgetPrivate
{
public:
    QString html;
    DocumentContainerContext context;
    QUrl url;
    DocumentContainer documentContainer;
    qreal zoomFactor = 1;
    QUrl lastHighlightedLink;
    QTimer selectionScrollTimer;
    QTimer renderTimer;
    QPoint selectionDragPosition;
    
    QVariantAnimation *smoothScrollAnim = nullptr;
    QPoint targetScrollValue;
    
    bool isRendering = false;
    bool ignoreScrollbarWheel = false;
};

QLiteHtmlWidget::QLiteHtmlWidget(QWidget *parent)
    : QAbstractScrollArea(parent)
    , d(new QLiteHtmlWidgetPrivate)
{
    setMouseTracking(true);
    horizontalScrollBar()->setSingleStep(kScrollBarStep);
    verticalScrollBar()->setSingleStep(kScrollBarStep);
    
    horizontalScrollBar()->installEventFilter(this);
    verticalScrollBar()->installEventFilter(this);
    
    // Enable modern kinetic scrolling for touchpads and touchscreens
    QScroller::grabGesture(viewport(), QScroller::TouchGesture);

    connect(verticalScrollBar(), &QScrollBar::sliderPressed, this, [this] {
        if (d->smoothScrollAnim) d->smoothScrollAnim->stop();
    });
    connect(horizontalScrollBar(), &QScrollBar::sliderPressed, this, [this] {
        if (d->smoothScrollAnim) d->smoothScrollAnim->stop();
    });

    d->documentContainer.setCursorCallback([this](const QCursor &c) { viewport()->setCursor(c); });
    d->documentContainer.setPaletteCallback([this] { return palette(); });
    d->documentContainer.setLinkCallback([this](const QUrl &url) {
        QUrl fullUrl = url;
        if (url.isRelative() && url.path(QUrl::FullyEncoded).isEmpty()) { // fragment/anchor only
            fullUrl = d->url;
            fullUrl.setFragment(url.fragment(QUrl::FullyEncoded));
        }
        // delay because document may not be changed directly during this callback
        QMetaObject::invokeMethod(
            this, [this, fullUrl] { emit linkClicked(fullUrl); }, Qt::QueuedConnection);
    });
    d->documentContainer.setClipboardCallback([this](bool yes) { emit copyAvailable(yes); });
    d->documentContainer.setFormControlCallback([this](const QString &tag, const QString &type, const QString &name, const QString &value, bool checked) {
        emit formControlActivated(tag, type, name, value, checked);
    });
    d->documentContainer.setDetailsCallback([this](const QString &id, bool open) {
        const QSize vViewportSize = toVirtual(viewport()->size());
        const int fullWidth = width() / d->zoomFactor;
        const int scrollbarWidth = style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this);
        const int w = fullWidth - scrollbarWidth - 2;
        horizontalScrollBar()->setRange(0, std::max(0, d->documentContainer.documentWidth() - w));
        verticalScrollBar()->setRange(0, std::max(0, d->documentContainer.documentHeight() - vViewportSize.height()));
        viewport()->update();
        emit detailsToggled(id, open);
    });
    d->documentContainer.setRepaintCallback([this] { viewport()->update(); });
    d->selectionScrollTimer.setInterval(30);
    connect(&d->selectionScrollTimer, &QTimer::timeout, this, &QLiteHtmlWidget::scrollSelection);

    d->renderTimer.setInterval(16);
    d->renderTimer.setSingleShot(true);
    connect(&d->renderTimer, &QTimer::timeout, this, &QLiteHtmlWidget::render);

    // Default to litehtml v0.10's built-in master stylesheet, plus form control and details UA styles
    QString customMasterCss = QString::fromUtf8(litehtml::master_css) + R"(
html, body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "Microsoft YaHei", sans-serif;
}
input[type="button"], button, input[type="submit"], input[type="reset"] {
    padding: 2px 6px;
    background-color: #e0e0e0;
    border: 1px solid #a0a0a0;
    border-radius: 4px;
    color: #000000;
    text-align: center;
    display: inline-block;
    text-decoration: none;
}
input[type="button"]:hover, button:hover, input[type="submit"]:hover, input[type="reset"]:hover {
    background-color: #d0d0d0;
}
input[type="button"]:active, button:active, input[type="submit"]:active, input[type="reset"]:active {
    background-color: #c0c0c0;
}
details {
    display: block;
}
details > summary {
    display: block;
    cursor: pointer;
    padding-left: 18px;
    list-style-type: disc;
}
details:not([open]) > :not(summary) {
    display: none;
}
@media print {
    .__qlh_dummy_print { color: red; }
}
)";
    d->context.setMasterStyleSheet(customMasterCss);
}

QLiteHtmlWidget::~QLiteHtmlWidget()
{
    delete d;
}

void QLiteHtmlWidget::setUrl(const QUrl &url)
{
    d->url = url;
    QUrl baseUrl = url;
    baseUrl.setFragment({});
    const QString path = baseUrl.path(QUrl::FullyEncoded);
    const int lastSlash = path.lastIndexOf('/');
    const QString basePath = lastSlash >= 0 ? path.left(lastSlash) : QString();
    baseUrl.setPath(basePath);
    d->documentContainer.setBaseUrl(baseUrl.toString(QUrl::FullyEncoded));
    QMetaObject::invokeMethod(this, [this] { updateHightlightedLink(); }, Qt::QueuedConnection);
}

QUrl QLiteHtmlWidget::url() const
{
    return d->url;
}

void QLiteHtmlWidget::setHtml(const QString &content)
{
    d->html = content;
    d->documentContainer.setPaintDevice(viewport());
    d->documentContainer.setDocument(content.toUtf8(), &d->context);
    verticalScrollBar()->setValue(0);
    horizontalScrollBar()->setValue(0);
    render();
    QMetaObject::invokeMethod(this, [this] { updateHightlightedLink(); }, Qt::QueuedConnection);
}

void QLiteHtmlWidget::appendHtml(const QString &content)
{
    d->html += content;
    d->documentContainer.setPaintDevice(viewport());
    d->documentContainer.appendHtml(content.toUtf8());
    if (!d->renderTimer.isActive()) {
        d->renderTimer.start();
    }
}

QString QLiteHtmlWidget::html() const
{
    return d->html;
}

QString QLiteHtmlWidget::title() const
{
    return d->documentContainer.caption();
}

void QLiteHtmlWidget::setZoomFactor(qreal scale)
{
    Q_ASSERT(scale != 0);
    d->zoomFactor = scale;
    if (d->smoothScrollAnim) d->smoothScrollAnim->stop();
    withFixedTextPosition([this] { render(); });
}

qreal QLiteHtmlWidget::zoomFactor() const
{
    return d->zoomFactor;
}

bool QLiteHtmlWidget::findText(const QString &text,
                               QTextDocument::FindFlags flags,
                               bool incremental,
                               bool *wrapped)
{
    bool success = false;
    QVector<QRect> oldSelection;
    QVector<QRect> newSelection;
    d->documentContainer
        .findText(text, flags, incremental, wrapped, &success, &oldSelection, &newSelection);
    // scroll to search result position and/or redraw as necessary
    QRect newSelectionCombined;
    for (const QRect &r : std::as_const(newSelection))
        newSelectionCombined = newSelectionCombined.united(r);
    QScrollBar *vBar = verticalScrollBar();
    const int top = newSelectionCombined.top();
    const int bottom = newSelectionCombined.bottom() - toVirtual(viewport()->size()).height();
    if (success && top < vBar->value() && vBar->minimum() <= top) {
        vBar->setValue(top);
    } else if (success && vBar->value() < bottom && bottom <= vBar->maximum()) {
        vBar->setValue(bottom);
    } else {
        viewport()->update(fromVirtual(newSelectionCombined.translated(-scrollPosition())));
        for (const QRect &r : std::as_const(oldSelection))
            viewport()->update(fromVirtual(r.translated(-scrollPosition())));
    }
    return success;
}

void QLiteHtmlWidget::setDefaultFont(const QFont &font)
{
    withFixedTextPosition([this, &font] {
        d->documentContainer.setDefaultFont(font);
        render();
    });
}

QFont QLiteHtmlWidget::defaultFont() const
{
    return d->documentContainer.defaultFont();
}

void QLiteHtmlWidget::scrollToAnchor(const QString &name)
{
    if (!d->documentContainer.hasDocument())
        return;
    horizontalScrollBar()->setValue(0);
    if (name.isEmpty()) {
        smoothScrollTo(QPoint(horizontalScrollBar()->value(), 0));
        return;
    }
    const int y = d->documentContainer.anchorY(name);
    if (y >= 0)
        smoothScrollTo(QPoint(horizontalScrollBar()->value(), std::min(y, verticalScrollBar()->maximum())));
}

void QLiteHtmlWidget::setResourceHandler(const QLiteHtmlWidget::ResourceHandler &handler)
{
    d->documentContainer.setResourceHandler(handler);
}

void QLiteHtmlWidget::setAllowNetworkAccess(bool allow)
{
    d->documentContainer.setAllowNetworkAccess(allow);
}

bool QLiteHtmlWidget::allowNetworkAccess() const
{
    return d->documentContainer.allowNetworkAccess();
}

void QLiteHtmlWidget::clearResourceCache()
{
    d->documentContainer.clearResourceCache();
}

QString QLiteHtmlWidget::selectedText() const
{
    return d->documentContainer.selectedText();
}

QString QLiteHtmlWidget::selectedHtml() const
{
    return d->documentContainer.selectedHtml();
}

void QLiteHtmlWidget::paintEvent(QPaintEvent *event)
{
    if (!d->documentContainer.hasDocument())
        return;
    d->documentContainer.setScrollPosition(scrollPosition());
    QPainter p(viewport());
    p.setWorldTransform(QTransform().scale(d->zoomFactor, d->zoomFactor));
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setRenderHint(QPainter::Antialiasing, true);
    d->documentContainer.draw(&p, toVirtual(event->rect()));
}

void QLiteHtmlWidget::resizeEvent(QResizeEvent *event)
{
    withFixedTextPosition([this, event] {
        QAbstractScrollArea::resizeEvent(event);
        render();
    });
}

void QLiteHtmlWidget::scrollContentsBy(int dx, int dy)
{
    if (d->documentContainer.hasDocument()) {
        // Modern Qt recommendation: avoid viewport()->scroll() for complex custom painting
        // as it causes severe tearing on high-DPI displays and Wayland/macOS.
        // Modern CPUs/GPUs are fast enough to just repaint the viewport.
        viewport()->update();
    } else {
        QAbstractScrollArea::scrollContentsBy(dx, dy);
    }
}

void QLiteHtmlWidget::wheelEvent(QWheelEvent *event)
{
    QPoint viewportPos;
    QPoint documentPos;
    htmlPos(event->position().toPoint(), &viewportPos, &documentPos);

    QPointF virtualDelta;
    bool isPixelDelta = !event->pixelDelta().isNull();
    
    if (isPixelDelta) {
        // Touchpad pixel delta. Positive means scrolling left/up.
        virtualDelta = QPointF(event->pixelDelta()) / d->zoomFactor;
    } else {
        // Mouse wheel angle delta. 120 per notch.
        QPoint angle = event->angleDelta();
        if (event->modifiers() & Qt::ShiftModifier) {
            // Shift + wheel prefers horizontal scroll
            if (angle.x() == 0 && angle.y() != 0) {
                angle = QPoint(angle.y(), 0);
            }
        }
        // Map 120 angle delta to singleStep * 3 (standard wheel distance)
        int stepY = verticalScrollBar()->singleStep() * 3;
        int stepX = horizontalScrollBar()->singleStep() * 3;
        virtualDelta = QPointF(angle.x() * stepX / 120.0, angle.y() * stepY / 120.0) / d->zoomFactor;
    }
    
    // Qt delta is positive for scrolling UP/LEFT. litehtml expects positive dx/dy for scrolling DOWN/RIGHT.
    QPoint internalDelta = (-virtualDelta).toPoint();
    
    if (!internalDelta.isNull()) {
        const QVector<QRect> consumedRects = d->documentContainer.scrollAt(documentPos, viewportPos, internalDelta);
        if (!consumedRects.isEmpty()) {
            if (d->smoothScrollAnim && d->smoothScrollAnim->state() == QAbstractAnimation::Running) {
                d->smoothScrollAnim->stop();
            }
            for (const QRect &r : consumedRects) {
                viewport()->update(fromVirtual(r.translated(-scrollPosition())));
            }
            event->accept();
            return;
        }
    }
    
    // Not handled by internal overflow, hand off to outer area
    if (isPixelDelta) {
        d->ignoreScrollbarWheel = true;
        QAbstractScrollArea::wheelEvent(event);
        d->ignoreScrollbarWheel = false;
        return;
    }
    
    // Smooth scrolling for traditional mouse wheel
    int numDegreesY = event->angleDelta().y() / 8;
    int numStepsY = numDegreesY / 15;
    
    int numDegreesX = event->angleDelta().x() / 8;
    int numStepsX = numDegreesX / 15;
    
    if (numStepsY == 0 && numStepsX == 0) {
        d->ignoreScrollbarWheel = true;
        QAbstractScrollArea::wheelEvent(event);
        d->ignoreScrollbarWheel = false;
        return;
    }
    
    QPoint targetPos(horizontalScrollBar()->value(), verticalScrollBar()->value());
    if (d->smoothScrollAnim && d->smoothScrollAnim->state() == QAbstractAnimation::Running) {
        targetPos = d->targetScrollValue;
    }
    
    targetPos.rx() -= numStepsX * horizontalScrollBar()->singleStep() * 3;
    targetPos.ry() -= numStepsY * verticalScrollBar()->singleStep() * 3;
    
    smoothScrollTo(targetPos);
    
    event->accept();
}

void QLiteHtmlWidget::mouseMoveEvent(QMouseEvent *event)
{
    d->selectionDragPosition = event->pos();
    const bool scrollSelection = event->buttons().testFlag(Qt::LeftButton)
                                 && !viewport()->geometry().contains(event->pos());
    if (scrollSelection && !d->selectionScrollTimer.isActive())
        d->selectionScrollTimer.start();
    else if (!scrollSelection)
        d->selectionScrollTimer.stop();

    updateSelection(event->pos());

    updateHightlightedLink();
}

void QLiteHtmlWidget::mousePressEvent(QMouseEvent *event)
{
    QPoint viewportPos;
    QPoint pos;
    htmlPos(event->pos(), &viewportPos, &pos);
    const QVector<QRect> areas = d->documentContainer.mousePressEvent(pos,
                                                                      viewportPos,
                                                                      event->button(),
                                                                      event->modifiers());
    for (const QRect &r : areas)
        viewport()->update(fromVirtual(r.translated(-scrollPosition())));
}

void QLiteHtmlWidget::mouseReleaseEvent(QMouseEvent *event)
{
    d->selectionScrollTimer.stop();
    QPoint viewportPos;
    QPoint pos;
    htmlPos(event->pos(), &viewportPos, &pos);
    const QVector<QRect> areas = d->documentContainer.mouseReleaseEvent(pos,
                                                                        viewportPos,
                                                                        event->button());
    for (const QRect &r : areas)
        viewport()->update(fromVirtual(r.translated(-scrollPosition())));
}

void QLiteHtmlWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    QPoint viewportPos;
    QPoint pos;
    htmlPos(event->pos(), &viewportPos, &pos);
    const QVector<QRect> areas = d->documentContainer.mouseDoubleClickEvent(pos,
                                                                            viewportPos,
                                                                            event->button());
    for (const QRect &r : areas) {
        viewport()->update(fromVirtual(r.translated(-scrollPosition())));
    }
}

void QLiteHtmlWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    const QVector<QRect> areas = d->documentContainer.leaveEvent();
    for (const QRect &r : areas)
        viewport()->update(fromVirtual(r.translated(-scrollPosition())));
    setHightlightedLink(QUrl());
}

void QLiteHtmlWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QPoint viewportPos;
    QPoint pos;
    htmlPos(event->pos(), &viewportPos, &pos);
    emit contextMenuRequested(event->pos(),
                              d->documentContainer.linkAt(pos, viewportPos),
                              d->documentContainer.imageAt(pos, viewportPos));
}

static QAbstractSlider::SliderAction getSliderAction(int key)
{
    if (key == Qt::Key_Home)
        return QAbstractSlider::SliderToMinimum;
    if (key == Qt::Key_End)
        return QAbstractSlider::SliderToMaximum;
    if (key == Qt::Key_PageUp)
        return QAbstractSlider::SliderPageStepSub;
    if (key == Qt::Key_PageDown)
        return QAbstractSlider::SliderPageStepAdd;
    if (key == Qt::Key_Up)
        return QAbstractSlider::SliderSingleStepSub;
    if (key == Qt::Key_Down)
        return QAbstractSlider::SliderSingleStepAdd;
    return QAbstractSlider::SliderNoAction;
}

void QLiteHtmlWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() == Qt::NoModifier || event->modifiers() == Qt::KeypadModifier) {
        const bool isLeft = (event->key() == Qt::Key_Left);
        const bool isRight = (event->key() == Qt::Key_Right);
        const QAbstractSlider::SliderAction sliderAction = getSliderAction(event->key());
        
        if (sliderAction != QAbstractSlider::SliderNoAction || isLeft || isRight) {
            QPoint targetPos(horizontalScrollBar()->value(), verticalScrollBar()->value());
            if (d->smoothScrollAnim && d->smoothScrollAnim->state() == QAbstractAnimation::Running) {
                targetPos = d->targetScrollValue;
            }
            
            switch (sliderAction) {
                case QAbstractSlider::SliderToMinimum:
                    targetPos.setY(verticalScrollBar()->minimum());
                    break;
                case QAbstractSlider::SliderToMaximum:
                    targetPos.setY(verticalScrollBar()->maximum());
                    break;
                case QAbstractSlider::SliderPageStepSub:
                    targetPos.ry() -= verticalScrollBar()->pageStep();
                    break;
                case QAbstractSlider::SliderPageStepAdd:
                    targetPos.ry() += verticalScrollBar()->pageStep();
                    break;
                case QAbstractSlider::SliderSingleStepSub:
                    targetPos.ry() -= verticalScrollBar()->singleStep();
                    break;
                case QAbstractSlider::SliderSingleStepAdd:
                    targetPos.ry() += verticalScrollBar()->singleStep();
                    break;
                default:
                    break;
            }
            
            if (isLeft) targetPos.rx() -= horizontalScrollBar()->singleStep();
            if (isRight) targetPos.rx() += horizontalScrollBar()->singleStep();
            
            smoothScrollTo(targetPos);
            event->accept();
            return;
        }
    } else if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_C) {
        // Copy selected text to clipboard when Ctrl+C is pressed
        const QString text = selectedText();
        if (!text.isEmpty()) {
            auto *mimeData = new QMimeData();
            mimeData->setText(text);
            // Also copy HTML if available
            const QString html = selectedHtml();

            if (!html.isEmpty()) {
                mimeData->setHtml(html);
            }
            QGuiApplication::clipboard()->setMimeData(mimeData);
        }
        event->accept();
        return;
    }

    QAbstractScrollArea::keyPressEvent(event);
}

void QLiteHtmlWidget::changeEvent(QEvent *event)
{
    // Re-layout on DPI, theme, or font changes
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange || 
        event->type() == QEvent::FontChange || event->type() == QEvent::ScreenChangeInternal) {
        if (d->documentContainer.hasDocument()) {
            if (!d->renderTimer.isActive()) {
                d->renderTimer.start();
            }
        }
    }
    QAbstractScrollArea::changeEvent(event);
}

bool QLiteHtmlWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (!d->ignoreScrollbarWheel && (obj == verticalScrollBar() || obj == horizontalScrollBar()) && event->type() == QEvent::Wheel) {
        auto *wheelEvent = static_cast<QWheelEvent *>(event);
        
        QPoint pixelDelta = wheelEvent->pixelDelta();
        QPoint angleDelta = wheelEvent->angleDelta();
        
        // Native Qt behavior: if hovering over horizontal scrollbar, treat vertical scroll as horizontal
        if (obj == horizontalScrollBar() && angleDelta.x() == 0) {
            angleDelta = QPoint(angleDelta.y(), 0);
            pixelDelta = QPoint(pixelDelta.y(), 0);
        }
        
        QWheelEvent clonedEvent(
            this->mapFromGlobal(wheelEvent->globalPosition()), 
            wheelEvent->globalPosition(),
            pixelDelta, angleDelta,
            wheelEvent->buttons(), wheelEvent->modifiers(),
            wheelEvent->phase(), wheelEvent->inverted(), wheelEvent->source()
        );
        this->wheelEvent(&clonedEvent);
        return true;
    }
    return QAbstractScrollArea::eventFilter(obj, event);
}

void QLiteHtmlWidget::updateHightlightedLink()
{
    QPoint viewportPos;
    QPoint pos;
    htmlPos(mapFromGlobal(QCursor::pos()), &viewportPos, &pos);
    setHightlightedLink(d->documentContainer.linkAt(pos, viewportPos));
}

void QLiteHtmlWidget::setHightlightedLink(const QUrl &url)
{
    if (d->lastHighlightedLink == url)
        return;
    d->lastHighlightedLink = url;
    emit linkHighlighted(d->lastHighlightedLink);
}

void QLiteHtmlWidget::updateSelection(const QPoint &position)
{
    QPoint viewportPos;
    QPoint documentPos;
    htmlPos(position, &viewportPos, &documentPos);
    const QVector<QRect> areas = d->documentContainer.mouseMoveEvent(documentPos, viewportPos);
    QRegion dirtyRegion;
    for (const QRect &area : areas)
        dirtyRegion += fromVirtual(area.translated(-scrollPosition()));
    if (!dirtyRegion.isEmpty())
        viewport()->update(dirtyRegion);
}

void QLiteHtmlWidget::scrollSelection()
{
    const QRect viewportRect = viewport()->geometry();
    const QPoint position = d->selectionDragPosition;
    const auto scrollBar = [](QScrollBar *bar, int distance) {
        const int delta = qBound(-kScrollBarStep, distance, kScrollBarStep);
        bar->setValue(bar->value() + delta);
    };

    const int horizontalDistance = position.x() < viewportRect.left()
                                       ? position.x() - viewportRect.left()
                                   : position.x() > viewportRect.right()
                                       ? position.x() - viewportRect.right()
                                       : 0;
    const int verticalDistance = position.y() < viewportRect.top()
                                     ? position.y() - viewportRect.top()
                                 : position.y() > viewportRect.bottom()
                                     ? position.y() - viewportRect.bottom()
                                     : 0;
    scrollBar(horizontalScrollBar(), horizontalDistance);
    scrollBar(verticalScrollBar(), verticalDistance);

    updateSelection({qBound(viewportRect.left(), position.x(), viewportRect.right()),
                     qBound(viewportRect.top(), position.y(), viewportRect.bottom())});
}

void QLiteHtmlWidget::withFixedTextPosition(const std::function<void()> &action)
{
    // remember element to which to scroll after re-rendering
    QPoint viewportPos;
    QPoint pos;
    htmlPos({}, &viewportPos, &pos); // top-left
    const int y = d->documentContainer.withFixedElementPosition(pos.y(), action);
    if (y >= 0)
        verticalScrollBar()->setValue(std::min(y, verticalScrollBar()->maximum()));
}

void QLiteHtmlWidget::render()
{
    if (!d->documentContainer.hasDocument() || d->isRendering)
        return;

    d->isRendering = true;

    // 1. Initial size estimation. 
    // Fallback if viewport width is 0 to prevent 0-width layout freeze.
    int physicalWidth = viewport()->width();
    if (physicalWidth <= 0) {
        physicalWidth = std::max(10, width() - style()->pixelMetric(QStyle::PM_ScrollBarExtent));
    }
    
    int w = toVirtual(QPoint(physicalWidth, 0)).x();
    int h = toVirtual(viewport()->size()).height();

    d->documentContainer.render(w, h);

    horizontalScrollBar()->setPageStep(w);
    horizontalScrollBar()->setRange(0, std::max(0, d->documentContainer.documentWidth() - w));
    verticalScrollBar()->setPageStep(h);
    verticalScrollBar()->setRange(0, std::max(0, d->documentContainer.documentHeight() - h));

    // 2. Two-Pass Layout:
    // If scrollbar visibility changes, the viewport width updates synchronously.
    // Perform a second layout pass to fit the updated viewport bounds exactly.
    if (viewport()->width() > 0 && viewport()->width() != physicalWidth) {
        physicalWidth = viewport()->width();
        w = toVirtual(QPoint(physicalWidth, 0)).x();
        h = toVirtual(viewport()->size()).height();
        
        d->documentContainer.render(w, h);
        
        horizontalScrollBar()->setPageStep(w);
        horizontalScrollBar()->setRange(0, std::max(0, d->documentContainer.documentWidth() - w));
        verticalScrollBar()->setPageStep(h);
        verticalScrollBar()->setRange(0, std::max(0, d->documentContainer.documentHeight() - h));
    }
    
    viewport()->update();
    d->isRendering = false;
}

void QLiteHtmlWidget::smoothScrollTo(const QPoint &target)
{
    d->targetScrollValue.setX(qBound(horizontalScrollBar()->minimum(), target.x(), horizontalScrollBar()->maximum()));
    d->targetScrollValue.setY(qBound(verticalScrollBar()->minimum(), target.y(), verticalScrollBar()->maximum()));
    
    if (!d->smoothScrollAnim) {
        d->smoothScrollAnim = new QVariantAnimation(this);
        d->smoothScrollAnim->setEasingCurve(QEasingCurve::OutCubic);
        d->smoothScrollAnim->setDuration(250);
        connect(d->smoothScrollAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            const QPoint p = value.toPoint();
            horizontalScrollBar()->setValue(p.x());
            verticalScrollBar()->setValue(p.y());
        });
    }
    
    d->smoothScrollAnim->stop();
    d->smoothScrollAnim->setStartValue(QPoint(horizontalScrollBar()->value(), verticalScrollBar()->value()));
    d->smoothScrollAnim->setEndValue(d->targetScrollValue);
    d->smoothScrollAnim->start();
}

QPoint QLiteHtmlWidget::scrollPosition() const
{
    return {horizontalScrollBar()->value(), verticalScrollBar()->value()};
}

void QLiteHtmlWidget::htmlPos(const QPoint &pos, QPoint *viewportPos, QPoint *htmlPos) const
{
    *viewportPos = toVirtual(viewport()->mapFromParent(pos));
    *htmlPos = *viewportPos + scrollPosition();
}

QPoint QLiteHtmlWidget::toVirtual(const QPoint &p) const
{
    return {qRound(p.x() / d->zoomFactor), qRound(p.y() / d->zoomFactor)};
}

QSize QLiteHtmlWidget::toVirtual(const QSize &s) const
{
    return {qRound(s.width() / d->zoomFactor), qRound(s.height() / d->zoomFactor)};
}

QRect QLiteHtmlWidget::toVirtual(const QRect &r) const
{
    QRectF virtualRect(r.x() / d->zoomFactor,
                       r.y() / d->zoomFactor,
                       r.width() / d->zoomFactor,
                       r.height() / d->zoomFactor);
    return virtualRect.toAlignedRect();
}

QRect QLiteHtmlWidget::fromVirtual(const QRect &r) const
{
    QRectF physicalRect(r.x() * d->zoomFactor,
                        r.y() * d->zoomFactor,
                        r.width() * d->zoomFactor,
                        r.height() * d->zoomFactor);
    return physicalRect.toAlignedRect();
}
