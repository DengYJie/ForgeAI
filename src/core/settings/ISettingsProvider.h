#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <memory>

namespace core::settings {
    class ISettingsProvider : public QObject {
        Q_OBJECT

    public:
        explicit ISettingsProvider(QObject *parent = nullptr) : QObject(parent) {
        }

        virtual ~ISettingsProvider() = default;

        virtual QString id() const = 0;

        virtual QString category() const = 0;

        virtual bool useSeparateFile() const = 0;

        virtual QString configFileName() const = 0;

        virtual void fromJson(const QJsonObject &json) = 0;

        virtual void saveToJson(QJsonObject &json) const = 0;

    signals:
        void dataChanged();
    };
} // namespace core::settings

#define _SETTINGS_PROVIDER_CONCAT_IMPL(a, b) a##b
#define _SETTINGS_PROVIDER_CONCAT(a, b) _SETTINGS_PROVIDER_CONCAT_IMPL(a, b)

#define _REGISTER_SETTINGS_PROVIDER_IMPL2(ProviderClass, Line) \
    namespace { \
        struct _SETTINGS_PROVIDER_CONCAT(_SettingsProviderAutoRegister_, Line) { \
            _SETTINGS_PROVIDER_CONCAT(_SettingsProviderAutoRegister_, Line)() { \
                ::core::settings::SettingsRegistry::instance().registerProvider( \
                    std::make_shared<ProviderClass>()); \
            } \
        } _SETTINGS_PROVIDER_CONCAT(_s_auto_register_provider_, Line); \
    }

#define _REGISTER_SETTINGS_PROVIDER_IMPL(ProviderClass, Counter) \
    _REGISTER_SETTINGS_PROVIDER_IMPL2(ProviderClass, Counter)

#define REGISTER_SETTINGS_PROVIDER(ProviderClass) \
    _REGISTER_SETTINGS_PROVIDER_IMPL(ProviderClass, __COUNTER__)
