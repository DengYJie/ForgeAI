#pragma once

#include "qlitehtml_global.h"

#include <QByteArray>
#include <QPaintDevice>
#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QTextDocument>
#include <QUrl>
#include <QVector>
#include <QtCore/Qt>

#include <litehtml/types.h>

#include <functional>
#include <memory>

class DocumentContainerPrivate;
class DocumentContainerContextPrivate;

namespace litehtml {
class document;
class element;
}

class QLITEHTML_EXPORT DocumentContainerContext
{
public:
    DocumentContainerContext();
    ~DocumentContainerContext();

    void setMasterStyleSheet(const QString &css);

private:
    std::unique_ptr<DocumentContainerContextPrivate> d;

    friend class DocumentContainer;
    friend class DocumentContainerPrivate;
};

class QLITEHTML_EXPORT DocumentContainer
{
public:
    DocumentContainer();
    virtual ~DocumentContainer();

public: // outside API
    void setPaintDevice(QPaintDevice *paintDevice);
    void setDocument(const QByteArray &data, DocumentContainerContext *context);
    bool hasDocument() const;
    void setBaseUrl(const QString &url);
    void setScrollPosition(const QPoint &pos);
    void render(int width, int height);
    void draw(QPainter *painter, const QRect &clip);
    int documentWidth() const;
    int documentHeight() const;
    int anchorY(const QString &anchorName) const;

    // these return areas to redraw in document space
    QVector<QRect> mousePressEvent(const QPoint &documentPos,
                                   const QPoint &viewportPos,
                                   Qt::MouseButton button,
                                   Qt::KeyboardModifiers modifiers);
    QVector<QRect> mouseMoveEvent(const QPoint &documentPos, const QPoint &viewportPos);
    QVector<QRect> mouseReleaseEvent(const QPoint &documentPos,
                                     const QPoint &viewportPos,
                                     Qt::MouseButton button);
    QVector<QRect> mouseDoubleClickEvent(const QPoint &documentPos,
                                         const QPoint &viewportPos,
                                         Qt::MouseButton button);
    QVector<QRect> leaveEvent();

    QUrl linkAt(const QPoint &documentPos, const QPoint &viewportPos);
    QUrl imageAt(const QPoint &documentPos, const QPoint &viewportPos);

    QString caption() const;
    QString selectedText() const;
    QString selectedHtml() const;

    void findText(const QString &text,
                  QTextDocument::FindFlags flags,
                  bool incremental,
                  bool *wrapped,
                  bool *success,
                  QVector<QRect> *oldSelection,
                  QVector<QRect> *newSelection);

    void setDefaultFont(const QFont &font);
    QFont defaultFont() const;

    enum class ResourceType {
        Image,
        StyleSheet,
        Font
    };
    using ResourceHandler = std::function<QByteArray(const QUrl &url, ResourceType type)>;
    void setResourceHandler(const ResourceHandler &handler);
    // Custom element factory: called from create_element for the registered
    // tag; return nullptr to fall through to the default (no element).
    // The checkbox factory for "input[type=checkbox]" is registered by default.
    using ElementFactory = std::function<std::shared_ptr<litehtml::element>(
        const char *tagName,
        const litehtml::string_map &attributes,
        const std::shared_ptr<litehtml::document> &doc)>;
    void registerElementFactory(const QByteArray &tagName, const ElementFactory &factory);

    // Appends HTML to the end of the current document (body), re-renders and
    // incrementally updates the search index. Creates the document if needed.
    void appendHtml(const QByteArray &html);

    // Drops all decoded images from the cache. The cache survives setDocument()
    // by default so streaming re-renders reuse decoded pixmaps; call this to
    // force resources to be fetched again (e.g. explicit refresh).
    void clearResourceCache();

    // Document-coordinate rects of position:fixed elements, used by the
    // widget to repaint them after scrolling.
    QVector<QRect> fixedBoxes() const;

    // Document-coordinate rects of the current selection highlight.
    QVector<QRect> selectionRects() const;

    using CursorCallback = std::function<void(QCursor)>;
    void setCursorCallback(const CursorCallback &callback);

    using LinkCallback = std::function<void(QUrl)>;
    void setLinkCallback(const LinkCallback &callback);

    using PaletteCallback = std::function<QPalette()>;
    void setPaletteCallback(const PaletteCallback &callback);

    using ClipboardCallback = std::function<void(bool)>;
    void setClipboardCallback(const ClipboardCallback &callback);

    // Called (main thread) when an asynchronously loaded image finished and
    // the document was re-laid-out; the widget should repaint the viewport.
    // The DataCallback is invoked on a worker thread.
    using RepaintCallback = std::function<void()>;
    void setRepaintCallback(const RepaintCallback &callback);

    int withFixedElementPosition(int y, const std::function<void()> &action);

private:
    // Shared so async image tasks can hold a weak reference and safely skip
    // completion handling after destruction.
    std::shared_ptr<DocumentContainerPrivate> d;
};
