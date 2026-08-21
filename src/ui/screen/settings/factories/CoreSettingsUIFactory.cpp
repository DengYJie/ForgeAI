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
        return "应用主题";
    }

    QString CoreSettingsUIFactory::subtitle() const {
        return "选择要显示的应用主题";
    }

    QWidget *CoreSettingsUIFactory::createControlWidget(QWidget *parent) {
        auto providerBase = core::settings::SettingsRegistry::instance().getProvider("core");
        auto provider = std::dynamic_pointer_cast<core::settings::CoreSettingsProvider>(providerBase);

        if (!provider) return new QWidget(parent);

        auto *themeCombo = new fluent::basicinput::ComboBox(parent);
        themeCombo->addItems({"跟随系统", "浅色", "深色"});
        themeCombo->setMinimumWidth(130);

        // 读数据
        int currentMode = static_cast<int>(provider->get(core::settings::CoreSettingsProvider::ThemeModeKey));
        themeCombo->setCurrentIndex(currentMode);

        // 写数据
        QObject::connect(themeCombo, qOverload<int>(&fluent::basicinput::ComboBox::currentIndexChanged),
                         [provider](int idx) {
                             provider->set(core::settings::CoreSettingsProvider::ThemeModeKey, 
                                           static_cast<core::settings::ThemeMode>(idx));
                         });

        // 监听全局数据变化并自动更新 UI
        QObject::connect(provider.get(), &core::settings::ISettingsProvider::dataChanged, themeCombo,
                         [themeCombo, provider]() {
                             int index = static_cast<int>(provider->get(core::settings::CoreSettingsProvider::ThemeModeKey));
                             if (themeCombo->currentIndex() != index) {
                                 themeCombo->setCurrentIndex(index);
                             }
                         });

        return themeCombo;
    }
} // namespace ui::screen::settings

REGISTER_SETTINGS_UI (ui::screen::settings::CoreSettingsUIFactory)
