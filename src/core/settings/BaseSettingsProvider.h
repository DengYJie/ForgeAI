#pragma once

#include <QObject>
#include <QVariantMap>
#include "ISettingsProvider.h"

namespace core::settings {
    /**
     * @brief 强类型配置键定义
     */
    template<typename T>
    struct SettingKey {
        QString name;
        T defaultValue;

        SettingKey(const QString &name, const T &defaultValue)
            : name(name), defaultValue(defaultValue) {
        }
    };

    /**
     * @brief 自动处理序列化的基类
     */
    class BaseSettingsProvider : public ISettingsProvider {
    public:
        explicit BaseSettingsProvider(QObject *parent = nullptr) : ISettingsProvider(parent) {
        }

        template<typename T>
        T get(const SettingKey<T> &key) const {
            if (!m_preferences.contains(key.name)) {
                return key.defaultValue;
            }
            QVariant var = m_preferences.value(key.name);

            if constexpr (std::is_enum_v<T>) {
                return static_cast<T>(var.toInt());
            } else if constexpr (std::is_same_v<T, int>) {
                return var.toInt();
            } else if constexpr (std::is_same_v<T, bool>) {
                return var.toBool();
            } else if constexpr (std::is_same_v<T, double>) {
                return var.toDouble();
            } else if constexpr (std::is_same_v<T, float>) {
                return var.toFloat();
            } else if constexpr (std::is_same_v<T, QString>) {
                return var.toString();
            } else {
                return var.template value<T>();
            }
        }

        template<typename T>
        void set(const SettingKey<T> &key, const T &value) {
            QVariant newVal;
            if constexpr (std::is_enum_v<T>) {
                newVal = static_cast<int>(value);
            } else {
                newVal = QVariant::fromValue(value);
            }

            if (m_preferences.value(key.name) != newVal) {
                m_preferences.insert(key.name, newVal);
                emit dataChanged();
                onSettingChanged(key.name);
            }
        }

        void fromJson(const QJsonObject &json) override {
            QVariantMap newPrefs;
            for (auto it = json.begin(); it != json.end(); ++it) {
                newPrefs.insert(it.key(), it.value().toVariant());
            }

            if (m_preferences != newPrefs) {
                m_preferences = newPrefs;
                emit dataChanged();
                onSettingsLoaded();
            }
        }

        void saveToJson(QJsonObject &json) const override {
            for (auto it = m_preferences.begin(); it != m_preferences.end(); ++it) {
                json.insert(it.key(), QJsonValue::fromVariant(it.value()));
            }
        }

    protected:
        virtual void onSettingChanged(const QString &key) { Q_UNUSED(key) }

        virtual void onSettingsLoaded() {
        }

    private:
        QVariantMap m_preferences;
    };
} // namespace core::settings
