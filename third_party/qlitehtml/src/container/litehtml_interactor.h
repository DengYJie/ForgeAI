#pragma once

#include "litehtml_renderer.h"

#include <litehtml.h>
#include <QPoint>
#include <QRect>
#include <QVector>
#include <QString>
#include <QUrl>
#include <QCursor>
#include <QHash>
#include <QSet>
#include <QTextDocument>
#include <functional>
#include <memory>
#include <unordered_map>

class details_element; // Forward declaration

namespace qlitehtml {
namespace internal {

struct Index
{
    QString text;
    // only contains leaf elements
    std::unordered_map<litehtml::element::ptr, int> elementToIndex;

    using Entry = std::pair<int, litehtml::element::ptr>;
    std::vector<Entry> indexToElement;

    Entry findElement(int index) const;
};

class Selection
{
public:
    struct Element
    {
        Element() = default;
        Element(litehtml::element::ptr p, int idx, int _x)
            : element(p)
            , index(idx)
            , x(_x)
        {}
        litehtml::element::ptr element;
        int index = -1;
        int x = -1;
    };

    using SegmentInfo = SelectionSegmentInfo;

    enum class Mode { Free, Word };

    bool isValid() const;

    void update();
    QRect boundingRect() const;

    Element startElem;
    Element endElem;
    QVector<QRect> selection;
    QString text;

    QHash<QRect, SegmentInfo> segmentMap;
    QPoint selectionStartDocumentPos;
    bool isSelecting = false;
    Mode mode = Mode::Free;
};

class LiteHtmlInteractor {
public:
    using CursorCallback = std::function<void(const QCursor &)>;
    using LinkCallback = std::function<void(const QUrl &)>;
    using FormControlCallback = std::function<void(const QString &, const QString &, const QString &, const QString &, bool)>;
    using DetailsCallback = std::function<void(const QString &, bool)>;
    using ClipboardCallback = std::function<void(bool)>;
    using RelayoutCallback = std::function<void()>;

    LiteHtmlInteractor() = default;
    ~LiteHtmlInteractor() = default;

    void clear();

    void setBaseUrl(const QString &url) { m_baseUrl = url; }
    void setDocument(const litehtml::document::ptr &doc) { m_document = doc; }
    void setClientRect(const QRect &rect) { m_clientRect = rect; }

    void setCursorCallback(const CursorCallback &cb) { m_cursorCallback = cb; }
    void setLinkCallback(const LinkCallback &cb) { m_linkCallback = cb; }
    void setFormControlCallback(const FormControlCallback &cb) { m_formControlCallback = cb; }
    void setDetailsCallback(const DetailsCallback &cb) { m_detailsCallback = cb; }
    void setClipboardCallback(const ClipboardCallback &cb) { m_clipboardCallback = cb; }
    void setRelayoutCallback(const RelayoutCallback &cb) { m_relayoutCallback = cb; }

    // Interactor implementations for litehtml callbacks
    void on_anchor_click(const char *url, const litehtml::element::ptr &el);
    bool on_element_click(const litehtml::element::ptr &el);
    void on_mouse_event(const litehtml::element::ptr &el, litehtml::mouse_event event);
    void set_cursor(const char *cursor);

    // Mouse and layout events from frontend
    QVector<QRect> mousePressEvent(const QPoint &documentPos, const QPoint &viewportPos, Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
    QVector<QRect> mouseMoveEvent(const QPoint &documentPos, const QPoint &viewportPos);
    QVector<QRect> mouseReleaseEvent(const QPoint &documentPos, const QPoint &viewportPos, Qt::MouseButton button);
    QVector<QRect> mouseDoubleClickEvent(const QPoint &documentPos, const QPoint &viewportPos, Qt::MouseButton button);
    QVector<QRect> leaveEvent();
    QVector<QRect> scrollAt(const QPoint &documentPos, const QPoint &viewportPos, const QPoint &delta);

    QUrl linkAt(const QPoint &documentPos, const QPoint &viewportPos) const;
    QUrl imageAt(const QPoint &documentPos, const QPoint &viewportPos) const;

    // Text selection and indexing
    void buildIndex();
    void updateIndex();
    void updateSelection();
    void clearSelection();
    void findText(const QString &text,
                  QTextDocument::FindFlags flags,
                  bool incremental,
                  bool *wrapped,
                  bool *success,
                  QVector<QRect> *oldSelection,
                  QVector<QRect> *newSelection);

    QString selectedText() const { return m_selection.isValid() ? m_selection.text : QString(); }
    const QVector<QRect>& selectionRects() const { return m_selection.selection; }
    const Selection& selection() const { return m_selection; }
    const Selection::SegmentInfo* getSelectionSegmentInfo(const QRect &placementRect, Selection::SegmentInfo &localSeg) const;

    // Helpers
    std::shared_ptr<details_element> detailsForSummary(const litehtml::element::ptr &element) const;
    QUrl resolveUrl(const QString &url, const QString &baseUrl) const;

private:
    CursorCallback m_cursorCallback;
    LinkCallback m_linkCallback;
    FormControlCallback m_formControlCallback;
    DetailsCallback m_detailsCallback;
    ClipboardCallback m_clipboardCallback;
    RelayoutCallback m_relayoutCallback;

    QString m_baseUrl;
    QRect m_clientRect;
    litehtml::document::ptr m_document;
    std::weak_ptr<details_element> m_pressedDetails;
    bool m_blockLinks = false;

    Index m_index;
    Selection m_selection;
    litehtml::element::ptr m_lastIndexedElement;
};

} // namespace internal
} // namespace qlitehtml
