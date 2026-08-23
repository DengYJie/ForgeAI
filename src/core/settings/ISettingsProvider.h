#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <memory>

namespace core::settings {
    /**
     * @brief 设置提供者抽象基类接口
     * @details 负责具体模块配置数据的标识、分类、序列化策略以及持久化 JSON 转换
     */
    class ISettingsProvider : public QObject {
        Q_OBJECT

    public:
        explicit ISettingsProvider(QObject *parent = nullptr) : QObject(parent) {
        }

        virtual ~ISettingsProvider() = default;

        /**
         * @brief 获取当前提供者的唯一标识符
         * @return 稳定唯一的 Provider ID (如 "appearance", "logging", "model")
         */
        virtual QString id() const = 0;

        /**
         * @brief 获取当前提供者所属的分类显示名称
         * @return 分类显示名称 (如 "外观与行为")
         */
        virtual QString category() const = 0;

        /**
         * @brief 是否采用独立的独立配置文件持久化
         * @return true 存储于独立配置文件，false 汇聚存储于全局 config.json
         */
        virtual bool useSeparateFile() const = 0;

        /**
         * @brief 独立持久化时的配置文件名（仅当 useSeparateFile 为 true 时有效）
         * @return 配置文件名 (如 "logging.json")
         */
        virtual QString configFileName() const = 0;

        /**
         * @brief 从 JSON 对象反序列化并更新内存配置
         * @param json 传入的配置 JSON 对象
         */
        virtual void fromJson(const QJsonObject &json) = 0;

        /**
         * @brief 将当前内存配置序列化并输出至 JSON 对象
         * @param json 待写入的目标 JSON 对象
         */
        virtual void saveToJson(QJsonObject &json) const = 0;

    signals:
        /**
         * @brief 配置数据发生变更信号
         * @details 当任何内部配置项发生修改时触发，用于驱动 SettingsRegistry 触发防抖保存并通知 ViewModel
         */
        void dataChanged();
    };
} // namespace core::settings
