#pragma once
#include <QString>
#include <QWidget>
#include <memory>

namespace ui::screen::settings {
    class ISettingsUIFactory {
    public:
        virtual ~ISettingsUIFactory() = default;

        virtual QString targetProviderId() const = 0;

        virtual QString iconGlyph() const = 0;

        virtual QString title() const = 0;

        virtual QString subtitle() const = 0;

        virtual QWidget *createControlWidget(QWidget *parent) = 0;
    };
} // namespace ui::screen::settings

#define _REGISTER_SETTINGS_UI_IMPL(FactoryClass, Counter) \
    namespace { \
        struct _SettingsUIAutoRegister_##Counter { \
            _SettingsUIAutoRegister_##Counter() { \
                ::ui::screen::settings::SettingsUIRegistry::instance().registerFactory( \
                    std::make_shared<FactoryClass>()); \
            } \
        } _s_auto_register_ui_##Counter; \
    }

#define REGISTER_SETTINGS_UI(FactoryClass) \
    _REGISTER_SETTINGS_UI_IMPL(FactoryClass, __COUNTER__)
