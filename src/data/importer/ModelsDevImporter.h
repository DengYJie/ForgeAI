#pragma once
#include <QJsonObject>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringView>
#include <utility>

#include "domain/model/CanonicalModel.h"
#include "domain/model/ModelProvider.h"
#include "domain/model/ProviderModel.h"

namespace data::importer {

    /**
     * @brief models.dev 导入结果结构
     */
    struct ModelsDevImportResult {
        QHash<QString, domain::model::CanonicalModel> canonicalModels;
        QList<domain::model::ModelProvider>          providers;
        QList<domain::model::ProviderModel>          providerModels;
        int unresolvedBindingsCount = 0;
    };

    /**
     * @brief models.dev 数据源（api.json 与 models.json）格式解析器
     *
     * 唯一感知 models.dev 字段名（npm/api/env/doc/modalities/limit/cost 等）的组件。
     * 将外部 JSON 结构映射为正交解耦的领域对象，隔离外部数据源变化对领域层的影响。
     */
    class ModelsDevImporter {
    public:
        /**
         * @brief 解析 models.json 与 api.json 并建立规范化关联
         * @param apiRoot api.json 根对象（包含 Provider 及其提供的模型挂载）
         * @param modelsRoot models.json 根对象（包含 Canonical 模型本体）
         */
        static ModelsDevImportResult parseAll(const QJsonObject &apiRoot, const QJsonObject &modelsRoot);

        /**
         * @brief 解析 models.json 根对象，提取所有 Canonical 模型本体
         */
        static QHash<QString, domain::model::CanonicalModel> parseCanonicalModels(const QJsonObject &modelsRoot);

        /**
         * @brief 解析 api.json 根对象，提取所有 ModelProvider 与 ProviderModel 挂载
         */
        static std::pair<QList<domain::model::ModelProvider>, QList<domain::model::ProviderModel>>
        parseProvidersAndBindings(const QJsonObject &apiRoot);

        /**
         * @brief 解析单个 Canonical 模型对象
         */
        static domain::model::CanonicalModel parseCanonicalModel(const QString &modelKey, const QJsonObject &modelObj);

        /**
         * @brief 解析单个 Provider 对象元数据
         */
        static domain::model::ModelProvider parseProvider(const QString &id, const QJsonObject &providerObj);

        /**
         * @brief 解析单个 ProviderModel 挂载对象
         */
        static domain::model::ProviderModel parseProviderModel(
            const QString &providerId,
            const QString &providerName,
            const QString &modelKey,
            const QJsonObject &modelObj
        );

        /**
         * @brief 将 models.dev npm 驱动包名映射到领域 ProviderType
         */
        static domain::model::ProviderType mapNpmToProviderType(QStringView npm);
    };

} // namespace data::importer
