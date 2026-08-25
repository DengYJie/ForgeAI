#pragma once

#include <QWidget>
#include <FluentQt/Foundation.h>
#include <FluentQt/FluentQt.h>

namespace ui::screen::work {

class ProjectHeader final : public QWidget, public fluent::FluentElement {
    Q_OBJECT

public:
    explicit ProjectHeader(QWidget* parent = nullptr);
    ~ProjectHeader() override = default;

    void setTitle(const QString& title);
    QString title() const { return m_title; }

    void setExpanded(bool expanded);
    bool isExpanded() const { return m_isExpanded; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    void expandToggled(bool expanded);
    void addProjectClicked();
    void moreProjectsClicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    bool event(QEvent* event) override;
    void onThemeUpdated() override;

private:
    enum ButtonType { None, Chevron, More, Add };

    ButtonType hitTest(const QPoint& pos) const;
    QRect chevronRect() const;
    QRect moreRect() const;
    QRect addRect() const;

    QString m_title{tr("项目")};
    bool m_isExpanded{true};
    bool m_isHovered{false};
    ButtonType m_hoverButton{None};
    ButtonType m_pressButton{None};

    void showToolTip(const QString& text, const QRect& targetRect);
    void hideToolTip();
    mutable QPointer<QWidget> m_tooltip;
};

} // namespace ui::screen::work
