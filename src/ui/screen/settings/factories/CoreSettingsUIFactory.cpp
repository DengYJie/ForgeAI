#include "CoreSettingsUIFactory.h"
#include "core/settings/SettingsRegistry.h"
#include "core/settings/providers/CoreSettingsProvider.h"
#include "ui/screen/settings/SettingsUIRegistry.h"
#include <FluentQt/Design.h>
#include <FluentQt/BasicInput.h>

namespace ui::screen::settings {
    QString CoreSettingsUIFactory::iconGlyph() const {
        return Typography::Icons::Brightness;
    }

    QString CoreSettingsUIFactory::title() const {
        return "App theme";
    }

    QString CoreSettingsUIFactory::subtitle() const {
        return "Select which app theme to display";
    }

    QWidget *CoreSettingsUIFactory::createControlWidget(QWidget *parent) {
        auto providerBase = core::settings::SettingsRegistry::instance().getProvider("core");
        auto provider = std::dynamic_pointer_cast<core::settings::CoreSettingsProvider>(providerBase);

        if (!provider) return new QWidget(parent);

        auto *themeCombo = new fluent::basicinput::ComboBox(parent);
        themeCombo->addItems({"System", "Light", "Dark"});
        themeCombo->setMinimumWidth(130);

        int currentMode = static_cast<int>(provider->themeMode());
        themeCombo->setCurrentIndex(currentMode);

        QObject::connect(themeCombo, qOverload<int>(&fluent::basicinput::ComboBox::currentIndexChanged),
                         [provider](int idx) {
                             provider->setThemeMode(static_cast<core::settings::CoreSettingsProvider::ThemeMode>(idx));
                         });

        QObject::connect(provider.get(), &core::settings::CoreSettingsProvider::themeModeChanged, themeCombo,
                         [themeCombo](core::settings::CoreSettingsProvider::ThemeMode mode) {
                             int index = static_cast<int>(mode);
                             if (themeCombo->currentIndex() != index) {
                                 themeCombo->setCurrentIndex(index);
                             }
                         });

        return themeCombo;
    }
} // namespace ui::screen::settings

REGISTER_SETTINGS_UI (ui::screen::settings::CoreSettingsUIFactory)
