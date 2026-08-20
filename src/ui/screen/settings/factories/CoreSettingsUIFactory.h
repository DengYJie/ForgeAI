#pragma once
#include "../ISettingsUIFactory.h"

namespace ui::screen::settings {
    class CoreSettingsUIFactory : public ISettingsUIFactory {
    public:
        QString targetProviderId() const override { return "core"; }

        QString iconGlyph() const override;

        QString title() const override;

        QString subtitle() const override;

        QWidget *createControlWidget(QWidget *parent) override;
    };
} // namespace ui::screen::settings
