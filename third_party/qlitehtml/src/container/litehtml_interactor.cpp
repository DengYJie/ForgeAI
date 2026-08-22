#include "litehtml_interactor.h"
#include "container_internal.h"
#include "elements/details_element.h"
#include "elements/form_control_element.h"

#include <litehtml/render_item.h>

#if QT_CONFIG(clipboard)
#include <QClipboard>
#endif
#include <QDebug>
#include <QDir>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QTextDocument>
#include <QTextLayout>

#include <algorithm>
#include <functional>
#include <limits>

namespace {
static Q_LOGGING_CATEGORY(log, "qlitehtml", QtCriticalMsg)
constexpr int kDragDistance = 5;
}

namespace qlitehtml::internal {

std::vector<litehtml::element::ptr> path(const litehtml::element::ptr &element)
{
    std::vector<litehtml::element::ptr> result;
    litehtml::element::ptr current = element;
    while (current) {
        result.push_back(current);
        current = current->parent();
    }
    std::reverse(std::begin(result), std::end(result));
    return result;
}

std::pair<litehtml::element::ptr, size_t> getCommonParent(const std::vector<litehtml::element::ptr> &a,
                                                          const std::vector<litehtml::element::ptr> &b)
{
    litehtml::element::ptr parent;
    const size_t minSize = std::min(a.size(), b.size());
    for (size_t i = 0; i < minSize; ++i) {
        if (a.at(i) != b.at(i))
            return {parent, i};
        parent = a.at(i);
    }
    return {parent, minSize};
}

std::pair<Selection::Element, Selection::Element> getStartAndEnd(const Selection::Element &a,
                                                                 const Selection::Element &b)
{
    if (a.element == b.element) {
        if (a.index <= b.index)
            return {a, b};
        return {b, a};
    }
    const std::vector<litehtml::element::ptr> aPath = path(a.element);
    const std::vector<litehtml::element::ptr> bPath = path(b.element);
    litehtml::element::ptr commonParent;
    size_t firstDifferentIndex;
    std::tie(commonParent, firstDifferentIndex) = getCommonParent(aPath, bPath);
    if (!commonParent) {
        qWarning() << "internal error: litehtml elements do not have common parent";
        return {a, b};
    }
    if (commonParent == a.element)
        return {a, a}; // 'a' already contains 'b'
    if (commonParent == b.element)
        return {b, b};
    // find out if a or b is first in the child sub-trees of commonParent
    const litehtml::element::ptr aBranch = aPath.at(firstDifferentIndex);
    const litehtml::element::ptr bBranch = bPath.at(firstDifferentIndex);
    for (const litehtml::element::ptr &child : commonParent->children()) {
        if (child == aBranch)
            return {a, b};
        if (child == bBranch)
            return {b, a};
    }
    qWarning() << "internal error: failed to find out order of litehtml elements";
    return {a, b};
}

// 1) stops right away if element == stop, otherwise stops whenever stop element is encountered
// 2) moves down the first children from element until there is none anymore
litehtml::element::ptr firstLeaf(const litehtml::element::ptr &element,
                                 const litehtml::element::ptr &stop)
{
    if (element == stop)
        return element;
    litehtml::element::ptr current = element;
    while (current != stop && !current->children().empty())
        current = current->children().front();
    return current;
}

// 1) stops right away if element == stop, otherwise stops whenever stop element is encountered
// 2) starts at next sibling (up the hierarchy chain) if possible, otherwise root
// 3) returns first leaf of the element found in 2
litehtml::element::ptr nextLeaf(const litehtml::element::ptr &element,
                                const litehtml::element::ptr &stop)
{
    if (element == stop)
        return element;
    litehtml::element::ptr current = element;
    if (current->parent()) {
        // find next sibling
        const litehtml::element::ptr parent = current->parent();
        const auto &siblings = parent->children();
        bool found = false;
        litehtml::element::ptr next;
        for (const litehtml::element::ptr &sibling : siblings) {
            if (found) {
                next = sibling;
                break;
            }
            if (sibling == current)
                found = true;
        }
        if (!next) // no sibling, move up
            return nextLeaf(parent, stop);
        current = next;
    }
    return firstLeaf(current, stop);
}

Selection::Element selectionDetails(const litehtml::element::ptr &element,
                                    const QString &text,
                                    const QPoint &pos)
{
    QTextLayout layout(text, toQFont(element->css().get_font()));
    layout.beginLayout();
    QTextLine line = layout.createLine();
    if (!line.isValid()) {
        layout.endLayout();
        return {element, 0, 0};
    }
    line.setLineWidth(std::numeric_limits<qreal>::max());
    layout.endLayout();

    const int index = line.xToCursor(pos.x(), QTextLine::CursorBetweenCharacters);
    return {element, index, qRound(line.cursorToX(index))};
}

Selection::Element deepestElementAtPoint(const litehtml::document::ptr &document,
                                         const QPoint &pos,
                                         const QPoint &viewportPos,
                                         Selection::Mode mode)
{
    if (!document)
        return {};

    // Find the element at this point
    const litehtml::element::ptr element
        = document->root_render()->get_element_by_point(pos.x(), pos.y(), viewportPos.x(), viewportPos.y(), nullptr);
    // Only rendered text leaves are valid selection endpoints. Containers return
    // concatenated descendant text, whose indices do not match their geometry.
    const std::function<Selection::Element(litehtml::element::ptr)> recursion =
        [&recursion, pos, mode](const litehtml::element::ptr &element) -> Selection::Element {
        if (!element)
            return {};

        const QRect placement = toQRect(element->get_placement());
        if (!placement.adjusted(0, 0, 1, 1).contains(pos))
            return {};

        for (const litehtml::element::ptr &child : element->children()) {
            const Selection::Element result = recursion(child);
            if (result.element)
                return result;
        }

        if (!element->children().empty())
            return {};

        litehtml::string text;
        element->get_text(text);
        if (text.empty())
            return {};

        return mode == Selection::Mode::Free ? selectionDetails(element,
                                                                QString::fromUtf8(text.data(), int(text.size())),
                                                                pos - placement.topLeft())
                                             : Selection::Element({element, -1, -1});
    };
    return recursion(element);
}

litehtml::element::ptr elementAtPoint(const litehtml::document::ptr &document,
                                      const QPoint &documentPos,
                                      const QPoint &viewportPos)
{
    if (!document || !document->root_render()) {
        return {};
    }

    return document->root_render()->get_element_by_point(documentPos.x(),
                                                         documentPos.y(),
                                                         viewportPos.x(),
                                                         viewportPos.y(),
                                                         nullptr);
}

litehtml::element::ptr firstMatchingAncestor(
    litehtml::element::ptr element, const std::function<bool(const litehtml::element::ptr &)> &match)
{
    while (element) {
        if (match(element)) {
            return element;
        }

        element = element->parent();
    }

    return {};
}

bool Selection::isValid() const
{
    return !selection.isEmpty();
}

void Selection::update()
{
    const auto addElement = [this](const Selection::Element &element,
                                   const Selection::Element &end = {}) {
        litehtml::string elemText;
        element.element->get_text(elemText);
        const QString textStr = QString::fromUtf8(elemText.data(), int(elemText.size()));
        if (!textStr.isEmpty()) {
            // placementRect is the unadjusted document-coordinate rect used as
            // a map key in segmentMap (must match what draw_text reconstructs).
            const QRect placementRect = toQRect(element.element->get_placement());
            QRect rect = placementRect;
            SegmentInfo seg;
            int selectionLength = 0;
            if (element.index < 0) { // fully selected
                selectionLength = textStr.size();
                text += textStr;
                seg.charStart = 0;
                seg.charEnd = -1;
                seg.pixelStart = 0;
                seg.pixelEnd = -1;
            } else if (end.element) { // select from element "to end"
                if (element.element == end.element) {
                    // end.index is guaranteed to be >= element.index by caller, same for x
                    selectionLength = end.index - element.index;
                    if (selectionLength <= 0)
                        return;
                    text += textStr.mid(element.index, selectionLength);
                    const int left = rect.left();
                    rect.setLeft(left + element.x);
                    rect.setRight(left + end.x);
                    seg.charStart = element.index;
                    seg.charEnd = end.index;
                    seg.pixelStart = element.x;
                    seg.pixelEnd = end.x;
                } else {
                    selectionLength = textStr.size() - element.index;
                    if (selectionLength <= 0)
                        return;
                    text += textStr.mid(element.index);
                    rect.setLeft(rect.left() + element.x);
                    seg.charStart = element.index;
                    seg.charEnd = -1;
                    seg.pixelStart = element.x;
                    seg.pixelEnd = -1;
                }
            } else { // select from start of element
                selectionLength = element.index;
                if (selectionLength <= 0)
                    return;
                text += textStr.left(element.index);
                rect.setRight(rect.left() + element.x);
                seg.charStart = 0;
                seg.charEnd = element.index;
                seg.pixelStart = 0;
                seg.pixelEnd = element.x;
            }
            // Skip degenerate (zero or negative width) rects to avoid a 1-pixel
            // vertical stripe artifact that appears during mouse-drag selection
            // when the cursor is at or near the start of a text element.
            if (rect.width() > 0) {
                rect = rect.adjusted(-1, -1, 1, 1);
                selection.append(rect);
                segmentMap[placementRect] = seg;
            }
        }
    };

    if (startElem.element && endElem.element) {
        // Edge cases:
        // start and end elements could be reversed or children of each other
        Selection::Element start;
        Selection::Element end;
        std::tie(start, end) = getStartAndEnd(startElem, endElem);

        selection.clear();
        text.clear();
        segmentMap.clear();

        addElement(start, end);
        if (start.element != end.element) {
            litehtml::element::ptr current = start.element;
            do {
                current = nextLeaf(current, end.element);
                if (current == end.element)
                    addElement(end);
                else
                    addElement({current, -1, -1});
            } while (current != end.element);
        }
    } else {
        selection = {};
        text.clear();
        segmentMap.clear();
    }
#if QT_CONFIG(clipboard)
    QClipboard *cb = QGuiApplication::clipboard();
    if (cb->supportsSelection())
        cb->setText(text, QClipboard::Selection);
#endif
}

QRect Selection::boundingRect() const
{
    QRect rect;
    for (const QRect &r : selection)
        rect = rect.united(r);
    return rect;
}

Index::Entry Index::findElement(int index) const
{
    const auto upper = std::upper_bound(std::begin(indexToElement),
                                        std::end(indexToElement),
                                        Entry{index, {}},
                                        [](const Entry &a, const Entry &b) {
                                            return a.first < b.first;
                                        });
    if (upper == std::begin(indexToElement)) // should not happen for index >= 0
        return {-1, {}};
    return *(upper - 1);
}

QUrl LiteHtmlInteractor::resolveUrl(const QString &url, const QString &baseUrl) const
{
    return qlitehtml::internal::resolveUrl(url, baseUrl.isEmpty() ? m_baseUrl : baseUrl);
}

void LiteHtmlInteractor::on_anchor_click(const char *url, const litehtml::element::ptr &el)
{
    Q_UNUSED(el)
    if (!m_blockLinks && m_linkCallback)
        m_linkCallback(resolveUrl(QString::fromUtf8(url), m_baseUrl));
}

bool LiteHtmlInteractor::on_element_click(const litehtml::element::ptr &el)
{
    if (m_formControlCallback && el) {
        if (auto formControl = std::dynamic_pointer_cast<form_control_element>(el)) {
            auto type = QString::fromUtf8(el->get_attr("type", ""));
            auto name = QString::fromUtf8(el->get_attr("name", ""));
            auto value = QString::fromUtf8(el->get_attr("value", ""));
            bool checked = formControl->is_checked();
            
            m_formControlCallback(QString::fromUtf8(el->get_tagName()), type, name, value, checked);
            return true;
        }
    }
    return false;
}

void LiteHtmlInteractor::on_mouse_event(const litehtml::element::ptr &el, litehtml::mouse_event event)
{
    Q_UNUSED(el)
    Q_UNUSED(event)
}

void LiteHtmlInteractor::set_cursor(const char *cursor)
{
    if (m_cursorCallback)
        m_cursorCallback(toQCursor(QString::fromUtf8(cursor)));
}

void LiteHtmlInteractor::buildIndex()
{
    m_index.elementToIndex.clear();
    m_index.indexToElement.clear();
    m_index.text.clear();
    m_lastIndexedElement = nullptr;
    if (!m_document || !m_document->root())
        return;

    const litehtml::element::ptr body = m_document->root()->select_one("body");
    int index = 0;
    litehtml::element::ptr current = firstLeaf(m_document->root(), nullptr);
    while (current != m_document->root()) {
        m_index.elementToIndex.insert({current, index});
        if ((!body || current->is_ancestor(body))
            && current->css().get_display() != litehtml::display_none
            && current->css().get_visibility() != litehtml::visibility_hidden) {
            litehtml::string text;
            current->get_text(text);
            if (!text.empty()) {
                m_index.indexToElement.push_back({index, current});
                const QString str = QString::fromUtf8(text.data(), int(text.size()));
                m_index.text += str;
                index += str.size();
            }
        }
        m_lastIndexedElement = current;
        current = nextLeaf(current, m_document->root());
    }
}

void LiteHtmlInteractor::updateIndex()
{
    if (!m_document || !m_document->root())
        return;

    litehtml::element::ptr body = m_document->root()->select_one("body");
    litehtml::element::ptr current = m_lastIndexedElement
                                          ? nextLeaf(m_lastIndexedElement, m_document->root())
                                          : firstLeaf(m_document->root(), nullptr);
    while (current != m_document->root()) {
        const int offset = int(m_index.text.size());
        m_index.elementToIndex.insert({current, offset});
        if ((!body || current->is_ancestor(body))
            && current->css().get_display() != litehtml::display_none
            && current->css().get_visibility() != litehtml::visibility_hidden) {
            litehtml::string text;
            current->get_text(text);
            if (!text.empty()) {
                m_index.indexToElement.push_back({offset, current});
                m_index.text += QString::fromUtf8(text.data(), int(text.size()));
            }
        }
        m_lastIndexedElement = current;
        current = nextLeaf(current, m_document->root());
    }
}

void LiteHtmlInteractor::updateSelection()
{
    const QString oldText = m_selection.text;
    m_selection.update();
    if (!m_clipboardCallback)
        return;

    const QString newText = m_selection.text;
    if (oldText.isEmpty() && !newText.isEmpty())
        m_clipboardCallback(true);
    else if (!oldText.isEmpty() && newText.isEmpty())
        m_clipboardCallback(false);
}

void LiteHtmlInteractor::clearSelection()
{
    const QString oldText = m_selection.text;
    m_selection = {};
    if (!m_clipboardCallback)
        return;

    if (!oldText.isEmpty())
        m_clipboardCallback(false);
}

void LiteHtmlInteractor::clear()
{
    clearSelection();
    m_index.elementToIndex.clear();
    m_index.indexToElement.clear();
    m_index.text.clear();
    m_lastIndexedElement = nullptr;
    m_document = nullptr;
}

const Selection::SegmentInfo* LiteHtmlInteractor::getSelectionSegmentInfo(const QRect &placementRect, Selection::SegmentInfo &localSeg) const
{
    const auto segIt = m_selection.segmentMap.constFind(placementRect);
    if (segIt != m_selection.segmentMap.constEnd()) {
        localSeg = segIt.value();
        return &localSeg;
    }
    return nullptr;
}

std::shared_ptr<details_element> LiteHtmlInteractor::detailsForSummary(const litehtml::element::ptr &element) const
{
    litehtml::element::ptr curr = element;
    litehtml::element::ptr summaryEl = nullptr;
    while (curr) {
        if (curr->tag() == litehtml::_id("summary")) {
            summaryEl = curr;
            break;
        }
        curr = curr->parent();
    }
    if (!summaryEl) {
        return nullptr;
    }
    litehtml::element::ptr parent = summaryEl->parent();
    if (!parent || parent->tag() != litehtml::_id("details")) {
        return nullptr;
    }
    // Verify it is the first summary child of details
    for (const auto &child : parent->children()) {
        if (child->tag() == litehtml::_id("summary")) {
            if (child == summaryEl) {
                return std::dynamic_pointer_cast<details_element>(parent);
            }
            return nullptr;
        }
    }
    return nullptr;
}

QVector<QRect> LiteHtmlInteractor::mousePressEvent(const QPoint &documentPos,
                                                   const QPoint &viewportPos,
                                                   Qt::MouseButton button,
                                                   Qt::KeyboardModifiers modifiers)
{
    if (!m_document || button != Qt::LeftButton)
        return {};
    QVector<QRect> redrawRects;
    // selection
    if (modifiers.testFlag(Qt::ShiftModifier) && m_selection.isValid()) {
        m_selection.selectionStartDocumentPos = documentPos;
        m_selection.endElem = deepestElementAtPoint(m_document,
                                                    documentPos,
                                                    viewportPos,
                                                    m_selection.mode);
        updateSelection();
        if (m_selection.isValid())
            redrawRects.append(m_selection.boundingRect());
    } else {
        if (m_selection.isValid())
            redrawRects.append(m_selection.boundingRect());
        clearSelection();
        m_selection.selectionStartDocumentPos = documentPos;
        m_selection.startElem = deepestElementAtPoint(m_document,
                                                      documentPos,
                                                      viewportPos,
                                                      m_selection.mode);
    }

    const litehtml::element::ptr pressedEl = elementAtPoint(m_document, documentPos, viewportPos);
    m_pressedDetails = detailsForSummary(pressedEl);

    // post to litehtml
    litehtml::position::vector redrawBoxes;
    if (m_document->on_lbutton_down(documentPos.x(),
                                    documentPos.y(),
                                    viewportPos.x(),
                                    viewportPos.y(),
                                    redrawBoxes)) {
        for (const litehtml::position &box : redrawBoxes)
            redrawRects.append(toQRect(box));
        // Custom elements (e.g. the checkbox) may flip internal state without
        // a CSS style change, so litehtml reports no redraw box for them.
        // Repaint the clicked element's box as well.
        const litehtml::element::ptr clicked = elementAtPoint(m_document, documentPos, viewportPos);
        if (clicked)
            redrawRects.append(toQRect(clicked->get_placement()));
    }
    return redrawRects;
}

QVector<QRect> LiteHtmlInteractor::mouseMoveEvent(const QPoint &documentPos,
                                                  const QPoint &viewportPos)
{
    if (!m_document)
        return {};
    QVector<QRect> redrawRects;
    // selection
    if (m_selection.isSelecting
        || (!m_selection.selectionStartDocumentPos.isNull()
            && (m_selection.selectionStartDocumentPos - documentPos).manhattanLength()
                   >= kDragDistance
            && m_selection.startElem.element)) {
        const Selection::Element element = deepestElementAtPoint(m_document,
                                                                 documentPos,
                                                                 viewportPos,
                                                                 m_selection.mode);
        if (element.element
            && (element.element != m_selection.endElem.element
                || element.index != m_selection.endElem.index)) {
            redrawRects.append(
                m_selection
                    .boundingRect() /*.adjusted(-1, -1, +1, +1)*/); // redraw old selection area
            m_selection.endElem = element;
            updateSelection();
            redrawRects.append(m_selection.boundingRect());
        }
        m_selection.isSelecting = true;
    }
    litehtml::position::vector redrawBoxes;
    if (m_document->on_mouse_over(documentPos.x(),
                                  documentPos.y(),
                                  viewportPos.x(),
                                  viewportPos.y(),
                                  redrawBoxes)) {
        for (const litehtml::position &box : redrawBoxes)
            redrawRects.append(toQRect(box));
    }
    return redrawRects;
}

QVector<QRect> LiteHtmlInteractor::mouseReleaseEvent(const QPoint &documentPos,
                                                     const QPoint &viewportPos,
                                                     Qt::MouseButton button)
{
    if (!m_document || button != Qt::LeftButton)
        return {};
    QVector<QRect> redrawRects;
    const bool wasSelecting = m_selection.isSelecting;
    // selection
    m_selection.isSelecting = false;
    m_selection.selectionStartDocumentPos = {};
    if (m_selection.isValid())
        m_blockLinks = true;
    else
        clearSelection();
    litehtml::position::vector redrawBoxes;
    if (m_document->on_lbutton_up(documentPos.x(),
                                  documentPos.y(),
                                  viewportPos.x(),
                                  viewportPos.y(),
                                  redrawBoxes)) {
        for (const litehtml::position &box : redrawBoxes)
            redrawRects.append(toQRect(box));
    }
    m_blockLinks = false;

    // Check if released on the same summary that was pressed
    const litehtml::element::ptr releasedEl = elementAtPoint(m_document, documentPos, viewportPos);
    auto releasedDetails = detailsForSummary(releasedEl);
    auto pressedDetails = m_pressedDetails.lock();
    m_pressedDetails.reset();

    if (!wasSelecting && !m_selection.isValid() && pressedDetails && releasedDetails && pressedDetails == releasedDetails) {
        pressedDetails->toggle();

        if (m_relayoutCallback) {
            m_relayoutCallback();
        }
        buildIndex();
        redrawRects.append(m_clientRect);

        if (m_detailsCallback) {
            QString id = QString::fromUtf8(pressedDetails->get_attr("id", ""));
            m_detailsCallback(id, pressedDetails->is_open());
        }
    }

    return redrawRects;
}

QVector<QRect> LiteHtmlInteractor::mouseDoubleClickEvent(const QPoint &documentPos,
                                                         const QPoint &viewportPos,
                                                         Qt::MouseButton button)
{
    if (!m_document || button != Qt::LeftButton)
        return {};
    QVector<QRect> redrawRects;
    clearSelection();
    m_selection.mode = Selection::Mode::Word;
    const Selection::Element element = deepestElementAtPoint(m_document,
                                                            documentPos,
                                                            viewportPos,
                                                            m_selection.mode);
    if (element.element) {
        m_selection.startElem = element;
        m_selection.endElem = m_selection.startElem;
        m_selection.isSelecting = true;
        updateSelection();
        if (m_selection.isValid())
            redrawRects.append(m_selection.boundingRect());
    } else {
        if (m_selection.isValid())
            redrawRects.append(m_selection.boundingRect());
        clearSelection();
    }
    return redrawRects;
}

QVector<QRect> LiteHtmlInteractor::leaveEvent()
{
    m_pressedDetails.reset();
    if (!m_document)
        return {};
    litehtml::position::vector redrawBoxes;
    if (m_document->on_mouse_leave(redrawBoxes)) {
        QVector<QRect> redrawRects;
        for (const litehtml::position &box : redrawBoxes)
            redrawRects.append(toQRect(box));
        return redrawRects;
    }
    return {};
}

QVector<QRect> LiteHtmlInteractor::scrollAt(const QPoint &documentPos, const QPoint &viewportPos, const QPoint &delta)
{
    if (!m_document)
        return {};
        
    std::vector<litehtml::scroll_values> scroll_values = 
        m_document->on_scroll(delta.x(), delta.y(), documentPos.x(), documentPos.y(), viewportPos.x(), viewportPos.y());
        
    QVector<QRect> redrawRects;
    for (const auto &val : scroll_values) {
        if (val.dx != 0 || val.dy != 0) {
            QRect rect = toQRect(val.scroll_box).adjusted(-1, -1, 1, 1);
            redrawRects.append(rect);
        }
    }
    return redrawRects;
}

QUrl LiteHtmlInteractor::linkAt(const QPoint &documentPos, const QPoint &viewportPos) const
{
    const litehtml::element::ptr element
        = firstMatchingAncestor(elementAtPoint(m_document, documentPos, viewportPos),
                                [](const litehtml::element::ptr &candidate) {
                                    const char *href = candidate->get_attr("href");
                                    return href && href[0] != '\0';
                                });
    if (!element)
        return {};
    const char *href = element->get_attr("href");
    if (href)
        return resolveUrl(QString::fromUtf8(href), m_baseUrl);
    return {};
}

QUrl LiteHtmlInteractor::imageAt(const QPoint &documentPos, const QPoint &viewportPos) const
{
    const litehtml::element::ptr element
        = firstMatchingAncestor(elementAtPoint(m_document, documentPos, viewportPos),
                                [](const litehtml::element::ptr &candidate) {
                                    const char *tagName = candidate->get_tagName();
                                    if (!tagName || strcmp(tagName, "img") != 0) {
                                        return false;
                                    }

                                    const char *src = candidate->get_attr("src");
                                    return src && src[0] != '\0';
                                });
    if (!element)
        return {};

    const char *src = element->get_attr("src");
    if (src) {
        return resolveUrl(QString::fromUtf8(src), m_baseUrl);
    }

    return {};
}

void LiteHtmlInteractor::findText(const QString &text,
                                  QTextDocument::FindFlags flags,
                                  bool incremental,
                                  bool *wrapped,
                                  bool *success,
                                  QVector<QRect> *oldSelection,
                                  QVector<QRect> *newSelection)
{
    if (success)
        *success = false;
    if (oldSelection)
        oldSelection->clear();
    if (newSelection)
        newSelection->clear();
    if (!m_document)
        return;
    const bool backward = flags & QTextDocument::FindBackward;
    int startIndex = backward ? -1 : 0;
    if (m_selection.startElem.element && m_selection.endElem.element) { // selection
        Selection::Element start;
        Selection::Element end;
        std::tie(start, end) = getStartAndEnd(m_selection.startElem, m_selection.endElem);
        Selection::Element searchStart;
        if (incremental || backward) {
            if (start.index < 0) // fully selected
                searchStart = {firstLeaf(start.element, nullptr), 0, -1};
            else
                searchStart = start;
        } else {
            if (end.index < 0) // fully selected
                searchStart = {nextLeaf(end.element, nullptr), 0, -1};
            else
                searchStart = end;
        }
        const auto findInIndex = m_index.elementToIndex.find(searchStart.element);
        if (findInIndex == std::end(m_index.elementToIndex)) {
            qWarning() << "internal error: cannot find litehtml element in index";
            return;
        }
        startIndex = findInIndex->second + searchStart.index;
        if (backward)
            --startIndex;
    }

    const auto fillXPos = [](const Selection::Element &e) {
        litehtml::string ttext;
        e.element->get_text(ttext);
        const QString text = QString::fromUtf8(ttext.data(), int(ttext.size()));
        const QFont &font = toQFont(e.element->css().get_font());
        const QFontMetrics fm(font);
        return Selection::Element{e.element, e.index, fm.size(0, text.left(e.index)).width()};
    };

    QString term = QRegularExpression::escape(text);
    if (flags & QTextDocument::FindWholeWords)
        term = QStringLiteral("\\b%1\\b").arg(term);
    const QRegularExpression::PatternOptions patternOptions
        = (flags & QTextDocument::FindCaseSensitively) ? QRegularExpression::NoPatternOption
                                                       : QRegularExpression::CaseInsensitiveOption;
    const QRegularExpression expression(term, patternOptions);

    int foundIndex = backward ? m_index.text.lastIndexOf(expression, startIndex)
                              : m_index.text.indexOf(expression, startIndex);
    if (foundIndex < 0) { // wrap
        foundIndex = backward ? m_index.text.lastIndexOf(expression)
                              : m_index.text.indexOf(expression);
        if (wrapped && foundIndex >= 0)
            *wrapped = true;
    }
    if (foundIndex >= 0) {
        const Index::Entry startEntry = m_index.findElement(foundIndex);
        const Index::Entry endEntry = m_index.findElement(foundIndex + text.size());
        if (!startEntry.second || !endEntry.second) {
            qWarning() << "internal error: search ended up with nullptr elements";
            return;
        }
        if (oldSelection)
            *oldSelection = m_selection.selection;
        clearSelection();
        m_selection.startElem = fillXPos({startEntry.second, foundIndex - startEntry.first, -1});
        m_selection.endElem = fillXPos(
            {endEntry.second, int(foundIndex + text.size() - endEntry.first), -1});
        updateSelection();
        if (newSelection)
            *newSelection = m_selection.selection;
        if (success)
            *success = true;
        return;
    }
    return;
}

} // namespace qlitehtml::internal
