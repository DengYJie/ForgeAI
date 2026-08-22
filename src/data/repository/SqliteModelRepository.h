#pragma once
#include <QSqlDatabase>
#include <QString>
#include <memory>
#include "domain/repository/IModelRepository.h"

namespace data::repository {

    /**
     * @brief 基于 SQLite 的模型与服务商仓储实现
     * @details 支持首次启动自动播种（Seed）静态预设 JSON 到 SQLite。
     */
    class SqliteModelRepository : public domain::repository::IModelRepository {
    public:
        explicit SqliteModelRepository(const QString &connectionName = QStringLiteral("forgeai_db"));
        ~SqliteModelRepository() override = default;

        /**
         * @brief 初始化数据库结构并执行初次播种
         * @param presetJsonPath 静态预设资源路径
         * @return 成功返回 true，失败返回 false
         */
        bool initializeDatabase(const QString &presetJsonPath = QStringLiteral(":/config/models.json"));

        QList<domain::model::ModelProvider> getAllProviders() override;
        std::optional<domain::model::ModelProvider> getProvider(const QString &providerId) override;
        void saveProvider(const domain::model::ModelProvider &provider) override;
        void deleteProvider(const QString &providerId) override;

        QList<domain::model::Model> getEnabledModels() override;
        std::optional<domain::model::Model> getModel(const QString &modelId) override;

        /**
         * @brief 从指定 JSON 文件批量播种数据到 SQLite 表中
         * @param jsonPath JSON 资源或文件路径
         * @param overwriteExisting 是否覆盖已有数据
         */
        bool seedFromPresetJson(const QString &jsonPath, bool overwriteExisting = false);

    private:
        QSqlDatabase getDatabase() const;
        QList<domain::model::Model> getModelsByProviderId(const QString &providerId);

        QString m_connectionName;
    };

} // namespace data::repository
