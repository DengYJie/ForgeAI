#include "SqliteModelRepository.h"
#include "data/sqlite/SqlTransaction.h"
#include "data/sqlite/SqlHelper.h"
#include "data/importer/ModelsDevImporter.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace data::repository {

    SqliteModelRepository::SqliteModelRepository(const QString &connectionName)
        : m_connectionName(connectionName) {
    }

    QSqlDatabase SqliteModelRepository::getDatabase() const {
        return QSqlDatabase::database(m_connectionName);
    }

    bool SqliteModelRepository::initializeDatabase(const QString &presetJsonPath) {
        auto db = getDatabase();
        if (!db.isOpen()) {
            return false;
        }

        // 创建服务商表
        const QString createProvidersTable = QStringLiteral(
            "CREATE TABLE IF NOT EXISTS model_providers ("
            "    id TEXT PRIMARY KEY,"
            "    name TEXT NOT NULL,"
            "    icon TEXT,"
            "    doc_url TEXT,"
            "    env_var_name TEXT,"
            "    type INTEGER NOT NULL,"
            "    base_url TEXT NOT NULL,"
            "    api_key TEXT,"
            "    custom_headers TEXT,"
            "    proxy_url TEXT,"
            "    timeout_ms INTEGER DEFAULT 60000,"
            "    is_enabled INTEGER DEFAULT 1"
            ");"
        );

        if (!data::sqlite::SqlHelper::exec(createProvidersTable, db)) {
            return false;
        }

        // 创建模型表
        const QString createModelsTable = QStringLiteral(
            "CREATE TABLE IF NOT EXISTS models ("
            "    id TEXT PRIMARY KEY,"
            "    provider_id TEXT NOT NULL,"
            "    display_name TEXT NOT NULL,"
            "    description TEXT,"
            "    family TEXT,"
            "    group_name TEXT,"
            "    context_limit INTEGER DEFAULT 128000,"
            "    max_input_limit INTEGER DEFAULT 128000,"
            "    max_output_limit INTEGER DEFAULT 8192,"
            "    capabilities INTEGER DEFAULT 1,"
            "    default_temperature REAL DEFAULT 0.7,"
            "    default_top_p REAL DEFAULT 1.0,"
            "    default_max_output_tokens INTEGER,"
            "    default_enable_thinking INTEGER DEFAULT 1,"
            "    default_reasoning_effort TEXT,"
            "    default_thinking_budget_tokens INTEGER DEFAULT 4096,"
            "    input_price REAL DEFAULT 0.0,"
            "    output_price REAL DEFAULT 0.0,"
            "    cache_read_price REAL DEFAULT 0.0,"
            "    cache_write_price REAL DEFAULT 0.0,"
            "    currency TEXT DEFAULT 'USD',"
            "    reasoning_field TEXT,"
            "    open_weights INTEGER DEFAULT 0,"
            "    knowledge_cutoff TEXT,"
            "    is_enabled INTEGER DEFAULT 1,"
            "    is_custom INTEGER DEFAULT 0,"
            "    FOREIGN KEY (provider_id) REFERENCES model_providers(id) ON DELETE CASCADE"
            ");"
        );

        if (!data::sqlite::SqlHelper::exec(createModelsTable, db)) {
            return false;
        }

        // 检查数据库是否为空，若为空则执行初次数据播种 (Seed)
        if (data::sqlite::SqlHelper::scalarInt(QStringLiteral("SELECT COUNT(*) FROM model_providers;"), {}, db) == 0) {
            if (!presetJsonPath.isEmpty() && QFile::exists(presetJsonPath)) {
                seedFromPresetJson(presetJsonPath, false);
            }
        }

        return true;
    }

    bool SqliteModelRepository::seedFromPresetJson(const QString &jsonPath, bool overwriteExisting) {
        QFile file(jsonPath);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }

        QByteArray data = file.readAll();
        file.close();

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            return false;
        }

        auto parseResult = data::importer::ModelsDevImporter::parseAll(doc.object());

        auto db = getDatabase();
        data::sqlite::SqlTransaction tx(db);
        if (!tx.isStarted()) {
            return false;
        }

        QSqlQuery providerQuery(db);
        providerQuery.prepare(QStringLiteral(
            "INSERT OR %1 INTO model_providers ("
            "    id, name, icon, doc_url, env_var_name, type, base_url, api_key, custom_headers, proxy_url, timeout_ms, is_enabled"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
        ).arg(overwriteExisting ? QStringLiteral("REPLACE") : QStringLiteral("IGNORE")));

        QSqlQuery modelQuery(db);
        modelQuery.prepare(QStringLiteral(
            "INSERT OR %1 INTO models ("
            "    id, provider_id, display_name, description, family, group_name, context_limit, max_input_limit, max_output_limit, "
            "    capabilities, default_temperature, default_top_p, default_max_output_tokens, default_enable_thinking, "
            "    default_reasoning_effort, default_thinking_budget_tokens, input_price, output_price, cache_read_price, "
            "    cache_write_price, currency, reasoning_field, open_weights, knowledge_cutoff, is_enabled, is_custom"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
        ).arg(overwriteExisting ? QStringLiteral("REPLACE") : QStringLiteral("IGNORE")));

        for (const auto &provider : parseResult.providers) {
            providerQuery.bindValue(0, provider.id);
            providerQuery.bindValue(1, provider.name);
            providerQuery.bindValue(2, provider.icon);
            providerQuery.bindValue(3, provider.docUrl);
            providerQuery.bindValue(4, provider.envVarName);
            providerQuery.bindValue(5, static_cast<int>(provider.type));
            providerQuery.bindValue(6, provider.baseUrl);
            providerQuery.bindValue(7, provider.apiKey);
            providerQuery.bindValue(8, QStringLiteral("{}"));
            providerQuery.bindValue(9, provider.proxyUrl.has_value() ? QVariant(provider.proxyUrl.value()) : QVariant());
            providerQuery.bindValue(10, provider.timeoutMs);
            providerQuery.bindValue(11, provider.isEnabled ? 1 : 0);
            providerQuery.exec();
        }

        for (const auto &model : parseResult.models) {
            modelQuery.bindValue(0, model.id);
            modelQuery.bindValue(1, model.providerId);
            modelQuery.bindValue(2, model.displayName);
            modelQuery.bindValue(3, model.description);
            modelQuery.bindValue(4, model.family);
            modelQuery.bindValue(5, model.group);
            modelQuery.bindValue(6, model.limits.context);
            modelQuery.bindValue(7, model.limits.maxInput);
            modelQuery.bindValue(8, model.limits.maxOutput);
            modelQuery.bindValue(9, static_cast<int>(model.capabilities.toInt()));
            modelQuery.bindValue(10, model.defaultParams.temperature);
            modelQuery.bindValue(11, model.defaultParams.topP);
            modelQuery.bindValue(12, model.defaultParams.maxOutputTokens.has_value() ? QVariant(model.defaultParams.maxOutputTokens.value()) : QVariant());
            modelQuery.bindValue(13, model.defaultParams.enableThinking ? 1 : 0);
            modelQuery.bindValue(14, model.defaultParams.reasoningEffort);
            modelQuery.bindValue(15, model.defaultParams.thinkingBudgetTokens);
            modelQuery.bindValue(16, model.pricing.inputPrice);
            modelQuery.bindValue(17, model.pricing.outputPrice);
            modelQuery.bindValue(18, model.pricing.cacheReadPrice);
            modelQuery.bindValue(19, model.pricing.cacheWritePrice);
            modelQuery.bindValue(20, model.pricing.currency);
            modelQuery.bindValue(21, model.reasoningField);
            modelQuery.bindValue(22, model.openWeights ? 1 : 0);
            modelQuery.bindValue(23, model.knowledgeCutoff);
            modelQuery.bindValue(24, model.isEnabled ? 1 : 0);
            modelQuery.bindValue(25, model.isCustom ? 1 : 0);
            modelQuery.exec();
        }

        return tx.commit();
    }

    QList<domain::model::Model> SqliteModelRepository::getModelsByProviderId(const QString &providerId) {
        QList<domain::model::Model> result;
        auto db = getDatabase();
        QSqlQuery query(db);

        query.prepare(QStringLiteral("SELECT * FROM models WHERE provider_id = ? ORDER BY display_name ASC;"));
        query.bindValue(0, providerId);

        if (!query.exec()) {
            return result;
        }

        while (query.next()) {
            domain::model::Model model;
            model.id = query.value(QStringLiteral("id")).toString();
            model.providerId = query.value(QStringLiteral("provider_id")).toString();
            model.displayName = query.value(QStringLiteral("display_name")).toString();
            model.description = query.value(QStringLiteral("description")).toString();
            model.family = query.value(QStringLiteral("family")).toString();
            model.group = query.value(QStringLiteral("group_name")).toString();

            model.limits.context = query.value(QStringLiteral("context_limit")).toInt();
            model.limits.maxInput = query.value(QStringLiteral("max_input_limit")).toInt();
            model.limits.maxOutput = query.value(QStringLiteral("max_output_limit")).toInt();

            model.capabilities = domain::model::ModelCapabilities(query.value(QStringLiteral("capabilities")).toInt());

            model.defaultParams.temperature = query.value(QStringLiteral("default_temperature")).toDouble();
            model.defaultParams.topP = query.value(QStringLiteral("default_top_p")).toDouble();
            if (!query.value(QStringLiteral("default_max_output_tokens")).isNull()) {
                model.defaultParams.maxOutputTokens = query.value(QStringLiteral("default_max_output_tokens")).toInt();
            }
            model.defaultParams.enableThinking = query.value(QStringLiteral("default_enable_thinking")).toBool();
            model.defaultParams.reasoningEffort = query.value(QStringLiteral("default_reasoning_effort")).toString();
            model.defaultParams.thinkingBudgetTokens = query.value(QStringLiteral("default_thinking_budget_tokens")).toInt();

            model.pricing.inputPrice = query.value(QStringLiteral("input_price")).toDouble();
            model.pricing.outputPrice = query.value(QStringLiteral("output_price")).toDouble();
            model.pricing.cacheReadPrice = query.value(QStringLiteral("cache_read_price")).toDouble();
            model.pricing.cacheWritePrice = query.value(QStringLiteral("cache_write_price")).toDouble();
            model.pricing.currency = query.value(QStringLiteral("currency")).toString();

            model.reasoningField = query.value(QStringLiteral("reasoning_field")).toString();
            model.openWeights = query.value(QStringLiteral("open_weights")).toBool();
            model.knowledgeCutoff = query.value(QStringLiteral("knowledge_cutoff")).toString();
            model.isEnabled = query.value(QStringLiteral("is_enabled")).toBool();
            model.isCustom = query.value(QStringLiteral("is_custom")).toBool();

            result.append(model);
        }

        return result;
    }

    QList<domain::model::ModelProvider> SqliteModelRepository::getAllProviders() {
        QList<domain::model::ModelProvider> result;
        auto db = getDatabase();
        QSqlQuery query(db);

        if (!query.exec(QStringLiteral("SELECT * FROM model_providers ORDER BY name ASC;"))) {
            return result;
        }

        while (query.next()) {
            domain::model::ModelProvider provider;
            provider.id = query.value(QStringLiteral("id")).toString();
            provider.name = query.value(QStringLiteral("name")).toString();
            provider.icon = query.value(QStringLiteral("icon")).toString();
            provider.docUrl = query.value(QStringLiteral("doc_url")).toString();
            provider.envVarName = query.value(QStringLiteral("env_var_name")).toString();
            provider.type = static_cast<domain::model::ProviderType>(query.value(QStringLiteral("type")).toInt());
            provider.baseUrl = query.value(QStringLiteral("base_url")).toString();
            provider.apiKey = query.value(QStringLiteral("api_key")).toString();

            QString headersJson = query.value(QStringLiteral("custom_headers")).toString();
            if (!headersJson.isEmpty()) {
                QJsonObject hObj = QJsonDocument::fromJson(headersJson.toUtf8()).object();
                for (auto it = hObj.begin(); it != hObj.end(); ++it) {
                    provider.customHeaders.insert(it.key(), it.value().toString());
                }
            }

            if (!query.value(QStringLiteral("proxy_url")).isNull()) {
                provider.proxyUrl = query.value(QStringLiteral("proxy_url")).toString();
            }
            provider.timeoutMs = query.value(QStringLiteral("timeout_ms")).toInt();
            provider.isEnabled = query.value(QStringLiteral("is_enabled")).toBool();

            provider.models = getModelsByProviderId(provider.id);
            result.append(provider);
        }

        return result;
    }

    std::optional<domain::model::ModelProvider> SqliteModelRepository::getProvider(const QString &providerId) {
        auto db = getDatabase();
        QSqlQuery query(db);
        query.prepare(QStringLiteral("SELECT * FROM model_providers WHERE id = ?;"));
        query.bindValue(0, providerId);

        if (!query.exec() || !query.next()) {
            return std::nullopt;
        }

        domain::model::ModelProvider provider;
        provider.id = query.value(QStringLiteral("id")).toString();
        provider.name = query.value(QStringLiteral("name")).toString();
        provider.icon = query.value(QStringLiteral("icon")).toString();
        provider.docUrl = query.value(QStringLiteral("doc_url")).toString();
        provider.envVarName = query.value(QStringLiteral("env_var_name")).toString();
        provider.type = static_cast<domain::model::ProviderType>(query.value(QStringLiteral("type")).toInt());
        provider.baseUrl = query.value(QStringLiteral("base_url")).toString();
        provider.apiKey = query.value(QStringLiteral("api_key")).toString();

        QString headersJson = query.value(QStringLiteral("custom_headers")).toString();
        if (!headersJson.isEmpty()) {
            QJsonObject hObj = QJsonDocument::fromJson(headersJson.toUtf8()).object();
            for (auto it = hObj.begin(); it != hObj.end(); ++it) {
                provider.customHeaders.insert(it.key(), it.value().toString());
            }
        }

        if (!query.value(QStringLiteral("proxy_url")).isNull()) {
            provider.proxyUrl = query.value(QStringLiteral("proxy_url")).toString();
        }
        provider.timeoutMs = query.value(QStringLiteral("timeout_ms")).toInt();
        provider.isEnabled = query.value(QStringLiteral("is_enabled")).toBool();
        provider.models = getModelsByProviderId(provider.id);

        return provider;
    }

    void SqliteModelRepository::saveProvider(const domain::model::ModelProvider &provider) {
        auto db = getDatabase();
        data::sqlite::SqlTransaction tx(db);
        if (!tx.isStarted()) {
            return;
        }

        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO model_providers ("
            "    id, name, icon, doc_url, env_var_name, type, base_url, api_key, custom_headers, proxy_url, timeout_ms, is_enabled"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
        ));

        QJsonObject headersObj;
        for (auto it = provider.customHeaders.begin(); it != provider.customHeaders.end(); ++it) {
            headersObj.insert(it.key(), it.value());
        }
        QString headersJson = QString::fromUtf8(QJsonDocument(headersObj).toJson(QJsonDocument::Compact));

        query.bindValue(0, provider.id);
        query.bindValue(1, provider.name);
        query.bindValue(2, provider.icon);
        query.bindValue(3, provider.docUrl);
        query.bindValue(4, provider.envVarName);
        query.bindValue(5, static_cast<int>(provider.type));
        query.bindValue(6, provider.baseUrl);
        query.bindValue(7, provider.apiKey);
        query.bindValue(8, headersJson);
        query.bindValue(9, provider.proxyUrl.has_value() ? QVariant(provider.proxyUrl.value()) : QVariant());
        query.bindValue(10, provider.timeoutMs);
        query.bindValue(11, provider.isEnabled ? 1 : 0);
        query.exec();

        // 保存其下所属的模型
        QSqlQuery modelQuery(db);
        modelQuery.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO models ("
            "    id, provider_id, display_name, description, family, group_name, context_limit, max_input_limit, max_output_limit, "
            "    capabilities, default_temperature, default_top_p, default_max_output_tokens, default_enable_thinking, "
            "    default_reasoning_effort, default_thinking_budget_tokens, input_price, output_price, cache_read_price, "
            "    cache_write_price, currency, reasoning_field, open_weights, knowledge_cutoff, is_enabled, is_custom"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
        ));

        for (const auto &model : provider.models) {
            modelQuery.bindValue(0, model.id);
            modelQuery.bindValue(1, provider.id);
            modelQuery.bindValue(2, model.displayName);
            modelQuery.bindValue(3, model.description);
            modelQuery.bindValue(4, model.family);
            modelQuery.bindValue(5, model.group);
            modelQuery.bindValue(6, model.limits.context);
            modelQuery.bindValue(7, model.limits.maxInput);
            modelQuery.bindValue(8, model.limits.maxOutput);
            modelQuery.bindValue(9, static_cast<int>(model.capabilities));
            modelQuery.bindValue(10, model.defaultParams.temperature);
            modelQuery.bindValue(11, model.defaultParams.topP);
            modelQuery.bindValue(12, model.defaultParams.maxOutputTokens.has_value() ? QVariant(model.defaultParams.maxOutputTokens.value()) : QVariant());
            modelQuery.bindValue(13, model.defaultParams.enableThinking ? 1 : 0);
            modelQuery.bindValue(14, model.defaultParams.reasoningEffort);
            modelQuery.bindValue(15, model.defaultParams.thinkingBudgetTokens);
            modelQuery.bindValue(16, model.pricing.inputPrice);
            modelQuery.bindValue(17, model.pricing.outputPrice);
            modelQuery.bindValue(18, model.pricing.cacheReadPrice);
            modelQuery.bindValue(19, model.pricing.cacheWritePrice);
            modelQuery.bindValue(20, model.pricing.currency);
            modelQuery.bindValue(21, model.reasoningField);
            modelQuery.bindValue(22, model.openWeights ? 1 : 0);
            modelQuery.bindValue(23, model.knowledgeCutoff);
            modelQuery.bindValue(24, model.isEnabled ? 1 : 0);
            modelQuery.bindValue(25, model.isCustom ? 1 : 0);
            modelQuery.exec();
        }

        tx.commit();
    }

    void SqliteModelRepository::deleteProvider(const QString &providerId) {
        auto db = getDatabase();
        data::sqlite::SqlTransaction tx(db);
        if (!tx.isStarted()) {
            return;
        }

        QSqlQuery deleteModels(db);
        deleteModels.prepare(QStringLiteral("DELETE FROM models WHERE provider_id = ?;"));
        deleteModels.bindValue(0, providerId);
        deleteModels.exec();

        QSqlQuery deleteProviderQuery(db);
        deleteProviderQuery.prepare(QStringLiteral("DELETE FROM model_providers WHERE id = ?;"));
        deleteProviderQuery.bindValue(0, providerId);
        deleteProviderQuery.exec();

        tx.commit();
    }

    QList<domain::model::Model> SqliteModelRepository::getEnabledModels() {
        QList<domain::model::Model> result;
        auto db = getDatabase();
        QSqlQuery query(db);

        const QString sql = QStringLiteral(
            "SELECT m.* FROM models m "
            "JOIN model_providers p ON m.provider_id = p.id "
            "WHERE m.is_enabled = 1 AND p.is_enabled = 1 "
            "ORDER BY m.group_name ASC, m.display_name ASC;"
        );

        if (!query.exec(sql)) {
            return result;
        }

        while (query.next()) {
            domain::model::Model model;
            model.id = query.value(QStringLiteral("id")).toString();
            model.providerId = query.value(QStringLiteral("provider_id")).toString();
            model.displayName = query.value(QStringLiteral("display_name")).toString();
            model.description = query.value(QStringLiteral("description")).toString();
            model.family = query.value(QStringLiteral("family")).toString();
            model.group = query.value(QStringLiteral("group_name")).toString();

            model.limits.context = query.value(QStringLiteral("context_limit")).toInt();
            model.limits.maxInput = query.value(QStringLiteral("max_input_limit")).toInt();
            model.limits.maxOutput = query.value(QStringLiteral("max_output_limit")).toInt();

            model.capabilities = domain::model::ModelCapabilities(query.value(QStringLiteral("capabilities")).toInt());

            model.defaultParams.temperature = query.value(QStringLiteral("default_temperature")).toDouble();
            model.defaultParams.topP = query.value(QStringLiteral("default_top_p")).toDouble();
            if (!query.value(QStringLiteral("default_max_output_tokens")).isNull()) {
                model.defaultParams.maxOutputTokens = query.value(QStringLiteral("default_max_output_tokens")).toInt();
            }
            model.defaultParams.enableThinking = query.value(QStringLiteral("default_enable_thinking")).toBool();
            model.defaultParams.reasoningEffort = query.value(QStringLiteral("default_reasoning_effort")).toString();
            model.defaultParams.thinkingBudgetTokens = query.value(QStringLiteral("default_thinking_budget_tokens")).toInt();

            model.pricing.inputPrice = query.value(QStringLiteral("input_price")).toDouble();
            model.pricing.outputPrice = query.value(QStringLiteral("output_price")).toDouble();
            model.pricing.cacheReadPrice = query.value(QStringLiteral("cache_read_price")).toDouble();
            model.pricing.cacheWritePrice = query.value(QStringLiteral("cache_write_price")).toDouble();
            model.pricing.currency = query.value(QStringLiteral("currency")).toString();

            model.reasoningField = query.value(QStringLiteral("reasoning_field")).toString();
            model.openWeights = query.value(QStringLiteral("open_weights")).toBool();
            model.knowledgeCutoff = query.value(QStringLiteral("knowledge_cutoff")).toString();
            model.isEnabled = query.value(QStringLiteral("is_enabled")).toBool();
            model.isCustom = query.value(QStringLiteral("is_custom")).toBool();

            result.append(model);
        }

        return result;
    }

    std::optional<domain::model::Model> SqliteModelRepository::getModel(const QString &modelId) {
        auto db = getDatabase();
        QSqlQuery query(db);
        query.prepare(QStringLiteral("SELECT * FROM models WHERE id = ?;"));
        query.bindValue(0, modelId);

        if (!query.exec() || !query.next()) {
            return std::nullopt;
        }

        domain::model::Model model;
        model.id = query.value(QStringLiteral("id")).toString();
        model.providerId = query.value(QStringLiteral("provider_id")).toString();
        model.displayName = query.value(QStringLiteral("display_name")).toString();
        model.description = query.value(QStringLiteral("description")).toString();
        model.family = query.value(QStringLiteral("family")).toString();
        model.group = query.value(QStringLiteral("group_name")).toString();

        model.limits.context = query.value(QStringLiteral("context_limit")).toInt();
        model.limits.maxInput = query.value(QStringLiteral("max_input_limit")).toInt();
        model.limits.maxOutput = query.value(QStringLiteral("max_output_limit")).toInt();

        model.capabilities = domain::model::ModelCapabilities(query.value(QStringLiteral("capabilities")).toInt());

        model.defaultParams.temperature = query.value(QStringLiteral("default_temperature")).toDouble();
        model.defaultParams.topP = query.value(QStringLiteral("default_top_p")).toDouble();
        if (!query.value(QStringLiteral("default_max_output_tokens")).isNull()) {
            model.defaultParams.maxOutputTokens = query.value(QStringLiteral("default_max_output_tokens")).toInt();
        }
        model.defaultParams.enableThinking = query.value(QStringLiteral("default_enable_thinking")).toBool();
        model.defaultParams.reasoningEffort = query.value(QStringLiteral("default_reasoning_effort")).toString();
        model.defaultParams.thinkingBudgetTokens = query.value(QStringLiteral("default_thinking_budget_tokens")).toInt();

        model.pricing.inputPrice = query.value(QStringLiteral("input_price")).toDouble();
        model.pricing.outputPrice = query.value(QStringLiteral("output_price")).toDouble();
        model.pricing.cacheReadPrice = query.value(QStringLiteral("cache_read_price")).toDouble();
        model.pricing.cacheWritePrice = query.value(QStringLiteral("cache_write_price")).toDouble();
        model.pricing.currency = query.value(QStringLiteral("currency")).toString();

        model.reasoningField = query.value(QStringLiteral("reasoning_field")).toString();
        model.openWeights = query.value(QStringLiteral("open_weights")).toBool();
        model.knowledgeCutoff = query.value(QStringLiteral("knowledge_cutoff")).toString();
        model.isEnabled = query.value(QStringLiteral("is_enabled")).toBool();
        model.isCustom = query.value(QStringLiteral("is_custom")).toBool();

        return model;
    }

} // namespace data::repository
