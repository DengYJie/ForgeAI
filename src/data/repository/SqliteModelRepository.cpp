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
#include <QCryptographicHash>
#include <QDebug>

namespace data::repository {

    namespace {
        domain::model::DataOrigin stringToOrigin(const QString &str) {
            if (str == QStringLiteral("User")) return domain::model::DataOrigin::User;
            return domain::model::DataOrigin::BuiltIn;
        }

        QString originToString(domain::model::DataOrigin origin) {
            if (origin == domain::model::DataOrigin::User) return QStringLiteral("User");
            return QStringLiteral("BuiltIn");
        }

        domain::model::ModelProvider readProviderRow(const QSqlQuery &query, int offset = 0) {
            domain::model::ModelProvider p;
            p.id = query.value(offset + 0).toString();
            p.name = query.value(offset + 1).toString();
            p.icon = query.value(offset + 2).toString();
            p.docUrl = query.value(offset + 3).toString();
            p.envVarName = query.value(offset + 4).toString();
            p.type = static_cast<domain::model::ProviderType>(query.value(offset + 5).toInt());
            p.baseUrl = query.value(offset + 6).toString();
            p.apiKey = query.value(offset + 7).toString();
            // custom_headers: 简易跳过或保留
            p.proxyUrl = query.value(offset + 9).isNull() ? std::nullopt : std::optional<QString>(query.value(offset + 9).toString());
            int timeout = query.value(offset + 10).toInt();
            p.timeoutMs = timeout > 0 ? timeout : 60000;
            p.isEnabled = query.value(offset + 11).toInt() != 0;
            p.isCustom = query.value(offset + 12).toInt() != 0;
            p.origin = stringToOrigin(query.value(offset + 13).toString());
            return p;
        }

        domain::model::ProviderModel readProviderModelRow(const QSqlQuery &query, int offset = 0) {
            domain::model::ProviderModel pm;
            pm.providerId = query.value(offset + 0).toString();
            pm.remoteModelId = query.value(offset + 1).toString();
            if (!query.value(offset + 2).isNull()) {
                pm.canonicalModelId = query.value(offset + 2).toString();
            }
            pm.pricing.inputPrice = query.value(offset + 3).toDouble();
            pm.pricing.outputPrice = query.value(offset + 4).toDouble();
            pm.pricing.cacheReadPrice = query.value(offset + 5).toDouble();
            pm.pricing.cacheWritePrice = query.value(offset + 6).toDouble();
            pm.pricing.currency = query.value(offset + 7).toString();

            if (!query.value(offset + 8).isNull()) {
                domain::model::ModelLimit lim;
                lim.context = query.value(offset + 8).toInt();
                lim.maxInput = query.value(offset + 9).toInt();
                lim.maxOutput = query.value(offset + 10).toInt();
                pm.limitsOverride = lim;
            }

            if (!query.value(offset + 11).isNull()) {
                pm.capabilitiesOverride = static_cast<domain::model::ModelCapabilities>(query.value(offset + 11).toLongLong());
            }

            pm.reasoningField = query.value(offset + 12).toString();
            pm.group = query.value(offset + 13).toString();
            pm.isEnabled = query.value(offset + 14).toInt() != 0;
            pm.isCustom = query.value(offset + 15).toInt() != 0;
            pm.origin = stringToOrigin(query.value(offset + 16).toString());
            return pm;
        }

        domain::model::CanonicalModel readCanonicalModelRow(const QSqlQuery &query, int offset = 0) {
            domain::model::CanonicalModel cm;
            cm.id = query.value(offset + 0).toString();
            cm.name = query.value(offset + 1).toString();
            cm.family = query.value(offset + 2).toString();
            cm.description = query.value(offset + 3).toString();
            cm.capabilities = static_cast<domain::model::ModelCapabilities>(query.value(offset + 4).toLongLong());
            cm.limits.context = query.value(offset + 5).toInt();
            cm.limits.maxInput = query.value(offset + 6).toInt();
            cm.limits.maxOutput = query.value(offset + 7).toInt();

            QString modIn = query.value(offset + 8).toString();
            if (!modIn.isEmpty()) cm.modalities.input = modIn.split(QLatin1Char(','));

            QString modOut = query.value(offset + 9).toString();
            if (!modOut.isEmpty()) cm.modalities.output = modOut.split(QLatin1Char(','));

            cm.defaultParams.temperature = query.value(offset + 10).toDouble();
            cm.defaultParams.topP = query.value(offset + 11).toDouble();
            cm.defaultParams.enableThinking = query.value(offset + 12).toInt() != 0;
            cm.defaultParams.thinkingBudgetTokens = query.value(offset + 13).toInt();
            cm.openWeights = query.value(offset + 14).toInt() != 0;
            cm.knowledgeCutoff = query.value(offset + 15).toString();
            cm.releaseDate = query.value(offset + 16).toString();
            return cm;
        }
    } // namespace

    SqliteModelRepository::SqliteModelRepository(const QString &connectionName)
        : m_connectionName(connectionName) {
    }

    QSqlDatabase SqliteModelRepository::getDatabase() const {
        return QSqlDatabase::database(m_connectionName);
    }

    QString SqliteModelRepository::getMetadata(const QString &key) const {
        auto db = getDatabase();
        QSqlQuery query(db);
        query.prepare(QStringLiteral("SELECT value FROM app_metadata WHERE key = ?;"));
        query.bindValue(0, key);
        if (query.exec() && query.next()) {
            return query.value(0).toString();
        }
        return QString();
    }

    void SqliteModelRepository::setMetadata(const QString &key, const QString &value) {
        auto db = getDatabase();
        QSqlQuery query(db);
        query.prepare(QStringLiteral("INSERT OR REPLACE INTO app_metadata (key, value) VALUES (?, ?);"));
        query.bindValue(0, key);
        query.bindValue(1, value);
        query.exec();
    }

    bool SqliteModelRepository::initializeDatabase(const QString &apiJsonPath, const QString &modelsJsonPath) {
        auto db = getDatabase();
        if (!db.isOpen()) {
            return false;
        }

        // 1. 元数据与版本表
        data::sqlite::SqlHelper::exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS app_metadata ("
            "    key TEXT PRIMARY KEY,"
            "    value TEXT"
            ");"
        ), db);

        // 2. Canonical Models 模型本体表
        data::sqlite::SqlHelper::exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS canonical_models ("
            "    id TEXT PRIMARY KEY,"
            "    name TEXT NOT NULL,"
            "    family TEXT,"
            "    description TEXT,"
            "    capabilities INTEGER DEFAULT 0,"
            "    context_limit INTEGER DEFAULT 128000,"
            "    max_input_limit INTEGER DEFAULT 128000,"
            "    max_output_limit INTEGER DEFAULT 8192,"
            "    modalities_input TEXT,"
            "    modalities_output TEXT,"
            "    default_temperature REAL DEFAULT 0.7,"
            "    default_top_p REAL DEFAULT 1.0,"
            "    default_enable_thinking INTEGER DEFAULT 1,"
            "    default_thinking_budget_tokens INTEGER DEFAULT 4096,"
            "    open_weights INTEGER DEFAULT 0,"
            "    knowledge_cutoff TEXT,"
            "    release_date TEXT"
            ");"
        ), db);

        // 3. Model Providers 服务商表
        data::sqlite::SqlHelper::exec(QStringLiteral(
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
            "    is_enabled INTEGER DEFAULT 0,"
            "    is_custom INTEGER DEFAULT 0,"
            "    origin TEXT DEFAULT 'BuiltIn'"
            ");"
        ), db);

        // 4. Provider Models 服务商-模型挂载表
        data::sqlite::SqlHelper::exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS provider_models ("
            "    provider_id TEXT NOT NULL,"
            "    remote_model_id TEXT NOT NULL,"
            "    canonical_model_id TEXT,"
            "    pricing_input REAL DEFAULT 0.0,"
            "    pricing_output REAL DEFAULT 0.0,"
            "    pricing_cache_read REAL DEFAULT 0.0,"
            "    pricing_cache_write REAL DEFAULT 0.0,"
            "    pricing_currency TEXT DEFAULT 'USD',"
            "    context_limit_override INTEGER,"
            "    max_input_override INTEGER,"
            "    max_output_override INTEGER,"
            "    capabilities_override INTEGER,"
            "    reasoning_field TEXT,"
            "    group_name TEXT,"
            "    is_enabled INTEGER DEFAULT 1,"
            "    is_custom INTEGER DEFAULT 0,"
            "    origin TEXT DEFAULT 'BuiltIn',"
            "    PRIMARY KEY (provider_id, remote_model_id),"
            "    FOREIGN KEY (provider_id) REFERENCES model_providers(id) ON DELETE CASCADE,"
            "    FOREIGN KEY (canonical_model_id) REFERENCES canonical_models(id) ON DELETE SET NULL"
            ");"
        ), db);

        // 计算资源文件的哈希校验
        QByteArray hashContent;
        {
            QFile apiF(apiJsonPath);
            if (apiF.open(QIODevice::ReadOnly)) hashContent.append(apiF.readAll());
            QFile modelsF(modelsJsonPath);
            if (modelsF.open(QIODevice::ReadOnly)) hashContent.append(modelsF.readAll());
        }
        QString currentHash = QString::fromLatin1(QCryptographicHash::hash(hashContent, QCryptographicHash::Md5).toHex());
        QString storedHash = getMetadata(QStringLiteral("models_dev_hash"));
        int canonicalCount = data::sqlite::SqlHelper::scalarInt(QStringLiteral("SELECT COUNT(*) FROM canonical_models;"), {}, db);

        if (currentHash != storedHash || canonicalCount == 0) {
            qDebug() << "[SqliteModelRepository] 检测到 models.dev 资源更新或数据库为空，开始全量原子同步...";
            if (seedFromPresetJson(apiJsonPath, modelsJsonPath, true)) {
                setMetadata(QStringLiteral("models_dev_hash"), currentHash);
                qDebug() << "[SqliteModelRepository] 同步完成并已更新哈希:" << currentHash;
            }
        } else {
            qDebug() << "[SqliteModelRepository] 模型库缓存有效，跳过全量写入 (Hash:" << storedHash << ")";
        }

        return true;
    }

    bool SqliteModelRepository::seedFromPresetJson(const QString &apiJsonPath, const QString &modelsJsonPath, bool force) {
        Q_UNUSED(force);

        QFile apiFile(apiJsonPath);
        if (!apiFile.open(QIODevice::ReadOnly)) {
            qWarning() << "[SqliteModelRepository] 无法打开 api.json:" << apiJsonPath;
            return false;
        }
        QJsonObject apiRoot = QJsonDocument::fromJson(apiFile.readAll()).object();
        apiFile.close();

        QFile modelsFile(modelsJsonPath);
        if (!modelsFile.open(QIODevice::ReadOnly)) {
            qWarning() << "[SqliteModelRepository] 无法打开 models.json:" << modelsJsonPath;
            return false;
        }
        QJsonObject modelsRoot = QJsonDocument::fromJson(modelsFile.readAll()).object();
        modelsFile.close();

        auto importResult = data::importer::ModelsDevImporter::parseAll(apiRoot, modelsRoot);

        auto db = getDatabase();
        data::sqlite::SqlTransaction tx(db);
        if (!tx.isStarted()) {
            return false;
        }

        // 1. 批量插入 Canonical Models
        QSqlQuery canonicalQuery(db);
        canonicalQuery.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO canonical_models ("
            "    id, name, family, description, capabilities, context_limit, max_input_limit, max_output_limit, "
            "    modalities_input, modalities_output, default_temperature, default_top_p, default_enable_thinking, "
            "    default_thinking_budget_tokens, open_weights, knowledge_cutoff, release_date"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
        ));

        for (const auto &cm : importResult.canonicalModels) {
            canonicalQuery.bindValue(0, cm.id);
            canonicalQuery.bindValue(1, cm.name);
            canonicalQuery.bindValue(2, cm.family);
            canonicalQuery.bindValue(3, cm.description);
            canonicalQuery.bindValue(4, static_cast<qint64>(cm.capabilities));
            canonicalQuery.bindValue(5, cm.limits.context);
            canonicalQuery.bindValue(6, cm.limits.maxInput);
            canonicalQuery.bindValue(7, cm.limits.maxOutput);
            canonicalQuery.bindValue(8, cm.modalities.input.join(QLatin1Char(',')));
            canonicalQuery.bindValue(9, cm.modalities.output.join(QLatin1Char(',')));
            canonicalQuery.bindValue(10, cm.defaultParams.temperature);
            canonicalQuery.bindValue(11, cm.defaultParams.topP);
            canonicalQuery.bindValue(12, cm.defaultParams.enableThinking ? 1 : 0);
            canonicalQuery.bindValue(13, cm.defaultParams.thinkingBudgetTokens);
            canonicalQuery.bindValue(14, cm.openWeights ? 1 : 0);
            canonicalQuery.bindValue(15, cm.knowledgeCutoff);
            canonicalQuery.bindValue(16, cm.releaseDate);
            canonicalQuery.exec();
        }

        // 2. 批量插入 Providers (若已存在则保留已配置的 apiKey 与 is_enabled)
        QSqlQuery providerQuery(db);
        providerQuery.prepare(QStringLiteral(
            "INSERT INTO model_providers ("
            "    id, name, icon, doc_url, env_var_name, type, base_url, api_key, custom_headers, proxy_url, timeout_ms, is_enabled, is_custom, origin"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(id) DO UPDATE SET "
            "    name = excluded.name, doc_url = excluded.doc_url, env_var_name = excluded.env_var_name, "
            "    type = excluded.type, base_url = excluded.base_url;"
        ));

        for (const auto &p : importResult.providers) {
            providerQuery.bindValue(0, p.id);
            providerQuery.bindValue(1, p.name);
            providerQuery.bindValue(2, p.icon);
            providerQuery.bindValue(3, p.docUrl);
            providerQuery.bindValue(4, p.envVarName);
            providerQuery.bindValue(5, static_cast<int>(p.type));
            providerQuery.bindValue(6, p.baseUrl);
            providerQuery.bindValue(7, p.apiKey);
            providerQuery.bindValue(8, QStringLiteral("{}"));
            providerQuery.bindValue(9, p.proxyUrl.has_value() ? QVariant(p.proxyUrl.value()) : QVariant());
            providerQuery.bindValue(10, p.timeoutMs);
            providerQuery.bindValue(11, p.isEnabled ? 1 : 0);
            providerQuery.bindValue(12, p.isCustom ? 1 : 0);
            providerQuery.bindValue(13, originToString(p.origin));
            providerQuery.exec();
        }

        // 3. 批量插入 Provider Models 挂载
        QSqlQuery bindingQuery(db);
        bindingQuery.prepare(QStringLiteral(
            "INSERT INTO provider_models ("
            "    provider_id, remote_model_id, canonical_model_id, pricing_input, pricing_output, "
            "    pricing_cache_read, pricing_cache_write, pricing_currency, context_limit_override, "
            "    max_input_override, max_output_override, capabilities_override, reasoning_field, "
            "    group_name, is_enabled, is_custom, origin"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(provider_id, remote_model_id) DO UPDATE SET "
            "    canonical_model_id = excluded.canonical_model_id, "
            "    pricing_input = excluded.pricing_input, "
            "    pricing_output = excluded.pricing_output, "
            "    pricing_cache_read = excluded.pricing_cache_read, "
            "    pricing_cache_write = excluded.pricing_cache_write, "
            "    pricing_currency = excluded.pricing_currency, "
            "    context_limit_override = excluded.context_limit_override, "
            "    max_input_override = excluded.max_input_override, "
            "    max_output_override = excluded.max_output_override, "
            "    reasoning_field = excluded.reasoning_field;"
        ));

        for (const auto &binding : importResult.providerModels) {
            bindingQuery.bindValue(0, binding.providerId);
            bindingQuery.bindValue(1, binding.remoteModelId);
            bindingQuery.bindValue(2, binding.canonicalModelId.has_value() ? QVariant(binding.canonicalModelId.value()) : QVariant());
            bindingQuery.bindValue(3, binding.pricing.inputPrice);
            bindingQuery.bindValue(4, binding.pricing.outputPrice);
            bindingQuery.bindValue(5, binding.pricing.cacheReadPrice);
            bindingQuery.bindValue(6, binding.pricing.cacheWritePrice);
            bindingQuery.bindValue(7, binding.pricing.currency);

            if (binding.limitsOverride.has_value()) {
                bindingQuery.bindValue(8, binding.limitsOverride->context);
                bindingQuery.bindValue(9, binding.limitsOverride->maxInput);
                bindingQuery.bindValue(10, binding.limitsOverride->maxOutput);
            } else {
                bindingQuery.bindValue(8, QVariant());
                bindingQuery.bindValue(9, QVariant());
                bindingQuery.bindValue(10, QVariant());
            }

            if (binding.capabilitiesOverride.has_value()) {
                bindingQuery.bindValue(11, static_cast<qint64>(*binding.capabilitiesOverride));
            } else {
                bindingQuery.bindValue(11, QVariant());
            }

            bindingQuery.bindValue(12, binding.reasoningField);
            bindingQuery.bindValue(13, binding.group);
            bindingQuery.bindValue(14, binding.isEnabled ? 1 : 0);
            bindingQuery.bindValue(15, binding.isCustom ? 1 : 0);
            bindingQuery.bindValue(16, originToString(binding.origin));
            bindingQuery.exec();
        }

        qInfo().noquote() << QStringLiteral("[ModelsDevImporter] 导入完成: Canonical=%1, Providers=%2, Bindings=%3, Unresolved=%4")
            .arg(importResult.canonicalModels.size())
            .arg(importResult.providers.size())
            .arg(importResult.providerModels.size())
            .arg(importResult.unresolvedBindingsCount);

        tx.commit();
        return true;
    }

    QList<domain::model::ModelProvider> SqliteModelRepository::getAllProviders() {
        QList<domain::model::ModelProvider> list;
        auto db = getDatabase();
        QSqlQuery query(QStringLiteral(
            "SELECT id, name, icon, doc_url, env_var_name, type, base_url, api_key, custom_headers, proxy_url, timeout_ms, is_enabled, is_custom, origin "
            "FROM model_providers ORDER BY origin DESC, name ASC;"
        ), db);

        while (query.next()) {
            list.append(readProviderRow(query));
        }
        return list;
    }

    std::optional<domain::model::ModelProvider> SqliteModelRepository::getProvider(const QString &providerId) {
        auto db = getDatabase();
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT id, name, icon, doc_url, env_var_name, type, base_url, api_key, custom_headers, proxy_url, timeout_ms, is_enabled, is_custom, origin "
            "FROM model_providers WHERE id = ?;"
        ));
        query.bindValue(0, providerId);
        if (query.exec() && query.next()) {
            return readProviderRow(query);
        }
        return std::nullopt;
    }

    void SqliteModelRepository::saveProvider(const domain::model::ModelProvider &provider) {
        auto db = getDatabase();
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO model_providers ("
            "    id, name, icon, doc_url, env_var_name, type, base_url, api_key, custom_headers, proxy_url, timeout_ms, is_enabled, is_custom, origin"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
        ));
        query.bindValue(0, provider.id);
        query.bindValue(1, provider.name);
        query.bindValue(2, provider.icon);
        query.bindValue(3, provider.docUrl);
        query.bindValue(4, provider.envVarName);
        query.bindValue(5, static_cast<int>(provider.type));
        query.bindValue(6, provider.baseUrl);
        query.bindValue(7, provider.apiKey);
        query.bindValue(8, QStringLiteral("{}"));
        query.bindValue(9, provider.proxyUrl.has_value() ? QVariant(provider.proxyUrl.value()) : QVariant());
        query.bindValue(10, provider.timeoutMs);
        query.bindValue(11, provider.isEnabled ? 1 : 0);
        query.bindValue(12, provider.isCustom ? 1 : 0);
        query.bindValue(13, originToString(provider.origin));
        query.exec();
    }

    void SqliteModelRepository::deleteProvider(const QString &providerId) {
        auto db = getDatabase();
        auto provider = getProvider(providerId);
        if (!provider.has_value() || provider->origin == domain::model::DataOrigin::BuiltIn) {
            return;
        }

        data::sqlite::SqlTransaction tx(db);
        QSqlQuery delBindings(db);
        delBindings.prepare(QStringLiteral("DELETE FROM provider_models WHERE provider_id = ? AND origin = 'User';"));
        delBindings.bindValue(0, providerId);
        delBindings.exec();

        QSqlQuery delProvider(db);
        delProvider.prepare(QStringLiteral("DELETE FROM model_providers WHERE id = ? AND origin = 'User';"));
        delProvider.bindValue(0, providerId);
        delProvider.exec();
        tx.commit();
    }

    std::optional<domain::model::CanonicalModel> SqliteModelRepository::getCanonicalModel(const QString &modelId) {
        auto db = getDatabase();
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT id, name, family, description, capabilities, context_limit, max_input_limit, max_output_limit, "
            "       modalities_input, modalities_output, default_temperature, default_top_p, default_enable_thinking, "
            "       default_thinking_budget_tokens, open_weights, knowledge_cutoff, release_date "
            "FROM canonical_models WHERE id = ?;"
        ));
        query.bindValue(0, modelId);
        if (query.exec() && query.next()) {
            return readCanonicalModelRow(query);
        }
        return std::nullopt;
    }

    QList<domain::model::CanonicalModel> SqliteModelRepository::getAllCanonicalModels() {
        QList<domain::model::CanonicalModel> list;
        auto db = getDatabase();
        QSqlQuery query(QStringLiteral(
            "SELECT id, name, family, description, capabilities, context_limit, max_input_limit, max_output_limit, "
            "       modalities_input, modalities_output, default_temperature, default_top_p, default_enable_thinking, "
            "       default_thinking_budget_tokens, open_weights, knowledge_cutoff, release_date "
            "FROM canonical_models ORDER BY family ASC, name ASC;"
        ), db);
        while (query.next()) {
            list.append(readCanonicalModelRow(query));
        }
        return list;
    }

    QList<domain::model::ProviderModel> SqliteModelRepository::getProviderModels(const QString &providerId) {
        QList<domain::model::ProviderModel> list;
        auto db = getDatabase();
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT provider_id, remote_model_id, canonical_model_id, pricing_input, pricing_output, "
            "       pricing_cache_read, pricing_cache_write, pricing_currency, context_limit_override, "
            "       max_input_override, max_output_override, capabilities_override, reasoning_field, "
            "       group_name, is_enabled, is_custom, origin "
            "FROM provider_models WHERE provider_id = ? ORDER BY group_name ASC, remote_model_id ASC;"
        ));
        query.bindValue(0, providerId);
        if (query.exec()) {
            while (query.next()) {
                list.append(readProviderModelRow(query));
            }
        }
        return list;
    }

    void SqliteModelRepository::saveProviderModel(const domain::model::ProviderModel &binding) {
        auto db = getDatabase();
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO provider_models ("
            "    provider_id, remote_model_id, canonical_model_id, pricing_input, pricing_output, "
            "    pricing_cache_read, pricing_cache_write, pricing_currency, context_limit_override, "
            "    max_input_override, max_output_override, capabilities_override, reasoning_field, "
            "    group_name, is_enabled, is_custom, origin"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
        ));
        query.bindValue(0, binding.providerId);
        query.bindValue(1, binding.remoteModelId);
        query.bindValue(2, binding.canonicalModelId.has_value() ? QVariant(binding.canonicalModelId.value()) : QVariant());
        query.bindValue(3, binding.pricing.inputPrice);
        query.bindValue(4, binding.pricing.outputPrice);
        query.bindValue(5, binding.pricing.cacheReadPrice);
        query.bindValue(6, binding.pricing.cacheWritePrice);
        query.bindValue(7, binding.pricing.currency);

        if (binding.limitsOverride.has_value()) {
            query.bindValue(8, binding.limitsOverride->context);
            query.bindValue(9, binding.limitsOverride->maxInput);
            query.bindValue(10, binding.limitsOverride->maxOutput);
        } else {
            query.bindValue(8, QVariant());
            query.bindValue(9, QVariant());
            query.bindValue(10, QVariant());
        }

        if (binding.capabilitiesOverride.has_value()) {
            query.bindValue(11, static_cast<qint64>(*binding.capabilitiesOverride));
        } else {
            query.bindValue(11, QVariant());
        }

        query.bindValue(12, binding.reasoningField);
        query.bindValue(13, binding.group);
        query.bindValue(14, binding.isEnabled ? 1 : 0);
        query.bindValue(15, binding.isCustom ? 1 : 0);
        query.bindValue(16, originToString(binding.origin));
        query.exec();
    }

    void SqliteModelRepository::deleteProviderModel(const QString &providerId, const QString &remoteModelId) {
        auto db = getDatabase();
        QSqlQuery query(db);
        query.prepare(QStringLiteral("DELETE FROM provider_models WHERE provider_id = ? AND remote_model_id = ?;"));
        query.bindValue(0, providerId);
        query.bindValue(1, remoteModelId);
        query.exec();
    }

    QList<domain::model::ResolvedModel> SqliteModelRepository::getResolvedModelsForProvider(const QString &providerId) {
        QList<domain::model::ResolvedModel> list;
        auto db = getDatabase();
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT "
            "    p.id, p.name, p.icon, p.doc_url, p.env_var_name, p.type, p.base_url, p.api_key, p.custom_headers, p.proxy_url, p.timeout_ms, p.is_enabled, p.is_custom, p.origin, "
            "    pm.provider_id, pm.remote_model_id, pm.canonical_model_id, pm.pricing_input, pm.pricing_output, pm.pricing_cache_read, pm.pricing_cache_write, pm.pricing_currency, pm.context_limit_override, pm.max_input_override, pm.max_output_override, pm.capabilities_override, pm.reasoning_field, pm.group_name, pm.is_enabled, pm.is_custom, pm.origin, "
            "    cm.id, cm.name, cm.family, cm.description, cm.capabilities, cm.context_limit, cm.max_input_limit, cm.max_output_limit, cm.modalities_input, cm.modalities_output, cm.default_temperature, cm.default_top_p, cm.default_enable_thinking, cm.default_thinking_budget_tokens, cm.open_weights, cm.knowledge_cutoff, cm.release_date "
            "FROM provider_models pm "
            "JOIN model_providers p ON pm.provider_id = p.id "
            "LEFT JOIN canonical_models cm ON pm.canonical_model_id = cm.id "
            "WHERE pm.provider_id = ? "
            "ORDER BY pm.group_name ASC, pm.remote_model_id ASC;"
        ));
        query.bindValue(0, providerId);
        if (query.exec()) {
            while (query.next()) {
                domain::model::ResolvedModel rm;
                rm.provider = readProviderRow(query, 0);
                rm.binding = readProviderModelRow(query, 14);
                if (!query.value(31).isNull()) {
                    rm.canonical = readCanonicalModelRow(query, 31);
                }
                list.append(rm);
            }
        }
        return list;
    }

    QList<domain::model::ResolvedModel> SqliteModelRepository::getAllResolvedModels() {
        QList<domain::model::ResolvedModel> list;
        auto db = getDatabase();
        QSqlQuery query(QStringLiteral(
            "SELECT "
            "    p.id, p.name, p.icon, p.doc_url, p.env_var_name, p.type, p.base_url, p.api_key, p.custom_headers, p.proxy_url, p.timeout_ms, p.is_enabled, p.is_custom, p.origin, "
            "    pm.provider_id, pm.remote_model_id, pm.canonical_model_id, pm.pricing_input, pm.pricing_output, pm.pricing_cache_read, pm.pricing_cache_write, pm.pricing_currency, pm.context_limit_override, pm.max_input_override, pm.max_output_override, pm.capabilities_override, pm.reasoning_field, pm.group_name, pm.is_enabled, pm.is_custom, pm.origin, "
            "    cm.id, cm.name, cm.family, cm.description, cm.capabilities, cm.context_limit, cm.max_input_limit, cm.max_output_limit, cm.modalities_input, cm.modalities_output, cm.default_temperature, cm.default_top_p, cm.default_enable_thinking, cm.default_thinking_budget_tokens, cm.open_weights, cm.knowledge_cutoff, cm.release_date "
            "FROM provider_models pm "
            "JOIN model_providers p ON pm.provider_id = p.id "
            "LEFT JOIN canonical_models cm ON pm.canonical_model_id = cm.id "
            "ORDER BY p.name ASC, pm.group_name ASC, pm.remote_model_id ASC;"
        ), db);

        while (query.next()) {
            domain::model::ResolvedModel rm;
            rm.provider = readProviderRow(query, 0);
            rm.binding = readProviderModelRow(query, 14);
            if (!query.value(31).isNull()) {
                rm.canonical = readCanonicalModelRow(query, 31);
            }
            list.append(rm);
        }
        return list;
    }

    QList<domain::model::ResolvedModel> SqliteModelRepository::getEnabledResolvedModels() {
        QList<domain::model::ResolvedModel> list;
        auto db = getDatabase();
        QSqlQuery query(QStringLiteral(
            "SELECT "
            "    p.id, p.name, p.icon, p.doc_url, p.env_var_name, p.type, p.base_url, p.api_key, p.custom_headers, p.proxy_url, p.timeout_ms, p.is_enabled, p.is_custom, p.origin, "
            "    pm.provider_id, pm.remote_model_id, pm.canonical_model_id, pm.pricing_input, pm.pricing_output, pm.pricing_cache_read, pm.pricing_cache_write, pm.pricing_currency, pm.context_limit_override, pm.max_input_override, pm.max_output_override, pm.capabilities_override, pm.reasoning_field, pm.group_name, pm.is_enabled, pm.is_custom, pm.origin, "
            "    cm.id, cm.name, cm.family, cm.description, cm.capabilities, cm.context_limit, cm.max_input_limit, cm.max_output_limit, cm.modalities_input, cm.modalities_output, cm.default_temperature, cm.default_top_p, cm.default_enable_thinking, cm.default_thinking_budget_tokens, cm.open_weights, cm.knowledge_cutoff, cm.release_date "
            "FROM provider_models pm "
            "JOIN model_providers p ON pm.provider_id = p.id "
            "LEFT JOIN canonical_models cm ON pm.canonical_model_id = cm.id "
            "WHERE p.is_enabled = 1 AND pm.is_enabled = 1 "
            "ORDER BY p.name ASC, pm.group_name ASC, pm.remote_model_id ASC;"
        ), db);

        while (query.next()) {
            domain::model::ResolvedModel rm;
            rm.provider = readProviderRow(query, 0);
            rm.binding = readProviderModelRow(query, 14);
            if (!query.value(31).isNull()) {
                rm.canonical = readCanonicalModelRow(query, 31);
            }
            list.append(rm);
        }
        return list;
    }

    std::optional<domain::model::ResolvedModel> SqliteModelRepository::resolveModel(const QString &providerId, const QString &remoteModelId) {
        auto db = getDatabase();
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT "
            "    p.id, p.name, p.icon, p.doc_url, p.env_var_name, p.type, p.base_url, p.api_key, p.custom_headers, p.proxy_url, p.timeout_ms, p.is_enabled, p.is_custom, p.origin, "
            "    pm.provider_id, pm.remote_model_id, pm.canonical_model_id, pm.pricing_input, pm.pricing_output, pm.pricing_cache_read, pm.pricing_cache_write, pm.pricing_currency, pm.context_limit_override, pm.max_input_override, pm.max_output_override, pm.capabilities_override, pm.reasoning_field, pm.group_name, pm.is_enabled, pm.is_custom, pm.origin, "
            "    cm.id, cm.name, cm.family, cm.description, cm.capabilities, cm.context_limit, cm.max_input_limit, cm.max_output_limit, cm.modalities_input, cm.modalities_output, cm.default_temperature, cm.default_top_p, cm.default_enable_thinking, cm.default_thinking_budget_tokens, cm.open_weights, cm.knowledge_cutoff, cm.release_date "
            "FROM provider_models pm "
            "JOIN model_providers p ON pm.provider_id = p.id "
            "LEFT JOIN canonical_models cm ON pm.canonical_model_id = cm.id "
            "WHERE pm.provider_id = ? AND pm.remote_model_id = ?;"
        ));
        query.bindValue(0, providerId);
        query.bindValue(1, remoteModelId);
        if (query.exec() && query.next()) {
            domain::model::ResolvedModel rm;
            rm.provider = readProviderRow(query, 0);
            rm.binding = readProviderModelRow(query, 14);
            if (!query.value(31).isNull()) {
                rm.canonical = readCanonicalModelRow(query, 31);
            }
            return rm;
        }
        return std::nullopt;
    }

    QList<domain::model::Model> SqliteModelRepository::getEnabledModels() {
        QList<domain::model::Model> result;
        auto resolvedList = getEnabledResolvedModels();
        result.reserve(resolvedList.size());
        for (const auto &rm : resolvedList) {
            result.append(domain::model::Model::fromResolved(rm));
        }
        return result;
    }

    std::optional<domain::model::Model> SqliteModelRepository::getModel(const QString &modelId) {
        auto allResolved = getAllResolvedModels();
        for (const auto &rm : allResolved) {
            if (rm.requestModelId() == modelId) {
                return domain::model::Model::fromResolved(rm);
            }
        }
        return std::nullopt;
    }

} // namespace data::repository
