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

#define _SETTINGS_UI_CONCAT_IMPL(a, b) a##b
#define _SETTINGS_UI_CONCAT(a, b) _SETTINGS_UI_CONCAT_IMPL(a, b)

#define _REGISTER_SETTINGS_UI_IMPL2(FactoryClass, Line) \
    namespace { \
        struct _SETTINGS_UI_CONCAT(_SettingsUIAutoRegister_, Line) { \
            _SETTINGS_UI_CONCAT(_SettingsUIAutoRegister_, Line)() { \
                ::ui::screen::settings::SettingsUIRegistry::instance().registerFactory( \
                    std::make_shared<FactoryClass>()); \
            } \
        } _SETTINGS_UI_CONCAT(_s_auto_register_ui_, Line); \
    }

#define _REGISTER_SETTINGS_UI_IMPL(FactoryClass, Counter) \
    _REGISTER_SETTINGS_UI_IMPL2(FactoryClass, Counter)

#define REGISTER_SETTINGS_UI(FactoryClass) \
    _REGISTER_SETTINGS_UI_IMPL(FactoryClass, __COUNTER__)
