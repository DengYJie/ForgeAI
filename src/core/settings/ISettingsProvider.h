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

#define _REGISTER_SETTINGS_PROVIDER_IMPL(ProviderClass, Counter) \
    namespace { \
        struct _SettingsProviderAutoRegister_##Counter { \
            _SettingsProviderAutoRegister_##Counter() { \
                ::core::settings::SettingsRegistry::instance().registerProvider( \
                    std::make_shared<ProviderClass>()); \
            } \
        } _s_auto_register_provider_##Counter; \
    }

#define REGISTER_SETTINGS_PROVIDER(ProviderClass) \
    _REGISTER_SETTINGS_PROVIDER_IMPL(ProviderClass, __COUNTER__)
