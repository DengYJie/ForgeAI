#pragma once

#include <QWidget>
#include <QList>
#include <QString>
#include <QPoint>
#include <QVariantAnimation>
#include <FluentQt/Foundation.h>
#include <FluentQt/TextFields.h>

namespace ui::screen::chat {
    struct ChatAnchorItem {
        QString id;
        QString title;
        QString previewText;
    };

    class ChatAnchorPreviewCard : public QWidget, public fluent::FluentElement {
        Q_OBJECT

    public:
        explicit ChatAnchorPreviewCard(QWidget *parent = nullptr);

        void setContent(const QString &title, const QString &body);

        void onThemeUpdated() override;

    protected:
        void paintEvent(QPaintEvent *event) override;

    private:
        fluent::textfields::Label *m_titleLabel = nullptr;
        fluent::textfields::Label *m_bodyLabel = nullptr;
    };

    /**
     * @brief 对话时间线/锚点导航条
     */
    class ChatAnchorBar : public QWidget, public fluent::FluentElement {
        Q_OBJECT

    public:
        explicit ChatAnchorBar(QWidget *parent = nullptr);

        ~ChatAnchorBar() override;

        void setAnchors(const QList<ChatAnchorItem> &anchors);

        void addAnchor(const QString &id, const QString &title, const QString &previewText = QString());

        void removeAnchor(const QString &id);

        void clearAnchors();

        void setActiveIndex(int index);

        int activeIndex() const { return m_activeIndex; }
        int count() const { return m_items.size(); }

        void onThemeUpdated() override;

    Q_SIGNALS:
        void anchorClicked(int index, const QString &id);

    protected:
        void paintEvent(QPaintEvent *event) override;

        void mouseMoveEvent(QMouseEvent *event) override;

        void mousePressEvent(QMouseEvent *event) override;

        void leaveEvent(QEvent *event) override;

    private:
        qreal contentTopOffset() const;

        QRectF itemDashRect(int index) const;

        QRectF itemHitRect(int index) const;

        int indexAtPosition(const QPoint &pos) const;

        void showPreviewAt(int index);

        void hidePreview();

        QList<ChatAnchorItem> m_items;
        int m_activeIndex = -1;
        int m_hoveredIndex = -1;

        qreal m_mouseY = -1000.0;
        qreal m_hoverIntensity = 0.0;
        QVariantAnimation *m_hoverAnim = nullptr;

        ChatAnchorPreviewCard *m_previewCard = nullptr;
    };
} // namespace ui::screen::chat
