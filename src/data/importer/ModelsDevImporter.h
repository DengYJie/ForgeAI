#pragma once
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringView>
#include "domain/model/Model.h"
#include "domain/model/ModelProvider.h"

namespace data::importer {

    /**
     * @brief models.dev api.json 格式解析器
     *
     * 唯一感知 models.dev 字段名（npm/api/env/doc/modalities 等）的组件。
     * 将外部 JSON 结构映射为领域对象，隔离外部数据源变化对领域层的影响。
     */
    class ModelsDevImporter {
    public:
        struct ParseResult {
            QList<domain::model::ModelProvider> providers;
            QList<domain::model::Model>         models;
        };

        /**
         * @brief 解析完整的 api.json 根对象
         */
        static ParseResult parseAll(const QJsonObject &root);

        /**
         * @brief 解析单个 Provider 对象（不含 models 列表的展开，仅自身元数据）
         */
        static domain::model::ModelProvider parseProvider(const QString &id, const QJsonObject &providerObj);

        /**
         * @brief 解析单个 Model 对象
         */
        static domain::model::Model parseModel(
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
