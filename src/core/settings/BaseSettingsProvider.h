#pragma once

#include <QObject>
#include <QVariantMap>
#include "ISettingsProvider.h"

namespace core::settings {
    /**
     * @brief 强类型配置键定义模板
     * @tparam T 配置项值的 C++ 类型 (如 int, bool, QString, enum 等)
     */
    template<typename T>
    struct SettingKey {
        QString name;    ///< 配置键名 (在 JSON 中的字段名)
        T defaultValue;  ///< 配置默认值

        SettingKey(const QString &name, const T &defaultValue)
            : name(name), defaultValue(defaultValue) {
        }
    };

    /**
     * @brief 基于内存 QVariantMap 与类型化转换的设置提供者基类
     * @details 提供类型安全的 get/set 操作与通用 JSON 序列化/反序列化实现
     */
    class BaseSettingsProvider : public ISettingsProvider {
    public:
        explicit BaseSettingsProvider(QObject *parent = nullptr) : ISettingsProvider(parent) {
        }

        /**
         * @brief 获取指定配置键的当前值
         * @tparam T 配置值类型
         * @param key 强类型配置键
         * @return 当前存储的值，若不存在则返回 key.defaultValue
         */
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

        /**
         * @brief 设置指定配置键的值
         * @tparam T 配置值类型
         * @param key 强类型配置键
         * @param value 新值
         * @details 当值发生实质变更时，更新内部映射表并触发 dataChanged 信号及 onSettingChanged 钩子
         */
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

        /**
         * @brief 从 JSON 对象加载配置
         * @param json JSON 对象
         */
        void fromJson(const QJsonObject &json) override {
            QVariantMap newPrefs;
            for (auto it = json.begin(); it != json.end(); ++it) {
                newPrefs.insert(it.key(), it.value().toVariant());
            }

            const bool changed = (m_preferences != newPrefs);
            m_preferences = newPrefs;
            if (changed) {
                emit dataChanged();
            }
            onSettingsLoaded();
        }

        /**
         * @brief 将内部配置保存到 JSON 对象
         * @param json 目标 JSON 对象
         */
        void saveToJson(QJsonObject &json) const override {
            for (auto it = m_preferences.begin(); it != m_preferences.end(); ++it) {
                json.insert(it.key(), QJsonValue::fromVariant(it.value()));
            }
        }

    protected:
        /**
         * @brief 单项设置修改后触发的虚钩子函数
         * @param key 发生变化的键名
         */
        virtual void onSettingChanged(const QString &key) { Q_UNUSED(key) }

        /**
         * @brief 全量配置加载完成后触发的虚钩子函数
         */
        virtual void onSettingsLoaded() {
        }

    private:
        QVariantMap m_preferences;
    };
} // namespace core::settings
