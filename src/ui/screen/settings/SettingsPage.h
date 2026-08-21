#pragma once

#include <QWidget>
#include <QList>
#include <FluentQt/Design.h>
#include <FluentQt/Foundation.h>

class QVBoxLayout;

namespace fluent::textfields {
    class Label;
}

namespace ui::screen::settings {
    class SettingsCardItem;

    /**
     * @brief 设置界面，遵循 WinUI 3 Gallery / Windows 11 设置页面规范
     */
    class SettingsPage : public QWidget, public fluent::FluentElement, public fluent::QMLPlus {
        Q_OBJECT

    public:
        explicit SettingsPage(QWidget *parent = nullptr);

        ~SettingsPage() override = default;

        void onThemeUpdated() override;

    protected:
        void resizeEvent(QResizeEvent *event) override;

    private:
        void setupUi();

        void updateResponsiveLayout();

        QWidget *createSectionHeader(const QString &title);

        QWidget *createSettingsCard(const QString &iconGlyph,
                                    const QString &title,
                                    const QString &subtitle,
                                    QWidget *trailingWidget);

        QWidget *m_viewport = nullptr;
        QVBoxLayout *m_contentLayout = nullptr;
        fluent::textfields::Label *m_titleLabel = nullptr;
        QList<SettingsCardItem *> m_cards;
    };
} // namespace ui::screen::settings
