#pragma once

#include <QList>
#include "ui/base/BasePage.h"

class QVBoxLayout;

namespace fluent::textfields {
    class Label;
}

namespace ui::screen::settings {
    class SettingsCardItem;
    class SettingsViewModel;
    struct SettingsState;

    /**
     * @brief 设置主界面 (纯 View)，接收注入的 SettingsViewModel
     */
    class SettingsPage : public ui::base::BasePage {
        Q_OBJECT

    public:
        explicit SettingsPage(
            SettingsViewModel *viewModel = nullptr,
            QWidget *parent = nullptr
        );

        ~SettingsPage() override = default;

        void onThemeUpdated() override;

    protected:
        void updateResponsiveLayout(int availableWidth) override;

    private:
        void setupUi();

        void render(const SettingsState &state);

        QWidget *createSectionHeader(const QString &title);

        QWidget *createSettingsCard(const QString &iconGlyph,
                                    const QString &title,
                                    const QString &subtitle,
                                    QWidget *trailingWidget);

        SettingsViewModel *m_viewModel = nullptr;
        QWidget *m_viewport = nullptr;
        QVBoxLayout *m_contentLayout = nullptr;
        fluent::textfields::Label *m_titleLabel = nullptr;
        QList<SettingsCardItem *> m_cards;
    };
} // namespace ui::screen::settings
