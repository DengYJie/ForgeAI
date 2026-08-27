#include "MarkdownView.h"

#include <QClipboard>
#include <QElapsedTimer>
#include <QEasingCurve>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMimeData>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QRegularExpression>
#include <QScrollBar>
#include <QShowEvent>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <FluentQt/Foundation.h>

namespace ui::widget {

MarkdownView::MarkdownView(QWidget *parent)
    : QAbstractScrollArea(parent)
    , m_theme(fluent::FluentElement::currentTheme() == fluent::FluentElement::Dark
          ? ui::markdown::MarkdownTheme::dark(font())
          : ui::markdown::MarkdownTheme::light(font()))
    , m_resources(this)
{
    m_controller = new MarkdownDocumentController(this);
    m_layoutCache = new MarkdownDocumentLayout(m_controller, this);
    m_eventFilter = new MarkdownViewEventFilter(viewport(), this);

    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFocusPolicy(Qt::ClickFocus);
    viewport()->setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_layoutCache->setTheme(m_theme);
    m_layoutCache->setImages(&m_resources.images());

    connect(m_controller, &MarkdownDocumentController::streamingChanged, this, &MarkdownView::streamingChanged);
    connect(m_controller, &MarkdownDocumentController::taskToggled, this, &MarkdownView::taskToggled);

    connect(m_layoutCache, &MarkdownDocumentLayout::layoutReady, this, &MarkdownView::onLayoutReady);

    connect(m_eventFilter, &MarkdownViewEventFilter::repaintRequested, this, &MarkdownView::onRepaintRequested);
    connect(m_eventFilter, &MarkdownViewEventFilter::linkActivated, this, [this](const QUrl& url) {
        if (url.toString().startsWith(u'#') || (url.scheme().isEmpty() && !url.fragment().isEmpty())) {
            scrollToAnchor(url.fragment().isEmpty() ? url.toString().mid(1) : url.fragment());
        }
        emit linkActivated(url);
    });
    connect(m_eventFilter, &MarkdownViewEventFilter::linkHighlighted, this, &MarkdownView::linkHighlighted);
    connect(m_eventFilter, &MarkdownViewEventFilter::selectionChanged, this, &MarkdownView::selectionChanged);
    connect(m_eventFilter, &MarkdownViewEventFilter::imageActivated, this, &MarkdownView::imageActivated);
    connect(m_eventFilter, &MarkdownViewEventFilter::contextMenuRequested, this, &MarkdownView::contextMenuRequested);
    connect(m_eventFilter, &MarkdownViewEventFilter::copySelectionRequested, this, &MarkdownView::copy);
    connect(m_eventFilter, &MarkdownViewEventFilter::selectAllRequested, this, &MarkdownView::selectAll);
    connect(m_eventFilter, &MarkdownViewEventFilter::taskToggleRequested, this, &MarkdownView::onTaskToggleRequested);
    connect(m_eventFilter, &MarkdownViewEventFilter::cursorChanged, this, [this](Qt::CursorShape shape) {
        viewport()->setCursor(shape);
    });
    connect(m_eventFilter, &MarkdownViewEventFilter::blockScrollRequested, this, [this](int idx, qreal dx, qreal dy, bool smooth) {
        if (idx < 0 || idx >= m_documentLayout->blockCount()) return;
        const auto& block = m_documentLayout->blockAt(idx);
        scrollBlock(block.blockId, block.elementId, dx, dy, smooth);
    });

    connect(&m_resources, &ui::markdown::MarkdownImageResourceManager::imageUpdated, this, [this](const QString& source) {
        const auto& images = m_resources.images();
        if (const auto it = images.constFind(source); it != images.constEnd() && !it->isNull()) {
            if (m_layoutCache->updateImageSize(source, it->size())) {
                return;
            }
        }
        m_layoutCache->forceRelayout();
    });

    m_eventFilter->setDocumentLayoutGetter([this] { return m_documentLayout.get(); });
    m_eventFilter->setScrollOffsets(&m_blockScrollOffsets);
    m_eventFilter->setScrollBarValueGetter([this] { return m_autoFitHeight ? 0 : verticalScrollBar()->value(); });
}

MarkdownView::~MarkdownView() = default;

void MarkdownView::setMarkdown(const QString &markdown)
{
    m_controller->setMarkdown(markdown);
}

QString MarkdownView::markdown() const { return m_controller->markdown(); }

void MarkdownView::clear() { setMarkdown({}); }

void MarkdownView::beginStream()
{
    m_metrics = {};
    m_documentLayout = std::make_shared<ui::markdown::DocumentLayout>();
    m_preferredContentSize = QSizeF();
    m_preferredSizeDirty = true;
    m_lastAutoFitHeight = 0;
    // blockIndex scroll offsets are keyed by index, which changes across streaming sessions.
    // Clear them to prevent old offsets from mapping to wrong blocks in the new session.
    for (auto* anim : m_blockScrollAnimations) { anim->stop(); anim->deleteLater(); }
    m_blockScrollAnimations.clear();
    m_blockScrollOffsets.clear();
    m_blockTargetScrollOffsets.clear();
    if (verticalScrollBar()) {
        verticalScrollBar()->setValue(0);
    }
    m_controller->beginStream();
    m_wasStreaming = true;
    updateAutoFitHeight();
    viewport()->update();
}

void MarkdownView::appendMarkdown(const QString &chunk) { m_controller->appendMarkdown(chunk); }
void MarkdownView::appendStreamingText(const QString &chunk) { m_controller->appendMarkdown(chunk); }
void MarkdownView::finishStream() { m_controller->finishStream(); }
void MarkdownView::finishStreaming() { m_controller->finishStream(); }
bool MarkdownView::isStreaming() const { return m_controller->isStreaming(); }

MarkdownViewMetrics MarkdownView::metrics() const noexcept
{
    auto m = m_metrics;
    if (m_controller) {
        const auto cm = m_controller->metrics();
        m.parseCount = cm.parseCount;
        m.scheduledUpdateCount = cm.scheduledUpdateCount;
        m.coalescedChunkCount = cm.coalescedChunkCount;
        m.unchangedBlockCount = cm.lastChanges.unchangedCount;
        m.updatedBlockCount = cm.lastChanges.updatedCount;
        m.lastParseUs = cm.lastParseUs;
    }
    if (m_layoutCache) {
        const auto lm = m_layoutCache->metrics();
        m.layoutCount = lm.layoutCount;
        m.blockCacheHits = lm.blockCacheHits;
        m.blockCacheMisses = lm.blockCacheMisses;
        m.blockCacheEvictions = lm.blockCacheEvictions;
        m.blockCacheEntries = lm.blockCacheEntries;
        m.blockCacheEstimatedBytes = lm.blockCacheEstimatedBytes;
        m.blockCacheLimitBytes = lm.blockCacheLimitBytes;
        m.lastLayoutUs = lm.lastLayoutUs;
    }
    return m;
}

void MarkdownView::setBaseUrl(const QUrl &url) { m_baseUrl = url; }
QUrl MarkdownView::baseUrl() const { return m_baseUrl; }
void MarkdownView::scrollToAnchor(const QString& targetInput)
{
    if (!m_documentLayout || targetInput.isEmpty() || m_autoFitHeight) return;
    QString target = targetInput.trimmed();
    if (target.startsWith(u'#')) target = target.mid(1).trimmed();
    if (target.isEmpty() || m_documentLayout->blockCount() == 0) return;

    auto slugify = [](const QString& str) -> QString {
        QString s;
        for (const QChar& ch : str) {
            if (ch.isLetterOrNumber()) s += ch.toLower();
            else if (!s.isEmpty() && !s.endsWith(u'-')) s += u'-';
        }
        while (s.endsWith(u'-')) s.chop(1);
        return s;
    };

    const QString targetSlug = slugify(target);

    for (const auto& block : *m_documentLayout) {
        if (block.kind != ui::markdown::BlockKind::Heading) continue;
        const QString text = block.inlineLayout ? block.inlineLayout->text.trimmed() : QString();
        const QString slug = slugify(text);
        if (slug == targetSlug || text.compare(target, Qt::CaseInsensitive) == 0) {
            const int targetY = qBound(0, qRound(block.rect.top() - m_theme.contentMargins.top()), verticalScrollBar()->maximum());
            auto* anim = new QVariantAnimation(this);
            anim->setDuration(160);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            anim->setStartValue(verticalScrollBar()->value());
            anim->setEndValue(targetY);
            connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
                verticalScrollBar()->setValue(val.toInt());
            });
            connect(anim, &QVariantAnimation::finished, anim, &QObject::deleteLater);
            anim->start();
            return;
        }
    }
}

void MarkdownView::setMarkdownStyleSheet(const MarkdownStyleSheet &styleSheet)
{
    m_styleSheet = styleSheet;
    m_theme.bodyFont.setFamily(styleSheet.style.bodyFontFamily);
    if (styleSheet.style.fontSize > 0) m_theme.bodyFont.setPixelSize(styleSheet.style.fontSize);
    m_theme.codeFont.setFamily(styleSheet.style.monospaceFontFamily);
    if (styleSheet.colors.text.isValid()) m_theme.text = styleSheet.colors.text;
    if (styleSheet.colors.link.isValid()) m_theme.link = styleSheet.colors.link;
    if (styleSheet.colors.codeBackground.isValid()) m_theme.codeBackground = styleSheet.colors.codeBackground;
    ++m_theme.version;
    m_layoutCache->setTheme(m_theme);
    invalidatePreferredSize();
    updateActualLayoutWidth();
}

MarkdownStyleSheet MarkdownView::markdownStyleSheet() const { return m_styleSheet; }

void MarkdownView::resetMarkdownStyleSheetToTheme()
{
    m_theme = ui::markdown::MarkdownTheme::light(font());
    if (m_customContentMargins.has_value()) {
        m_theme.contentMargins = *m_customContentMargins;
    }
    m_layoutCache->setTheme(m_theme);
    invalidatePreferredSize();
    updateActualLayoutWidth();
}

void MarkdownView::setTheme(const ui::markdown::MarkdownTheme& theme)
{
    m_theme = theme;
    m_layoutCache->setTheme(m_theme);
    invalidatePreferredSize();
    updateActualLayoutWidth();
}

ui::markdown::MarkdownTheme MarkdownView::theme() const { return m_theme; }

void MarkdownView::setBaseFont(const QFont& font)
{
    if (font == m_theme.bodyFont) return;
    m_theme.bodyFont = font;
    m_theme.codeFont.setPointSizeF(font.pointSizeF() * .93);
    ++m_theme.version;
    m_layoutCache->setTheme(m_theme);
    invalidatePreferredSize();
    updateActualLayoutWidth();
}

void MarkdownView::setContentMargins(const QMarginsF& margins)
{
    m_customContentMargins = margins;
    if (m_theme.contentMargins == margins) return;
    m_theme.contentMargins = margins;
    ++m_theme.version;
    m_layoutCache->setTheme(m_theme);
    invalidatePreferredSize();
    updateActualLayoutWidth();
}

void MarkdownView::setTransparentBackground(bool transparent)
{
    m_transparentBackground = transparent;
    viewport()->update();
}

bool MarkdownView::isTransparentBackground() const { return m_transparentBackground; }

void MarkdownView::setHorizontalSizingMode(HorizontalSizingMode mode)
{
    if (m_horizontalSizingMode == mode) return;
    m_horizontalSizingMode = mode;
    auto policy = sizePolicy();
    policy.setHorizontalPolicy(mode == HorizontalSizingMode::FitContent ? QSizePolicy::Preferred : QSizePolicy::Expanding);
    setSizePolicy(policy);
    invalidatePreferredSize();
    updateGeometry();
}

MarkdownView::HorizontalSizingMode MarkdownView::horizontalSizingMode() const
{
    return m_horizontalSizingMode;
}

void MarkdownView::setPreferredWidthLimit(qreal width)
{
    if (qAbs(m_preferredWidthLimit - width) < 0.5) return;
    m_preferredWidthLimit = width;
    invalidatePreferredSize();
}

qreal MarkdownView::preferredWidthLimit() const
{
    return m_preferredWidthLimit;
}

void MarkdownView::setAutoFitHeight(bool enable)
{
    if (m_autoFitHeight == enable) return;
    m_autoFitHeight = enable;
    auto policy = sizePolicy();
    policy.setVerticalPolicy(QSizePolicy::Preferred);
    policy.setHeightForWidth(enable);
    setSizePolicy(policy);
    updateScrollBars();
    updateAutoFitHeight();
    invalidatePreferredSize();
    updateGeometry();
}

bool MarkdownView::isAutoFitHeight() const { return m_autoFitHeight; }

int MarkdownView::elementIndex(ui::markdown::BlockId blockId, ui::markdown::ElementId elementId) const
{
    for (int i = 0; i < m_documentLayout->blockCount(); ++i) {
        const auto& block = m_documentLayout->blockAt(i);
        if (block.blockId == blockId && block.elementId == elementId) return i;
    }
    return -1;
}

ui::markdown::BlockScrollOffset MarkdownView::blockScrollOffset(ui::markdown::BlockId blockId, ui::markdown::ElementId elementId) const
{
    return m_blockScrollOffsets.value(elementIndex(blockId, elementId));
}

void MarkdownView::setBlockScrollOffset(ui::markdown::BlockId blockId, ui::markdown::ElementId elementId,
                                        const ui::markdown::BlockScrollOffset& offset)
{
    const int blockIndex = elementIndex(blockId, elementId);
    if (blockIndex < 0) return;
    if (auto* anim = m_blockScrollAnimations.value(blockIndex)) anim->stop();
    m_blockTargetScrollOffsets.insert(blockIndex, offset);
    m_blockScrollOffsets.insert(blockIndex, offset);
    viewport()->update();
}

bool MarkdownView::scrollBlock(ui::markdown::BlockId blockId, ui::markdown::ElementId elementId,
                               qreal deltaX, qreal deltaY, bool smooth)
{
    return scrollBlockAtIndex(elementIndex(blockId, elementId), deltaX, deltaY, smooth);
}

bool MarkdownView::scrollBlockAtIndex(int blockIndex, qreal deltaX, qreal deltaY, bool smooth)
{
    if (blockIndex < 0 || blockIndex >= m_documentLayout->blockCount()) return false;
    const auto& block = m_documentLayout->blockAt(blockIndex);
    if (!block.scrollInfo.hasHorizontalScroll() && !block.scrollInfo.hasVerticalScroll()) return false;

    const auto current = m_blockScrollOffsets.value(blockIndex);
    auto target = m_blockTargetScrollOffsets.value(blockIndex, current);

    if (block.scrollInfo.hasHorizontalScroll())
        target.x = qBound<qreal>(0, target.x + deltaX, block.scrollInfo.maxScrollX());
    if (block.scrollInfo.hasVerticalScroll())
        target.y = qBound<qreal>(0, target.y + deltaY, block.scrollInfo.maxScrollY());

    const auto prevTarget = m_blockTargetScrollOffsets.value(blockIndex, current);
    if (qFuzzyCompare(target.x, current.x) && qFuzzyCompare(target.y, current.y) &&
        qFuzzyCompare(target.x, prevTarget.x) && qFuzzyCompare(target.y, prevTarget.y))
        return false;

    m_blockTargetScrollOffsets.insert(blockIndex, target);

    if (!smooth) {
        if (auto* anim = m_blockScrollAnimations.value(blockIndex)) anim->stop();
        m_blockScrollOffsets.insert(blockIndex, target);
        viewport()->update();
        return true;
    }

    auto* anim = m_blockScrollAnimations.value(blockIndex);
    if (!anim) {
        anim = new QVariantAnimation(this);
        anim->setDuration(160);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim, &QVariantAnimation::valueChanged, this, [this, blockIndex](const QVariant& val) {
            const QPointF pt = val.toPointF();
            m_blockScrollOffsets.insert(blockIndex, {pt.x(), pt.y()});
            viewport()->update();
        });
        m_blockScrollAnimations.insert(blockIndex, anim);
    } else {
        anim->stop();
    }

    anim->setStartValue(QPointF(current.x, current.y));
    anim->setEndValue(QPointF(target.x, target.y));
    anim->start();
    return true;
}

void MarkdownView::onThemeUpdated()
{
    if (m_usesThemeStyleSheet) {
        m_theme = fluent::FluentElement::currentTheme() == fluent::FluentElement::Dark
            ? ui::markdown::MarkdownTheme::dark(font())
            : ui::markdown::MarkdownTheme::light(font());
        if (m_customContentMargins.has_value()) {
            m_theme.contentMargins = *m_customContentMargins;
        }
    }
    m_layoutCache->setTheme(m_theme);
    invalidatePreferredSize();
    updateActualLayoutWidth();
}

void MarkdownView::setAllowNetworkAccess(bool allow)
{
    m_allowNetworkAccess = allow;
    m_resources.setNetworkAccessEnabled(allow);
}

bool MarkdownView::allowNetworkAccess() const { return m_allowNetworkAccess; }

void MarkdownView::clearResourceCache()
{
    m_resources.clear();
    requestImageResources();
}

void MarkdownView::setImageLoadingEnabled(bool enabled) { setAllowNetworkAccess(enabled); }
bool MarkdownView::imageLoadingEnabled() const { return allowNetworkAccess(); }

void MarkdownView::setAllowHtml(bool allow) { m_controller->setAllowHtml(allow); }
bool MarkdownView::allowHtml() const { return m_controller->allowHtml(); }

QString MarkdownView::selectedText() const
{
    const QString text = documentPlainText();
    if (!m_eventFilter->selection().isValid()) return {};
    const auto sel = m_eventFilter->selection();
    const int begin = qBound(0, qMin(sel.anchor, sel.position), text.size());
    const int end = qBound(0, qMax(sel.anchor, sel.position), text.size());
    return text.mid(begin, end - begin);
}

QString MarkdownView::selectedHtml() const
{
    if (!m_eventFilter->selection().isValid()) return {};
    const auto sel = m_eventFilter->selection();
    const int selStart = qMin(sel.anchor, sel.position);
    const int selEnd = qMax(sel.anchor, sel.position);
    if (selStart >= selEnd) return {};

    QString html;
    for (const auto& block : *m_documentLayout) {
        int blockStart = block.displayTextOffset;
        int blockLen = block.inlineLayout ? static_cast<int>(block.inlineLayout->text.size())
                     : (block.kind == ui::markdown::BlockKind::CodeBlock ? static_cast<int>(block.code.size()) : 0);
        int blockEnd = blockStart + blockLen;

        if (selEnd <= blockStart || selStart >= blockEnd) continue;

        int relStart = qBound(0, selStart - blockStart, blockLen);
        int relEnd = qBound(0, selEnd - blockStart, blockLen);
        if (relStart >= relEnd) continue;

        if (block.kind == ui::markdown::BlockKind::CodeBlock) {
            const QString codeSlice = block.code.mid(relStart, relEnd - relStart).toHtmlEscaped();
            html += QStringLiteral("<pre><code>%1</code></pre>\n").arg(codeSlice);
        } else if (block.kind == ui::markdown::BlockKind::Heading && block.inlineLayout) {
            const QString textSlice = block.inlineLayout->text.mid(relStart, relEnd - relStart).toHtmlEscaped();
            html += QStringLiteral("<h3>%1</h3>\n").arg(textSlice);
        } else if (block.kind == ui::markdown::BlockKind::ListItem && block.inlineLayout) {
            const QString textSlice = block.inlineLayout->text.mid(relStart, relEnd - relStart).toHtmlEscaped();
            html += QStringLiteral("<li>%1</li>\n").arg(textSlice);
        } else if (block.kind == ui::markdown::BlockKind::QuoteContent && block.inlineLayout) {
            const QString textSlice = block.inlineLayout->text.mid(relStart, relEnd - relStart).toHtmlEscaped();
            html += QStringLiteral("<blockquote>%1</blockquote>\n").arg(textSlice);
        } else if (block.inlineLayout) {
            const QString textSlice = block.inlineLayout->text.mid(relStart, relEnd - relStart).toHtmlEscaped();
            html += QStringLiteral("<p>%1</p>\n").arg(textSlice);
        }
    }

    return html.isEmpty() ? selectedText().toHtmlEscaped() : html;
}

void MarkdownView::copy()
{
    const QString text = selectedText();
    if (text.isEmpty()) return;
    const QString html = selectedHtml();
    auto* mime = new QMimeData();
    mime->setText(text);
    if (!html.isEmpty()) {
        mime->setHtml(html);
    }
    QGuiApplication::clipboard()->setMimeData(mime);
}

void MarkdownView::selectAll()
{
    if (!isSelectable()) return;
    const int len = static_cast<int>(documentPlainText().size());
    m_eventFilter->setSelection({0, len});
    emit selectionChanged(true);
    viewport()->update();
}

void MarkdownView::setSelectable(bool selectable) { m_eventFilter->setSelectable(selectable); }
bool MarkdownView::isSelectable() const { return m_eventFilter->isSelectable(); }

void MarkdownView::setTaskListInteractive(bool interactive) { m_eventFilter->setTaskListInteractive(interactive); }
bool MarkdownView::isTaskListInteractive() const { return m_eventFilter->isTaskListInteractive(); }

void MarkdownView::setZoomFactor(qreal factor)
{
    m_zoomFactor = factor;
    m_theme.bodyFont.setPointSizeF(font().pointSizeF() * qBound<qreal>(0.5, factor, 3.0));
    m_theme.codeFont.setPointSizeF(m_theme.bodyFont.pointSizeF() * .93);
    ++m_theme.version;
    m_layoutCache->setTheme(m_theme);
    invalidatePreferredSize();
    updateActualLayoutWidth();
}

qreal MarkdownView::zoomFactor() const { return m_zoomFactor; }

bool MarkdownView::findText(const QString &text, QTextDocument::FindFlags flags, bool incremental, bool *wrapped)
{
    if (text.isEmpty()) return false;
    const QString haystack = documentPlainText();
    if (haystack.isEmpty()) return false;

    const bool backward = flags.testFlag(QTextDocument::FindBackward);
    const bool caseSensitive = flags.testFlag(QTextDocument::FindCaseSensitively);
    const bool wholeWords = flags.testFlag(QTextDocument::FindWholeWords);
    const auto sel = m_eventFilter->selection();

    int foundStart = -1;
    int foundLength = static_cast<int>(text.size());
    bool didWrap = false;

    if (wholeWords) {
        const QRegularExpression::PatternOptions options = caseSensitive
            ? QRegularExpression::NoPatternOption
            : QRegularExpression::CaseInsensitiveOption;
        const QRegularExpression re(QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(text)), options);

        QList<QPair<int, int>> matches;
        auto it = re.globalMatch(haystack);
        while (it.hasNext()) {
            auto match = it.next();
            matches.append({static_cast<int>(match.capturedStart()), static_cast<int>(match.capturedLength())});
        }

        if (matches.isEmpty()) return false;

        if (!backward) {
            const int from = sel.isValid()
                ? (incremental ? qMin(sel.anchor, sel.position) : qMax(sel.anchor, sel.position))
                : (sel.position >= 0 ? sel.position : 0);
            for (const auto& m : matches) {
                if (m.first >= from) {
                    foundStart = m.first;
                    foundLength = m.second;
                    break;
                }
            }
            if (foundStart < 0 && from > 0) {
                foundStart = matches.first().first;
                foundLength = matches.first().second;
                didWrap = true;
            }
        } else {
            const int from = sel.isValid()
                ? (incremental ? qMax(sel.anchor, sel.position) : qMin(sel.anchor, sel.position))
                : (sel.position >= 0 ? sel.position : static_cast<int>(haystack.size()));
            for (int i = matches.size() - 1; i >= 0; --i) {
                if (matches[i].first < from) {
                    foundStart = matches[i].first;
                    foundLength = matches[i].second;
                    break;
                }
            }
            if (foundStart < 0 && from < haystack.size()) {
                foundStart = matches.last().first;
                foundLength = matches.last().second;
                didWrap = true;
            }
        }
    } else {
        const Qt::CaseSensitivity cs = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
        if (!backward) {
            const int from = sel.isValid()
                ? (incremental ? qMin(sel.anchor, sel.position) : qMax(sel.anchor, sel.position))
                : (sel.position >= 0 ? sel.position : 0);
            int pos = static_cast<int>(haystack.indexOf(text, from, cs));
            if (pos < 0 && from > 0) {
                pos = static_cast<int>(haystack.indexOf(text, 0, cs));
                didWrap = (pos >= 0);
            }
            if (pos >= 0) {
                foundStart = pos;
                foundLength = static_cast<int>(text.size());
            }
        } else {
            const int from = sel.isValid()
                ? (incremental ? qMax(sel.anchor, sel.position) : qMin(sel.anchor, sel.position) - 1)
                : (sel.position >= 0 ? sel.position - 1 : static_cast<int>(haystack.size()) - 1);
            int pos = from >= 0 ? static_cast<int>(haystack.lastIndexOf(text, from, cs)) : -1;
            if (pos < 0 && from < haystack.size()) {
                pos = static_cast<int>(haystack.lastIndexOf(text, -1, cs));
                didWrap = (pos >= 0);
            }
            if (pos >= 0) {
                foundStart = pos;
                foundLength = static_cast<int>(text.size());
            }
        }
    }

    if (wrapped) *wrapped = didWrap;
    if (foundStart < 0) return false;

    m_eventFilter->setSelection({foundStart, foundStart + foundLength});
    emit selectionChanged(true);

    for (const auto& b : *m_documentLayout) {
        if (foundStart >= b.displayTextOffset && foundStart <= b.displayTextOffset + (b.inlineLayout ? b.inlineLayout->text.size() : 0)) {
            if (!m_autoFitHeight) {
                const int viewTop = verticalScrollBar()->value();
                const int viewBottom = viewTop + viewport()->height();
                if (b.rect.top() < viewTop || b.rect.bottom() > viewBottom) {
                    verticalScrollBar()->setValue(qBound(0, qRound(b.rect.top() - 20), verticalScrollBar()->maximum()));
                }
            }
            break;
        }
    }

    viewport()->update();
    return true;
}

bool MarkdownView::hasHeightForWidth() const { return m_autoFitHeight; }

int MarkdownView::heightForWidth(int width) const
{
    if (!m_autoFitHeight || width <= 0) return sizeHint().height();
    const qreal cw = qMax<qreal>(1, width - (verticalScrollBar()->isVisible() ? verticalScrollBar()->width() : 0));
    if (qAbs(m_documentLayout->width - cw) < 0.5 && m_lastAutoFitHeight > 0) return m_lastAutoFitHeight;
    if (m_layoutCache) {
        const auto docLayout = m_layoutCache->measure(cw);
        return qMax(16, qCeil(docLayout->size.height()));
    }
    return sizeHint().height();
}

// Preferred measurement and actual layout geometry must remain strictly independent.
// - sizeHint() is derived from content measured against an external width limit (m_preferredWidthLimit).
// - Actual rendering is laid out against viewport()->width().
// Never derive preferred width from the current viewport width to avoid geometry feedback loops.

QSize MarkdownView::sizeHint() const
{
    const int h = qMax(0, qCeil(m_preferredContentSize.height() > 0
        ? m_preferredContentSize.height()
        : QFontMetricsF(m_theme.bodyFont).height()));

    if (m_horizontalSizingMode == HorizontalSizingMode::FitContent) {
        const int w = qMax(0, qCeil(m_preferredContentSize.width()));
        return QSize(w, h);
    }
    return QSize(0, h);
}

QSize MarkdownView::minimumSizeHint() const
{
    return QSize(0, 0);
}


void MarkdownView::wheelEvent(QWheelEvent *event)
{
    if (m_autoFitHeight) {
        event->ignore();
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

void MarkdownView::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateActualLayoutWidth();
    updateScrollBars();
}

void MarkdownView::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::SelectAll)) { selectAll(); event->accept(); return; }
    if (event->matches(QKeySequence::Copy)) { copy(); event->accept(); return; }
    QAbstractScrollArea::keyPressEvent(event);
}

void MarkdownView::focusOutEvent(QFocusEvent* event)
{
    QAbstractScrollArea::focusOutEvent(event);
    if (!m_eventFilter->selection().isValid()) return;
    m_eventFilter->clearSelection();
}

void MarkdownView::onLayoutReady(ui::markdown::DocumentLayoutPtr layout)
{
    QHash<ui::markdown::ElementKey, ui::markdown::BlockScrollOffset> offsetsByElement;
    QHash<ui::markdown::ElementKey, ui::markdown::BlockScrollOffset> targetsByElement;
    if (m_documentLayout) {
        for (int i = 0; i < m_documentLayout->blockCount(); ++i) {
            const auto& oldBlock = m_documentLayout->blockAt(i);
            const ui::markdown::ElementKey key{oldBlock.blockId, oldBlock.elementId};
            if (m_blockScrollOffsets.contains(i)) offsetsByElement.insert(key, m_blockScrollOffsets.value(i));
            if (m_blockTargetScrollOffsets.contains(i)) targetsByElement.insert(key, m_blockTargetScrollOffsets.value(i));
        }
    }
    for (auto* animation : m_blockScrollAnimations) {
        animation->stop();
        animation->deleteLater();
    }
    m_blockScrollAnimations.clear();
    m_blockScrollOffsets.clear();
    m_blockTargetScrollOffsets.clear();

    m_documentLayout = layout;
    for (int i = 0; i < m_documentLayout->blockCount(); ++i) {
        const auto& newBlock = m_documentLayout->blockAt(i);
        const ui::markdown::ElementKey key{newBlock.blockId, newBlock.elementId};
        if (offsetsByElement.contains(key)) m_blockScrollOffsets.insert(i, offsetsByElement.value(key));
        if (targetsByElement.contains(key)) m_blockTargetScrollOffsets.insert(i, targetsByElement.value(key));
    }
    m_metrics.blockCount = m_documentLayout->blockCount();
    m_metrics.documentHeight = m_documentLayout->size.height();
    m_preferredContentSize = m_documentLayout->size;
    const auto lm = m_layoutCache->metrics();
    m_metrics.layoutCount = lm.layoutCount;
    m_metrics.blockCacheHits = lm.blockCacheHits;
    m_metrics.blockCacheMisses = lm.blockCacheMisses;
    m_metrics.blockCacheEvictions = lm.blockCacheEvictions;
    m_metrics.blockCacheEntries = lm.blockCacheEntries;
    m_metrics.blockCacheEstimatedBytes = lm.blockCacheEstimatedBytes;
    m_metrics.blockCacheLimitBytes = lm.blockCacheLimitBytes;
    m_metrics.lastLayoutUs = lm.lastLayoutUs;
    updateScrollBars();
    updateAutoFitHeight();
    requestImageResources();
    recalculatePreferredSize();
    if (m_lastDocumentSize != m_documentLayout->size) {
        m_lastDocumentSize = m_documentLayout->size;
        emit documentSizeChanged(m_lastDocumentSize);
    }
    viewport()->update();

    // Emit streamingFinished AFTER the final layout is in place.
    // Controller no longer emits streamingFinished; we detect the transition here
    // so consumers always see the completed layout when streamingFinished fires.
    if (m_wasStreaming && !m_controller->isStreaming()) {
        m_wasStreaming = false;
        emit streamingFinished();
    }
}

void MarkdownView::onTaskToggleRequested(int blockIndex)
{
    if (blockIndex < 0 || blockIndex >= m_documentLayout->blockCount()) return;
    const auto& block = m_documentLayout->blockAt(blockIndex);
    m_controller->toggleTask(block.taskMarkerRange, block.taskChecked);
}

void MarkdownView::onRepaintRequested()
{
    viewport()->update();
}

void MarkdownView::invalidatePreferredSize()
{
    m_preferredSizeDirty = true;
    recalculatePreferredSize();
}

void MarkdownView::recalculatePreferredSize()
{
    const qreal limit = m_preferredWidthLimit > 0 ? m_preferredWidthLimit : DefaultPreferredWidthLimit;
    const auto measured = m_layoutCache->measure(limit);
    const QSizeF newSize = measured ? measured->size : QSizeF();

    if (m_preferredSizeDirty || m_preferredContentSize != newSize) {
        m_preferredSizeDirty = false;
        m_preferredContentSize = newSize;
        updateGeometry();
    }
}

void MarkdownView::updateActualLayoutWidth()
{
    if (!viewport()) return;
    const qreal w = qMax<qreal>(1.0, viewport()->width());
    m_layoutCache->setWidth(w);
}

void MarkdownView::updateScrollBars()
{
    if (m_autoFitHeight) {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        verticalScrollBar()->setRange(0, 0);
        verticalScrollBar()->setValue(0);
        return;
    }

    const int height = qCeil(m_documentLayout->size.height());
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setRange(0, qMax(0, height - viewport()->height()));
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void MarkdownView::updateAutoFitHeight()
{
    if (!m_autoFitHeight) {
        return;
    }

    const int h = qMax(16, qCeil(m_documentLayout->size.height()));
    if (h == m_lastAutoFitHeight) {
        return;
    }

    m_lastAutoFitHeight = h;
    emit autoFitHeightChanged(h);
}

void MarkdownView::paintViewport(QPaintEvent* event)
{
    QElapsedTimer paintTimer; paintTimer.start();
    QPainter painter(viewport());
    if (!m_transparentBackground) painter.fillRect(event->rect(), m_theme.background.isValid() ? m_theme.background : palette().color(QPalette::Base));
    painter.save();
    const int scrollY = m_autoFitHeight ? 0 : verticalScrollBar()->value();
    painter.translate(0, -scrollY);
    const QRectF exposed = QRectF(event->rect()).translated(0, scrollY);
    m_metrics.visibleBlockCount = m_renderer.paint(painter, *m_documentLayout, m_theme, exposed,
                                                    m_eventFilter->selection(),
                                                    m_eventFilter->hoveredBlock(),
                                                    m_eventFilter->hoveredCopyBlock(),
                                                    m_blockScrollOffsets,
                                                    m_resources.images(),
                                                    m_eventFilter->copiedBlock(),
                                                    m_eventFilter->hoveredLinkUrl());
    painter.restore();
    m_metrics.lastPaintMs = paintTimer.elapsed();
}

bool MarkdownView::handleBlockWheel(QWheelEvent* event)
{
    const QPointF docPos = toDocument(event->position());
    const auto hit = m_renderer.hitTest(*m_documentLayout, docPos, m_blockScrollOffsets);
    if (hit.blockIndex < 0 || hit.blockIndex >= m_documentLayout->blockCount()) return false;
    const auto& block = m_documentLayout->blockAt(hit.blockIndex);
    if (!block.scrollInfo.hasHorizontalScroll() && !block.scrollInfo.hasVerticalScroll()) return false;

    const QPoint numPixels = event->pixelDelta();
    const QPoint numDegrees = event->angleDelta() / 8;
    qreal dx = 0, dy = 0;
    bool isPixel = false;
    if (!numPixels.isNull()) { dx = -numPixels.x(); dy = -numPixels.y(); isPixel = true; }
    else if (!numDegrees.isNull()) { dx = -numDegrees.x(); dy = -numDegrees.y(); }

    if (event->modifiers().testFlag(Qt::ShiftModifier) && dx == 0) { dx = dy; dy = 0; }
    else if (block.scrollInfo.hasHorizontalScroll() && !block.scrollInfo.hasVerticalScroll() && dx == 0) { dx = dy; dy = 0; }

    if (scrollBlock(hit.blockId, hit.elementId, dx, dy, !isPixel)) { event->accept(); return true; }
    return false;
}

bool MarkdownView::viewportEvent(QEvent* event)
{
    if (event->type() == QEvent::Paint) { paintViewport(static_cast<QPaintEvent*>(event)); return true; }
    if (event->type() == QEvent::Wheel) {
        if (handleBlockWheel(static_cast<QWheelEvent*>(event))) return true;
    }
    if (m_eventFilter->handleViewportEvent(event)) return true;
    return QAbstractScrollArea::viewportEvent(event);
}

QPointF MarkdownView::toDocument(const QPointF& viewportPosition) const
{
    return viewportPosition + QPointF(0, m_autoFitHeight ? 0 : verticalScrollBar()->value());
}

QString MarkdownView::documentPlainText() const
{
    return m_documentLayout ? m_documentLayout->displayText() : QString{};
}

void MarkdownView::requestImageResources()
{
    for (const auto& block : *m_documentLayout) {
        if (block.kind == ui::markdown::BlockKind::Image)
            m_resources.request(block.imageUrl, m_baseUrl);
    }
}

} // namespace ui::widget
