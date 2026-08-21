// Text selection model (Selection), the search index (Index) and the
// document-tree traversal / hit-testing helpers used by selection, search
// and link/image lookup.

#include "container_qpainter_p.h"
#include "container_internal.h"

#include <litehtml/render_item.h>

#if QT_CONFIG(clipboard)
#include <QClipboard>
#endif
#include <QDebug>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QTextLayout>

#include <algorithm>
#include <functional>
#include <limits>

using namespace qlitehtml::internal;

namespace {
static Q_LOGGING_CATEGORY(log, "qlitehtml", QtCriticalMsg)
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

} // namespace qlitehtml::internal

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
