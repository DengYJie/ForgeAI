#pragma once
#include <QSqlDatabase>
#include <QString>
#include <memory>
#include "domain/repository/IModelRepository.h"

namespace data::repository {

    /**
     * @brief 基于 SQLite 的规范化模型与服务商仓储实现
     * @details 维护 canonical_models, model_providers, provider_models 三张规范化关系表。
     * 支持首次启动与模型资源更新时的哈希校验及原子事务批量写入，并保护用户自定义数据。
     */
    class SqliteModelRepository : public domain::repository::IModelRepository {
    public:
        explicit SqliteModelRepository(const QString &connectionName = QStringLiteral("forgeai_db"));
        ~SqliteModelRepository() override = default;

        /**
         * @brief 初始化数据库表结构并执行初次播种与哈希校验
         * @param apiJsonPath api.json 静态资源路径
         * @param modelsJsonPath models.json 静态资源路径
         * @return 成功返回 true，失败返回 false
         */
        bool initializeDatabase(
            const QString &apiJsonPath = QStringLiteral(":/config/api.json"),
            const QString &modelsJsonPath = QStringLiteral(":/config/models.json")
        );

        QList<domain::model::ModelProvider> getAllProviders() override;
        QList<domain::model::ModelProvider> getEnabledProviders() override;
        std::optional<domain::model::ModelProvider> getProvider(const QString &providerId) override;
        void saveProvider(const domain::model::ModelProvider &provider) override;
        void deleteProvider(const QString &providerId) override;

        std::optional<domain::model::CanonicalModel> getCanonicalModel(const QString &modelId) override;
        QList<domain::model::CanonicalModel> getAllCanonicalModels() override;

        QList<domain::model::ProviderModel> getProviderModels(const QString &providerId) override;
        void saveProviderModel(const domain::model::ProviderModel &binding) override;
        void deleteProviderModel(const QString &providerId, const QString &remoteModelId) override;

        QList<domain::model::ResolvedModel> getResolvedModelsForProvider(const QString &providerId) override;
        QList<domain::model::ResolvedModel> getAllResolvedModels() override;
        QList<domain::model::ResolvedModel> getEnabledResolvedModels() override;
        std::optional<domain::model::ResolvedModel> resolveModel(const QString &providerId, const QString &remoteModelId) override;

        /**
         * @brief 从 api.json 和 models.json 批量更新/播种 models.dev 数据
         * @param apiJsonPath api.json 文件/资源路径
         * @param modelsJsonPath models.json 文件/资源路径
         * @param force 是否强制覆盖更新
         */
        bool seedFromPresetJson(
            const QString &apiJsonPath,
            const QString &modelsJsonPath,
            bool force = false
        );

    private:
        QSqlDatabase getDatabase() const;
        QString getMetadata(const QString &key) const;
        void setMetadata(const QString &key, const QString &value);

        QString m_connectionName;
    };

} // namespace data::repository
