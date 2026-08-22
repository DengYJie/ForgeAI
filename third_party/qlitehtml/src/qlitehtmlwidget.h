#pragma once

#include "qlitehtml_global.h"
#include "qlitehtml_types.h"

#include <QAbstractScrollArea>
#include <QTextDocument>

#include <functional>

class QLiteHtmlWidgetPrivate;

class QLITEHTML_EXPORT QLiteHtmlWidget : public QAbstractScrollArea
{
    Q_OBJECT
public:
    explicit QLiteHtmlWidget(QWidget *parent = nullptr);
    ~QLiteHtmlWidget() override;

    // declaring the getters Q_INVOKABLE to make them Squish-testable
    void setUrl(const QUrl &url);
    Q_INVOKABLE QUrl url() const;
    void setHtml(const QString &content);
    // Appends HTML to the current document (for streaming/chat content) and
    // re-renders. Incrementally updates the search index.
    void appendHtml(const QString &content);
    Q_INVOKABLE QString html() const;
    Q_INVOKABLE QString title() const;

    void setZoomFactor(qreal scale);
    qreal zoomFactor() const;

    bool findText(const QString &text,
                  QTextDocument::FindFlags flags,
                  bool incremental,
                  bool *wrapped = nullptr);

    void setDefaultFont(const QFont &font);
    QFont defaultFont() const;

    void scrollToAnchor(const QString &name);

    using ResourceType = qlitehtml::ResourceType;
    using ResourceHandler = qlitehtml::ResourceHandler;
    void setResourceHandler(const ResourceHandler &handler);
    void setAllowNetworkAccess(bool allow);
    bool allowNetworkAccess() const;
    void clearResourceCache();

    // declaring this Q_INVOKABLE to make it Squish-testable
    Q_INVOKABLE QString selectedText() const;
    Q_INVOKABLE QString selectedHtml() const;

signals:
    void linkClicked(const QUrl &url);
    void linkHighlighted(const QUrl &url);
    void formControlActivated(const QString &tag, const QString &type, const QString &name, const QString &value, bool checked);
    void detailsToggled(const QString &id, bool open);
    void copyAvailable(bool available);
    void contextMenuRequested(const QPoint &pos, const QUrl &linkUrl, const QUrl &imageUrl);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void updateHightlightedLink();
    void setHightlightedLink(const QUrl &url);
    void updateSelection(const QPoint &position);
    void scrollSelection();
    void withFixedTextPosition(const std::function<void()> &action);
    void render();
    void smoothScrollTo(const QPoint &target);
    QPoint scrollPosition() const;
    // Input coordinates are always local to viewport(). QAbstractScrollArea
    // remaps its mouse and wheel handlers to viewport events.
    void htmlPos(const QPoint &viewportPoint, QPoint *viewportPos, QPoint *htmlPos) const;
    QPoint toVirtual(const QPoint &p) const;
    QSize toVirtual(const QSize &s) const;
    QRect toVirtual(const QRect &r) const;
    QRect fromVirtual(const QRect &r) const;

    QLiteHtmlWidgetPrivate *d;
};
