#include "SqliteModelRepository.h"
#include "data/sqlite/SqlTransaction.h"
#include "data/sqlite/SqlHelper.h"
#include "data/importer/ModelsDevImporter.h"

#include <QSqlQuery>
#include <QSqlRecord>
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
            if (str == QStringLiteral("Discovered")) return domain::model::DataOrigin::Discovered;
            if (str == QStringLiteral("User")) return domain::model::DataOrigin::User;
            return domain::model::DataOrigin::BuiltIn;
        }

        QString originToString(domain::model::DataOrigin origin) {
            if (origin == domain::model::DataOrigin::Discovered) return QStringLiteral("Discovered");
            if (origin == domain::model::DataOrigin::User) return QStringLiteral("User");
            return QStringLiteral("BuiltIn");
        }

        domain::model::CanonicalModel readCanonicalModelRow(const QSqlQuery &query, int offset = 0) {
            domain::model::CanonicalModel cm;
            cm.id          = query.value(offset + 0).toString();
            cm.name        = query.value(offset + 1).toString();
            cm.family      = query.value(offset + 2).toString();
            cm.description = query.value(offset + 3).toString();
            cm.capabilities = static_cast<domain::model::ModelCapabilities>(query.value(offset + 4).toLongLong());
            cm.limits.context   = query.value(offset + 5).toInt();
            cm.limits.maxInput  = query.value(offset + 6).toInt();
            cm.limits.maxOutput = query.value(offset + 7).toInt();
            const QString modIn  = query.value(offset + 8).toString();
            if (!modIn.isEmpty()) cm.modalities.input = modIn.split(QLatin1Char(','));
            const QString modOut = query.value(offset + 9).toString();
            if (!modOut.isEmpty()) cm.modalities.output = modOut.split(QLatin1Char(','));
            cm.defaultParams.temperature          = query.value(offset + 10).toDouble();
            cm.defaultParams.topP                 = query.value(offset + 11).toDouble();
            cm.defaultParams.enableThinking       = query.value(offset + 12).toInt() != 0;
            cm.defaultParams.thinkingBudgetTokens = query.value(offset + 13).toInt();
            cm.openWeights     = query.value(offset + 14).toInt() != 0;
            cm.knowledgeCutoff = query.value(offset + 15).toString();
            cm.releaseDate     = query.value(offset + 16).toString();
            return cm;
        }

        domain::model::ProviderModel readProviderModelRow(const QSqlQuery &query, int offset = 0) {
            domain::model::ProviderModel pm;
            pm.providerId    = query.value(offset + 0).toString();
            pm.remoteModelId = query.value(offset + 1).toString();
            if (!query.value(offset + 2).isNull()) pm.canonicalModelId = query.value(offset + 2).toString();
            pm.pricing.inputPrice      = query.value(offset + 3).toDouble();
            pm.pricing.outputPrice     = query.value(offset + 4).toDouble();
            pm.pricing.cacheReadPrice  = query.value(offset + 5).toDouble();
            pm.pricing.cacheWritePrice = query.value(offset + 6).toDouble();
            pm.pricing.currency        = query.value(offset + 7).toString();
            if (!query.value(offset + 8).isNull()) {
                domain::model::ModelLimit lim;
                lim.context   = query.value(offset + 8).toInt();
                lim.maxInput  = query.value(offset + 9).toInt();
                lim.maxOutput = query.value(offset + 10).toInt();
                pm.limitsOverride = lim;
            }
            if (!query.value(offset + 11).isNull())
                pm.capabilitiesOverride = static_cast<domain::model::ModelCapabilities>(query.value(offset + 11).toLongLong());
            pm.reasoningField = query.value(offset + 12).toString();
            pm.isEnabled      = query.value(offset + 13).toInt() != 0;
            pm.isCustom       = query.value(offset + 14).toInt() != 0;
            pm.origin         = stringToOrigin(query.value(offset + 15).toString());
            return pm;
        }

        // col layout for preset provider join:
        //  0=pp.id, 1=pp.name, 2=pp.icon, 3=pp.doc_url, 4=pp.env_var_name, 5=pp.type,
        //  6=pp.base_url, 7=pp.proxy_url, 8=pp.timeout_ms,
        //  9=uo.is_enabled, 10=uo.base_url_override, 11=uo.api_key
        domain::model::ModelProvider buildEffectiveProvider(const QSqlQuery &q, int off = 0) {
            domain::model::ModelProvider p;
            p.id         = q.value(off + 0).toString();
            p.name       = q.value(off + 1).toString();
            p.icon       = q.value(off + 2).toString();
            p.docUrl     = q.value(off + 3).toString();
            p.envVarName = q.value(off + 4).toString();
            p.protocol   = static_cast<domain::model::ProtocolType>(q.value(off + 5).toInt());
            // base_url: user override 优先
            p.baseUrl    = q.value(off + 10).isNull() || q.value(off + 10).toString().isEmpty()
                             ? q.value(off + 6).toString()
                             : q.value(off + 10).toString();
            p.apiKey     = q.value(off + 11).toString();
            p.proxyUrl   = q.value(off + 7).isNull()
                             ? std::nullopt
                             : std::optional<QString>(q.value(off + 7).toString());
            const int timeout = q.value(off + 8).toInt();
            p.timeoutMs  = timeout > 0 ? timeout : 60000;
            // is_enabled: 若有 user override 则使用 override，否则默认 false
            p.isEnabled  = q.value(off + 9).isNull() ? false : q.value(off + 9).toInt() != 0;
            p.isCustom   = false;
            p.origin     = domain::model::DataOrigin::BuiltIn;
            return p;
        }

        // col layout for custom provider:
        //  0=id, 1=name, 2=icon, 3=type, 4=base_url, 5=api_key, 6=timeout_ms, 7=is_enabled
        domain::model::ModelProvider buildEffectiveCustomProvider(const QSqlQuery &q, int off = 0) {
            domain::model::ModelProvider p;
            p.id        = q.value(off + 0).toString();
            p.name      = q.value(off + 1).toString();
            p.icon      = q.value(off + 2).toString();
            p.protocol  = static_cast<domain::model::ProtocolType>(q.value(off + 3).toInt());
            p.baseUrl   = q.value(off + 4).toString();
            p.apiKey    = q.value(off + 5).toString();
            const int timeout = q.value(off + 6).toInt();
            p.timeoutMs = timeout > 0 ? timeout : 60000;
            p.isEnabled = q.value(off + 7).toInt() != 0;
            p.isCustom  = true;
            p.origin    = domain::model::DataOrigin::User;
            return p;
        }

        domain::model::ResolvedModel buildResolvedFromRow(const QSqlQuery &q) {
            domain::model::ResolvedModel rm;

            // Provider (offset 0..11)
            rm.provider.id         = q.value(0).toString();
            rm.provider.name       = q.value(1).toString();
            rm.provider.icon       = q.value(2).toString();
            rm.provider.docUrl     = q.value(3).toString();
            rm.provider.envVarName = q.value(4).toString();
            rm.provider.protocol   = static_cast<domain::model::ProtocolType>(q.value(5).toInt());
            rm.provider.baseUrl    = q.value(10).isNull() || q.value(10).toString().isEmpty()
                                       ? q.value(6).toString()
                                       : q.value(10).toString();
            rm.provider.apiKey     = q.value(11).toString();
            rm.provider.proxyUrl   = q.value(7).isNull()
                                       ? std::nullopt
                                       : std::optional<QString>(q.value(7).toString());
            const int timeout = q.value(8).toInt();
            rm.provider.timeoutMs  = timeout > 0 ? timeout : 60000;
            rm.provider.isEnabled  = q.value(9).isNull() ? false : q.value(9).toInt() != 0;
            rm.provider.isCustom   = false;
            rm.provider.origin     = domain::model::DataOrigin::BuiltIn;

            // ProviderModel (offset 12..27)
            rm.binding.providerId    = q.value(12).toString();
            rm.binding.remoteModelId = q.value(13).toString();
            if (!q.value(14).isNull()) rm.binding.canonicalModelId = q.value(14).toString();
            rm.binding.pricing.inputPrice      = q.value(15).toDouble();
            rm.binding.pricing.outputPrice     = q.value(16).toDouble();
            rm.binding.pricing.cacheReadPrice  = q.value(17).toDouble();
            rm.binding.pricing.cacheWritePrice = q.value(18).toDouble();
            rm.binding.pricing.currency        = q.value(19).toString();
            if (!q.value(20).isNull()) {
                domain::model::ModelLimit lim;
                lim.context   = q.value(20).toInt();
                lim.maxInput  = q.value(21).toInt();
                lim.maxOutput = q.value(22).toInt();
                rm.binding.limitsOverride = lim;
            }
            if (!q.value(23).isNull())
                rm.binding.capabilitiesOverride = static_cast<domain::model::ModelCapabilities>(q.value(23).toLongLong());
            rm.binding.reasoningField = q.value(24).toString();
            rm.binding.isEnabled      = q.value(25).toInt() != 0;
            rm.binding.isCustom       = q.value(26).toInt() != 0;
            rm.binding.origin         = stringToOrigin(q.value(27).toString());
            const int reasoningOptionsColumn = q.record().indexOf(QStringLiteral("reasoning_options"));
            if (reasoningOptionsColumn >= 0) rm.binding.reasoningOptionsJson = q.value(reasoningOptionsColumn).toString();

            // CanonicalModel (offset 28..44)
            if (!q.value(28).isNull()) {
                rm.canonical = readCanonicalModelRow(q, 28);
            }

            return rm;
        }

        QList<domain::model::ResolvedModel> getCustomResolvedModels(
            const QSqlDatabase &db, const QString &whereClause, const QVariantList &arguments = {}) {
            QList<domain::model::ResolvedModel> list;
            QSqlQuery query(db);
            query.prepare(QStringLiteral(
                "SELECT "
                "  COALESCE(pp.id, cp.id), COALESCE(pp.name, cp.name), COALESCE(pp.icon, cp.icon), "
                "  COALESCE(pp.doc_url, ''), COALESCE(pp.env_var_name, ''), COALESCE(pp.type, cp.type), "
                "  COALESCE(pp.base_url, cp.base_url), pp.proxy_url, COALESCE(pp.timeout_ms, cp.timeout_ms), "
                "  COALESCE(uo.is_enabled, cp.is_enabled, 0), uo.base_url_override, COALESCE(uo.api_key, cp.api_key), "
                "  ucm.provider_id, ucm.remote_model_id, ucm.canonical_model_id, "
                "  0.0, 0.0, 0.0, 0.0, 'USD', "
                "  NULL, NULL, NULL, NULL, '', ucm.is_enabled, 0, ucm.origin, "
                "  cm.id, cm.name, cm.family, cm.description, cm.capabilities, "
                "  cm.context_limit, cm.max_input_limit, cm.max_output_limit, "
                "  cm.modalities_input, cm.modalities_output, "
                "  cm.default_temperature, cm.default_top_p, cm.default_enable_thinking, cm.default_thinking_budget_tokens, "
                "  cm.open_weights, cm.knowledge_cutoff, cm.release_date "
                "FROM user_custom_models ucm "
                "LEFT JOIN preset_providers pp ON pp.id = ucm.provider_id "
                "LEFT JOIN user_provider_overrides uo ON uo.provider_id = pp.id "
                "LEFT JOIN user_custom_providers cp ON cp.id = ucm.provider_id "
                "LEFT JOIN canonical_models cm ON cm.id = ucm.canonical_model_id "
                "WHERE %1 "
                "ORDER BY COALESCE(pp.name, cp.name) ASC, ucm.remote_model_id ASC;"
            ).arg(whereClause));
            for (qsizetype i = 0; i < arguments.size(); ++i) {
                query.bindValue(i, arguments.at(i));
            }
            if (!query.exec()) {
                qWarning() << "[getCustomResolvedModels] error:" << query.lastError().text();
                return list;
            }
            while (query.next()) list.append(buildResolvedFromRow(query));
            return list;
        }

    } // anonymous namespace

    SqliteModelRepository::SqliteModelRepository(const QString &connectionName)
        : m_connectionName(connectionName) {}

    QSqlDatabase SqliteModelRepository::getDatabase() const {
        return QSqlDatabase::database(m_connectionName);
    }

    QString SqliteModelRepository::getMetadata(const QString &key) const {
        auto db = getDatabase();
        QSqlQuery query(db);
        query.prepare(QStringLiteral("SELECT value FROM app_metadata WHERE key = ?;"));
        query.bindValue(0, key);
        if (query.exec() && query.next()) return query.value(0).toString();
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
        if (!db.isOpen()) return false;

        // 1. 元数据表
        data::sqlite::SqlHelper::exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS app_metadata ("
            "  key TEXT PRIMARY KEY,"
            "  value TEXT"
            ");"
        ), db);

        // 2. Canonical Models（规范模型本体，预置，全量可重建）
        data::sqlite::SqlHelper::exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS canonical_models ("
            "  id TEXT PRIMARY KEY,"
            "  name TEXT NOT NULL,"
            "  family TEXT,"
            "  description TEXT,"
            "  capabilities INTEGER DEFAULT 0,"
            "  context_limit INTEGER DEFAULT 128000,"
            "  max_input_limit INTEGER DEFAULT 128000,"
            "  max_output_limit INTEGER DEFAULT 8192,"
            "  modalities_input TEXT,"
            "  modalities_output TEXT,"
            "  default_temperature REAL DEFAULT 0.7,"
            "  default_top_p REAL DEFAULT 1.0,"
            "  default_enable_thinking INTEGER DEFAULT 1,"
            "  default_thinking_budget_tokens INTEGER DEFAULT 4096,"
            "  open_weights INTEGER DEFAULT 0,"
            "  knowledge_cutoff TEXT,"
            "  release_date TEXT"
            ");"
        ), db);

        // 3. Preset Providers（预置服务商基线，不含用户配置）
        data::sqlite::SqlHelper::exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS preset_providers ("
            "  id TEXT PRIMARY KEY,"
            "  name TEXT NOT NULL,"
            "  icon TEXT,"
            "  doc_url TEXT,"
            "  env_var_name TEXT,"
            "  type INTEGER NOT NULL DEFAULT 0,"
            "  base_url TEXT DEFAULT '',"
            "  proxy_url TEXT,"
            "  timeout_ms INTEGER DEFAULT 60000"
            ");"
        ), db);

        // 4. Preset Provider Models（预置服务商模型挂载关系）
        data::sqlite::SqlHelper::exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS preset_provider_models ("
            "  provider_id TEXT NOT NULL,"
            "  remote_model_id TEXT NOT NULL,"
            "  canonical_model_id TEXT,"
            "  pricing_input REAL DEFAULT 0.0,"
            "  pricing_output REAL DEFAULT 0.0,"
            "  pricing_cache_read REAL DEFAULT 0.0,"
            "  pricing_cache_write REAL DEFAULT 0.0,"
            "  pricing_currency TEXT DEFAULT 'USD',"
            "  context_limit_override INTEGER,"
            "  max_input_override INTEGER,"
            "  max_output_override INTEGER,"
            "  capabilities_override INTEGER,"
            "  reasoning_field TEXT,"
            "  reasoning_options TEXT,"
            "  is_enabled INTEGER DEFAULT 1,"
            "  is_custom INTEGER DEFAULT 0,"
            "  origin TEXT DEFAULT 'BuiltIn',"
            "  PRIMARY KEY (provider_id, remote_model_id),"
            "  FOREIGN KEY (canonical_model_id) REFERENCES canonical_models(id) ON DELETE SET NULL"
            ");"
        ), db);

        // 5. User Provider Overrides（用户对官方服务商的局部覆盖，永不被 importer 修改）
        data::sqlite::SqlHelper::exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS user_provider_overrides ("
            "  provider_id TEXT PRIMARY KEY,"
            "  is_enabled INTEGER,"
            "  base_url_override TEXT,"
            "  api_key TEXT,"
            "  custom_headers TEXT"
            ");"
        ), db);

        // 6. User Model Overrides（用户对官方模型的局部覆盖）
        data::sqlite::SqlHelper::exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS user_model_overrides ("
            "  provider_id TEXT NOT NULL,"
            "  remote_model_id TEXT NOT NULL,"
            "  is_enabled INTEGER,"
            "  custom_alias TEXT,"
            "  PRIMARY KEY (provider_id, remote_model_id)"
            ");"
        ), db);

        // 7. User Custom Providers（用户完全自定义的服务商，永久保留）
        data::sqlite::SqlHelper::exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS user_custom_providers ("
            "  id TEXT PRIMARY KEY,"
            "  name TEXT NOT NULL,"
            "  icon TEXT,"
            "  type INTEGER NOT NULL DEFAULT 0,"
            "  base_url TEXT NOT NULL,"
            "  api_key TEXT,"
            "  custom_headers TEXT,"
            "  timeout_ms INTEGER DEFAULT 60000,"
            "  is_enabled INTEGER DEFAULT 0"
            ");"
        ), db);

        // 8. User Custom Models（用户完全自定义的模型，永久保留）
        data::sqlite::SqlHelper::exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS user_custom_models ("
            "  provider_id TEXT NOT NULL,"
            "  remote_model_id TEXT NOT NULL,"
            "  display_name TEXT NOT NULL,"
            "  canonical_model_id TEXT,"
            "  is_enabled INTEGER DEFAULT 1,"
            "  origin TEXT NOT NULL DEFAULT 'User',"
            "  PRIMARY KEY (provider_id, remote_model_id)"
            ");"
        ), db);

        // 校验 Hash，如有更新则原子重建 Preset 表
        QByteArray hashContent;
        {
            QFile apiF(apiJsonPath);
            if (apiF.open(QIODevice::ReadOnly)) hashContent.append(apiF.readAll());
            QFile modelsF(modelsJsonPath);
            if (modelsF.open(QIODevice::ReadOnly)) hashContent.append(modelsF.readAll());
        }
        const QString currentHash = QString::fromLatin1(
            QCryptographicHash::hash(hashContent, QCryptographicHash::Md5).toHex());
        const QString storedHash  = getMetadata(QStringLiteral("models_dev_hash"));
        const int canonicalCount  = data::sqlite::SqlHelper::scalarInt(
            QStringLiteral("SELECT COUNT(*) FROM canonical_models;"), {}, db);
        const int providerCount   = data::sqlite::SqlHelper::scalarInt(
            QStringLiteral("SELECT COUNT(*) FROM preset_providers;"), {}, db);
        const int bindingCount    = data::sqlite::SqlHelper::scalarInt(
            QStringLiteral("SELECT COUNT(*) FROM preset_provider_models;"), {}, db);

        qInfo().noquote() << QStringLiteral(
            "[SqliteModelRepository] Hash 检查: stored=%1 current=%2 canonical=%3 providers=%4 bindings=%5")
            .arg(storedHash, currentHash).arg(canonicalCount).arg(providerCount).arg(bindingCount);

        if (currentHash != storedHash || canonicalCount == 0 || providerCount == 0 || bindingCount == 0) {
            qInfo() << "[SqliteModelRepository] 检测到资源更新，开始原子重建 Preset 表...";
            if (seedFromPresetJson(apiJsonPath, modelsJsonPath)) {
                setMetadata(QStringLiteral("models_dev_hash"), currentHash);
                qInfo() << "[SqliteModelRepository] Preset 表重建完成，Hash:" << currentHash;
            } else {
                qWarning() << "[SqliteModelRepository] Preset 表重建失败！";
            }
        } else {
            qInfo() << "[SqliteModelRepository] Preset 表缓存有效，跳过重建 (Hash:" << storedHash << ")";
        }

        return true;
    }

    bool SqliteModelRepository::seedFromPresetJson(const QString &apiJsonPath,
                                                    const QString &modelsJsonPath,
                                                    bool /*force*/) {
        QFile apiFile(apiJsonPath);
        if (!apiFile.open(QIODevice::ReadOnly)) {
            qWarning() << "[SqliteModelRepository] 无法打开 api.json:" << apiJsonPath;
            return false;
        }
        const QJsonObject apiRoot = QJsonDocument::fromJson(apiFile.readAll()).object();
        apiFile.close();

        QFile modelsFile(modelsJsonPath);
        if (!modelsFile.open(QIODevice::ReadOnly)) {
            qWarning() << "[SqliteModelRepository] 无法打开 models.json:" << modelsJsonPath;
            return false;
        }
        const QJsonObject modelsRoot = QJsonDocument::fromJson(modelsFile.readAll()).object();
        modelsFile.close();

        const auto importResult = data::importer::ModelsDevImporter::parseAll(apiRoot, modelsRoot);

        auto db = getDatabase();
        data::sqlite::SqlTransaction tx(db);
        if (!tx.isStarted()) return false;

        // 1. 原子清空三张 Preset 表（用户数据独立在 user_ 系列表中，安全）
        data::sqlite::SqlHelper::exec(QStringLiteral("DELETE FROM preset_provider_models;"), db);
        data::sqlite::SqlHelper::exec(QStringLiteral("DELETE FROM preset_providers;"), db);
        data::sqlite::SqlHelper::exec(QStringLiteral("DELETE FROM canonical_models;"), db);

        // 2. 批量插入 CanonicalModels
        QSqlQuery canonicalQuery(db);
        canonicalQuery.prepare(QStringLiteral(
            "INSERT INTO canonical_models ("
            "  id, name, family, description, capabilities, context_limit, max_input_limit, max_output_limit, "
            "  modalities_input, modalities_output, default_temperature, default_top_p, default_enable_thinking, "
            "  default_thinking_budget_tokens, open_weights, knowledge_cutoff, release_date"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
        ));
        for (const auto &cm : importResult.canonicalModels) {
            canonicalQuery.bindValue(0,  cm.id);
            canonicalQuery.bindValue(1,  cm.name);
            canonicalQuery.bindValue(2,  cm.family);
            canonicalQuery.bindValue(3,  cm.description);
            canonicalQuery.bindValue(4,  static_cast<qint64>(cm.capabilities));
            canonicalQuery.bindValue(5,  cm.limits.context);
            canonicalQuery.bindValue(6,  cm.limits.maxInput);
            canonicalQuery.bindValue(7,  cm.limits.maxOutput);
            canonicalQuery.bindValue(8,  cm.modalities.input.join(QLatin1Char(',')));
            canonicalQuery.bindValue(9,  cm.modalities.output.join(QLatin1Char(',')));
            canonicalQuery.bindValue(10, cm.defaultParams.temperature);
            canonicalQuery.bindValue(11, cm.defaultParams.topP);
            canonicalQuery.bindValue(12, cm.defaultParams.enableThinking ? 1 : 0);
            canonicalQuery.bindValue(13, cm.defaultParams.thinkingBudgetTokens);
            canonicalQuery.bindValue(14, cm.openWeights ? 1 : 0);
            canonicalQuery.bindValue(15, cm.knowledgeCutoff);
            canonicalQuery.bindValue(16, cm.releaseDate);
            if (!canonicalQuery.exec())
                qWarning() << "[seed] canonical insert failed:" << canonicalQuery.lastError().text();
        }

        // 3. 批量插入 Preset Providers
        QSqlQuery providerQuery(db);
        providerQuery.prepare(QStringLiteral(
            "INSERT INTO preset_providers ("
            "  id, name, icon, doc_url, env_var_name, type, base_url, proxy_url, timeout_ms"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);"
        ));
        for (const auto &p : importResult.providers) {
            providerQuery.bindValue(0, p.id);
            providerQuery.bindValue(1, p.name);
            providerQuery.bindValue(2, p.icon);
            providerQuery.bindValue(3, p.docUrl);
            providerQuery.bindValue(4, p.envVarName);
            providerQuery.bindValue(5, static_cast<int>(p.protocol));
            providerQuery.bindValue(6, p.baseUrl.isEmpty() ? QStringLiteral("") : p.baseUrl);
            providerQuery.bindValue(7, p.proxyUrl.has_value() ? QVariant(p.proxyUrl.value()) : QVariant());
            providerQuery.bindValue(8, p.timeoutMs);
            if (!providerQuery.exec())
                qWarning() << "[seed] provider insert failed:" << p.id << providerQuery.lastError().text();
        }

        // 4. 批量插入 Preset Provider Models
        QSqlQuery bindingQuery(db);
        bindingQuery.prepare(QStringLiteral(
            "INSERT INTO preset_provider_models ("
            "  provider_id, remote_model_id, canonical_model_id, pricing_input, pricing_output, "
            "  pricing_cache_read, pricing_cache_write, pricing_currency, context_limit_override, "
            "  max_input_override, max_output_override, capabilities_override, reasoning_field, reasoning_options, "
            "  is_enabled, is_custom, origin"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
        ));
        for (const auto &binding : importResult.providerModels) {
            bindingQuery.bindValue(0,  binding.providerId);
            bindingQuery.bindValue(1,  binding.remoteModelId);
            bindingQuery.bindValue(2,  binding.canonicalModelId.has_value()
                                           ? QVariant(binding.canonicalModelId.value()) : QVariant());
            bindingQuery.bindValue(3,  binding.pricing.inputPrice);
            bindingQuery.bindValue(4,  binding.pricing.outputPrice);
            bindingQuery.bindValue(5,  binding.pricing.cacheReadPrice);
            bindingQuery.bindValue(6,  binding.pricing.cacheWritePrice);
            bindingQuery.bindValue(7,  binding.pricing.currency);
            if (binding.limitsOverride.has_value()) {
                bindingQuery.bindValue(8,  binding.limitsOverride->context);
                bindingQuery.bindValue(9,  binding.limitsOverride->maxInput);
                bindingQuery.bindValue(10, binding.limitsOverride->maxOutput);
            } else {
                bindingQuery.bindValue(8,  QVariant());
                bindingQuery.bindValue(9,  QVariant());
                bindingQuery.bindValue(10, QVariant());
            }
            bindingQuery.bindValue(11, binding.capabilitiesOverride.has_value()
                                           ? QVariant(static_cast<qint64>(*binding.capabilitiesOverride)) : QVariant());
            bindingQuery.bindValue(12, binding.reasoningField);
            bindingQuery.bindValue(13, binding.reasoningOptionsJson);
            bindingQuery.bindValue(14, binding.isEnabled ? 1 : 0);
            bindingQuery.bindValue(15, binding.isCustom  ? 1 : 0);
            bindingQuery.bindValue(16, originToString(binding.origin));
            if (!bindingQuery.exec())
                qWarning() << "[seed] binding insert failed:" << bindingQuery.lastError().text();
        }

        qInfo().noquote() << QStringLiteral(
            "[SqliteModelRepository] Preset 表重建完成: canonical=%1 providers=%2 bindings=%3 unresolved=%4")
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

        // Part A: 预置 Provider (LEFT JOIN user_provider_overrides)
        QSqlQuery queryA(QStringLiteral(
            "SELECT pp.id, pp.name, pp.icon, pp.doc_url, pp.env_var_name, pp.type, "
            "       pp.base_url, pp.proxy_url, pp.timeout_ms, "
            "       uo.is_enabled, uo.base_url_override, uo.api_key "
            "FROM preset_providers pp "
            "LEFT JOIN user_provider_overrides uo ON pp.id = uo.provider_id "
            "ORDER BY pp.name ASC;"
        ), db);
        while (queryA.next()) list.append(buildEffectiveProvider(queryA));

        // Part B: 自定义 Provider
        QSqlQuery queryB(QStringLiteral(
            "SELECT id, name, icon, type, base_url, api_key, timeout_ms, is_enabled "
            "FROM user_custom_providers ORDER BY name ASC;"
        ), db);
        while (queryB.next()) list.append(buildEffectiveCustomProvider(queryB));

        qInfo().noquote() << QStringLiteral("[SqliteModelRepository] getAllProviders: %1 个").arg(list.size());
        return list;
    }

    QList<domain::model::ModelProvider> SqliteModelRepository::getEnabledProviders() {
        QList<domain::model::ModelProvider> list;
        auto db = getDatabase();

        QSqlQuery queryA(QStringLiteral(
            "SELECT pp.id, pp.name, pp.icon, pp.doc_url, pp.env_var_name, pp.type, "
            "       pp.base_url, pp.proxy_url, pp.timeout_ms, "
            "       uo.is_enabled, uo.base_url_override, uo.api_key "
            "FROM preset_providers pp "
            "LEFT JOIN user_provider_overrides uo ON pp.id = uo.provider_id "
            "WHERE uo.is_enabled = 1 "
            "ORDER BY pp.name ASC;"
        ), db);
        while (queryA.next()) list.append(buildEffectiveProvider(queryA));

        QSqlQuery queryB(QStringLiteral(
            "SELECT id, name, icon, type, base_url, api_key, timeout_ms, is_enabled "
            "FROM user_custom_providers WHERE is_enabled = 1 ORDER BY name ASC;"
        ), db);
        while (queryB.next()) list.append(buildEffectiveCustomProvider(queryB));

        return list;
    }

    std::optional<domain::model::ModelProvider> SqliteModelRepository::getProvider(const QString &providerId) {
        auto db = getDatabase();

        QSqlQuery queryA(db);
        queryA.prepare(QStringLiteral(
            "SELECT pp.id, pp.name, pp.icon, pp.doc_url, pp.env_var_name, pp.type, "
            "       pp.base_url, pp.proxy_url, pp.timeout_ms, "
            "       uo.is_enabled, uo.base_url_override, uo.api_key "
            "FROM preset_providers pp "
            "LEFT JOIN user_provider_overrides uo ON pp.id = uo.provider_id "
            "WHERE pp.id = ?;"
        ));
        queryA.bindValue(0, providerId);
        if (queryA.exec() && queryA.next()) return buildEffectiveProvider(queryA);

        QSqlQuery queryB(db);
        queryB.prepare(QStringLiteral(
            "SELECT id, name, icon, type, base_url, api_key, timeout_ms, is_enabled "
            "FROM user_custom_providers WHERE id = ?;"
        ));
        queryB.bindValue(0, providerId);
        if (queryB.exec() && queryB.next()) return buildEffectiveCustomProvider(queryB);

        return std::nullopt;
    }

    void SqliteModelRepository::saveProvider(const domain::model::ModelProvider &provider) {
        auto db = getDatabase();

        if (provider.isCustom || provider.origin == domain::model::DataOrigin::User) {
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "INSERT INTO user_custom_providers (id, name, icon, type, base_url, api_key, timeout_ms, is_enabled) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
                "ON CONFLICT(id) DO UPDATE SET "
                "  name = excluded.name, icon = excluded.icon, type = excluded.type, "
                "  base_url = excluded.base_url, api_key = excluded.api_key, "
                "  timeout_ms = excluded.timeout_ms, is_enabled = excluded.is_enabled;"
            ));
            q.bindValue(0, provider.id);
            q.bindValue(1, provider.name);
            q.bindValue(2, provider.icon);
            q.bindValue(3, static_cast<int>(provider.protocol));
            q.bindValue(4, provider.baseUrl);
            q.bindValue(5, provider.apiKey);
            q.bindValue(6, provider.timeoutMs);
            q.bindValue(7, provider.isEnabled ? 1 : 0);
            if (!q.exec()) qWarning() << "[saveProvider] custom error:" << q.lastError().text();
        } else {
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "INSERT INTO user_provider_overrides (provider_id, is_enabled, base_url_override, api_key) "
                "VALUES (?, ?, ?, ?) "
                "ON CONFLICT(provider_id) DO UPDATE SET "
                "  is_enabled = excluded.is_enabled, "
                "  base_url_override = excluded.base_url_override, "
                "  api_key = excluded.api_key;"
            ));
            q.bindValue(0, provider.id);
            q.bindValue(1, provider.isEnabled ? 1 : 0);
            q.bindValue(2, provider.baseUrl.isEmpty() ? QVariant() : QVariant(provider.baseUrl));
            q.bindValue(3, provider.apiKey);
            if (!q.exec()) qWarning() << "[saveProvider] override error:" << q.lastError().text();
        }
    }

    void SqliteModelRepository::deleteProvider(const QString &providerId) {
        auto db = getDatabase();
        data::sqlite::SqlTransaction tx(db);
        QSqlQuery delModels(db);
        delModels.prepare(QStringLiteral("DELETE FROM user_custom_models WHERE provider_id = ?;"));
        delModels.bindValue(0, providerId);
        delModels.exec();

        QSqlQuery delProvider(db);
        delProvider.prepare(QStringLiteral("DELETE FROM user_custom_providers WHERE id = ?;"));
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
        if (query.exec() && query.next()) return readCanonicalModelRow(query);
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
        while (query.next()) list.append(readCanonicalModelRow(query));
        return list;
    }

    QList<domain::model::ProviderModel> SqliteModelRepository::getProviderModels(const QString &providerId) {
        QList<domain::model::ProviderModel> list;
        auto db = getDatabase();

        // 预置模型（合并 user_model_overrides 的 is_enabled）
        QSqlQuery queryA(db);
        queryA.prepare(QStringLiteral(
            "SELECT pm.provider_id, pm.remote_model_id, pm.canonical_model_id, "
            "       pm.pricing_input, pm.pricing_output, pm.pricing_cache_read, pm.pricing_cache_write, pm.pricing_currency, "
            "       pm.context_limit_override, pm.max_input_override, pm.max_output_override, "
            "       pm.capabilities_override, pm.reasoning_field, "
            "       COALESCE(umo.is_enabled, pm.is_enabled) AS eff_enabled, "
            "       pm.is_custom, pm.origin "
            "FROM preset_provider_models pm "
            "LEFT JOIN user_model_overrides umo ON pm.provider_id = umo.provider_id AND pm.remote_model_id = umo.remote_model_id "
            "WHERE pm.provider_id = ? "
            "ORDER BY pm.remote_model_id ASC;"
        ));
        queryA.bindValue(0, providerId);
        if (queryA.exec()) {
            while (queryA.next()) list.append(readProviderModelRow(queryA));
        }

        // 自定义模型
        QSqlQuery queryB(db);
        queryB.prepare(QStringLiteral(
            "SELECT provider_id, remote_model_id, canonical_model_id, 0.0, 0.0, 0.0, 0.0, 'USD', "
            "       NULL, NULL, NULL, NULL, '', is_enabled, 0, origin "
            "FROM user_custom_models WHERE provider_id = ? "
            "ORDER BY remote_model_id ASC;"
        ));
        queryB.bindValue(0, providerId);
        if (queryB.exec()) {
            while (queryB.next()) list.append(readProviderModelRow(queryB));
        }

        return list;
    }

    void SqliteModelRepository::saveProviderModel(const domain::model::ProviderModel &binding) {
        auto db = getDatabase();

        QSqlQuery presetBindingQuery(db);
        presetBindingQuery.prepare(QStringLiteral(
            "SELECT 1 FROM preset_provider_models WHERE provider_id = ? AND remote_model_id = ?;"));
        presetBindingQuery.bindValue(0, binding.providerId);
        presetBindingQuery.bindValue(1, binding.remoteModelId);
        const bool isPresetBinding = presetBindingQuery.exec() && presetBindingQuery.next();

        if (!isPresetBinding && (binding.isCustom || binding.origin == domain::model::DataOrigin::User
            || binding.origin == domain::model::DataOrigin::Discovered)) {
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "INSERT INTO user_custom_models (provider_id, remote_model_id, display_name, canonical_model_id, is_enabled, origin) "
                "VALUES (?, ?, ?, ?, ?, ?) "
                "ON CONFLICT(provider_id, remote_model_id) DO UPDATE SET "
                "  display_name = excluded.display_name, "
                "  canonical_model_id = excluded.canonical_model_id, "
                "  is_enabled = excluded.is_enabled, origin = excluded.origin;"
            ));
            q.bindValue(0, binding.providerId);
            q.bindValue(1, binding.remoteModelId);
            q.bindValue(2, binding.remoteModelId);
            q.bindValue(3, binding.canonicalModelId.has_value()
                ? QVariant(binding.canonicalModelId.value()) : QVariant());
            q.bindValue(4, binding.isEnabled ? 1 : 0);
            q.bindValue(5, originToString(binding.origin));
            if (!q.exec()) qWarning() << "[saveProviderModel] custom error:" << q.lastError().text();
        } else {
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "INSERT INTO user_model_overrides (provider_id, remote_model_id, is_enabled) "
                "VALUES (?, ?, ?) "
                "ON CONFLICT(provider_id, remote_model_id) DO UPDATE SET "
                "  is_enabled = excluded.is_enabled;"
            ));
            q.bindValue(0, binding.providerId);
            q.bindValue(1, binding.remoteModelId);
            q.bindValue(2, binding.isEnabled ? 1 : 0);
            if (!q.exec()) qWarning() << "[saveProviderModel] override error:" << q.lastError().text();
        }
    }

    void SqliteModelRepository::deleteProviderModel(const QString &providerId, const QString &remoteModelId) {
        auto db = getDatabase();
        QSqlQuery q(db);
        q.prepare(QStringLiteral("DELETE FROM user_custom_models WHERE provider_id = ? AND remote_model_id = ?;"));
        q.bindValue(0, providerId);
        q.bindValue(1, remoteModelId);
        q.exec();
    }

    QList<domain::model::ResolvedModel> SqliteModelRepository::getResolvedModelsForProvider(const QString &providerId) {
        QList<domain::model::ResolvedModel> list;
        auto db = getDatabase();
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT "
            "  pp.id, pp.name, pp.icon, pp.doc_url, pp.env_var_name, pp.type, "
            "  pp.base_url, pp.proxy_url, pp.timeout_ms, "
            "  uo.is_enabled, uo.base_url_override, uo.api_key, "
            "  pm.provider_id, pm.remote_model_id, pm.canonical_model_id, "
            "  pm.pricing_input, pm.pricing_output, pm.pricing_cache_read, pm.pricing_cache_write, pm.pricing_currency, "
            "  pm.context_limit_override, pm.max_input_override, pm.max_output_override, "
            "  pm.capabilities_override, pm.reasoning_field, "
            "  COALESCE(umo.is_enabled, pm.is_enabled), pm.is_custom, pm.origin, "
            "  cm.id, cm.name, cm.family, cm.description, cm.capabilities, "
            "  cm.context_limit, cm.max_input_limit, cm.max_output_limit, "
            "  cm.modalities_input, cm.modalities_output, "
            "  cm.default_temperature, cm.default_top_p, cm.default_enable_thinking, cm.default_thinking_budget_tokens, "
            "  cm.open_weights, cm.knowledge_cutoff, cm.release_date, pm.reasoning_options AS reasoning_options "
            "FROM preset_provider_models pm "
            "JOIN preset_providers pp ON pm.provider_id = pp.id "
            "LEFT JOIN user_provider_overrides uo ON pp.id = uo.provider_id "
            "LEFT JOIN user_model_overrides umo ON pm.provider_id = umo.provider_id AND pm.remote_model_id = umo.remote_model_id "
            "LEFT JOIN canonical_models cm ON pm.canonical_model_id = cm.id "
            "WHERE pm.provider_id = ? "
            "ORDER BY cm.family ASC, pm.remote_model_id ASC;"
        ));
        query.bindValue(0, providerId);
        if (query.exec()) {
            while (query.next()) list.append(buildResolvedFromRow(query));
        }
        list.append(getCustomResolvedModels(db, QStringLiteral("ucm.provider_id = ?"), {providerId}));
        return list;
    }

    QList<domain::model::ResolvedModel> SqliteModelRepository::getAllResolvedModels() {
        QList<domain::model::ResolvedModel> list;
        auto db = getDatabase();
        QSqlQuery query(QStringLiteral(
            "SELECT "
            "  pp.id, pp.name, pp.icon, pp.doc_url, pp.env_var_name, pp.type, "
            "  pp.base_url, pp.proxy_url, pp.timeout_ms, "
            "  uo.is_enabled, uo.base_url_override, uo.api_key, "
            "  pm.provider_id, pm.remote_model_id, pm.canonical_model_id, "
            "  pm.pricing_input, pm.pricing_output, pm.pricing_cache_read, pm.pricing_cache_write, pm.pricing_currency, "
            "  pm.context_limit_override, pm.max_input_override, pm.max_output_override, "
            "  pm.capabilities_override, pm.reasoning_field, "
            "  COALESCE(umo.is_enabled, pm.is_enabled), pm.is_custom, pm.origin, "
            "  cm.id, cm.name, cm.family, cm.description, cm.capabilities, "
            "  cm.context_limit, cm.max_input_limit, cm.max_output_limit, "
            "  cm.modalities_input, cm.modalities_output, "
            "  cm.default_temperature, cm.default_top_p, cm.default_enable_thinking, cm.default_thinking_budget_tokens, "
            "  cm.open_weights, cm.knowledge_cutoff, cm.release_date, pm.reasoning_options AS reasoning_options "
            "FROM preset_provider_models pm "
            "JOIN preset_providers pp ON pm.provider_id = pp.id "
            "LEFT JOIN user_provider_overrides uo ON pp.id = uo.provider_id "
            "LEFT JOIN user_model_overrides umo ON pm.provider_id = umo.provider_id AND pm.remote_model_id = umo.remote_model_id "
            "LEFT JOIN canonical_models cm ON pm.canonical_model_id = cm.id "
            "ORDER BY pp.name ASC, cm.family ASC, pm.remote_model_id ASC;"
        ), db);
        while (query.next()) list.append(buildResolvedFromRow(query));
        list.append(getCustomResolvedModels(db, QStringLiteral("1 = 1")));
        return list;
    }

    QList<domain::model::ResolvedModel> SqliteModelRepository::getEnabledResolvedModels() {
        QList<domain::model::ResolvedModel> list;
        auto db = getDatabase();
        QSqlQuery query(QStringLiteral(
            "SELECT "
            "  pp.id, pp.name, pp.icon, pp.doc_url, pp.env_var_name, pp.type, "
            "  pp.base_url, pp.proxy_url, pp.timeout_ms, "
            "  uo.is_enabled, uo.base_url_override, uo.api_key, "
            "  pm.provider_id, pm.remote_model_id, pm.canonical_model_id, "
            "  pm.pricing_input, pm.pricing_output, pm.pricing_cache_read, pm.pricing_cache_write, pm.pricing_currency, "
            "  pm.context_limit_override, pm.max_input_override, pm.max_output_override, "
            "  pm.capabilities_override, pm.reasoning_field, "
            "  COALESCE(umo.is_enabled, pm.is_enabled), pm.is_custom, pm.origin, "
            "  cm.id, cm.name, cm.family, cm.description, cm.capabilities, "
            "  cm.context_limit, cm.max_input_limit, cm.max_output_limit, "
            "  cm.modalities_input, cm.modalities_output, "
            "  cm.default_temperature, cm.default_top_p, cm.default_enable_thinking, cm.default_thinking_budget_tokens, "
            "  cm.open_weights, cm.knowledge_cutoff, cm.release_date, pm.reasoning_options AS reasoning_options "
            "FROM preset_provider_models pm "
            "JOIN preset_providers pp ON pm.provider_id = pp.id "
            "LEFT JOIN user_provider_overrides uo ON pp.id = uo.provider_id "
            "LEFT JOIN user_model_overrides umo ON pm.provider_id = umo.provider_id AND pm.remote_model_id = umo.remote_model_id "
            "LEFT JOIN canonical_models cm ON pm.canonical_model_id = cm.id "
            "WHERE uo.is_enabled = 1 AND COALESCE(umo.is_enabled, pm.is_enabled) = 1 "
            "ORDER BY pp.name ASC, cm.family ASC, pm.remote_model_id ASC;"
        ), db);
        while (query.next()) list.append(buildResolvedFromRow(query));
        list.append(getCustomResolvedModels(db, QStringLiteral("COALESCE(uo.is_enabled, cp.is_enabled, 0) = 1 AND ucm.is_enabled = 1")));
        return list;
    }

    std::optional<domain::model::ResolvedModel> SqliteModelRepository::resolveModel(
        const QString &providerId, const QString &remoteModelId)
    {
        auto db = getDatabase();
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT "
            "  pp.id, pp.name, pp.icon, pp.doc_url, pp.env_var_name, pp.type, "
            "  pp.base_url, pp.proxy_url, pp.timeout_ms, "
            "  uo.is_enabled, uo.base_url_override, uo.api_key, "
            "  pm.provider_id, pm.remote_model_id, pm.canonical_model_id, "
            "  pm.pricing_input, pm.pricing_output, pm.pricing_cache_read, pm.pricing_cache_write, pm.pricing_currency, "
            "  pm.context_limit_override, pm.max_input_override, pm.max_output_override, "
            "  pm.capabilities_override, pm.reasoning_field, "
            "  COALESCE(umo.is_enabled, pm.is_enabled), pm.is_custom, pm.origin, "
            "  cm.id, cm.name, cm.family, cm.description, cm.capabilities, "
            "  cm.context_limit, cm.max_input_limit, cm.max_output_limit, "
            "  cm.modalities_input, cm.modalities_output, "
            "  cm.default_temperature, cm.default_top_p, cm.default_enable_thinking, cm.default_thinking_budget_tokens, "
            "  cm.open_weights, cm.knowledge_cutoff, cm.release_date, pm.reasoning_options AS reasoning_options "
            "FROM preset_provider_models pm "
            "JOIN preset_providers pp ON pm.provider_id = pp.id "
            "LEFT JOIN user_provider_overrides uo ON pp.id = uo.provider_id "
            "LEFT JOIN user_model_overrides umo ON pm.provider_id = umo.provider_id AND pm.remote_model_id = umo.remote_model_id "
            "LEFT JOIN canonical_models cm ON pm.canonical_model_id = cm.id "
            "WHERE pm.provider_id = ? AND pm.remote_model_id = ?;"
        ));
        query.bindValue(0, providerId);
        query.bindValue(1, remoteModelId);
        if (query.exec() && query.next()) return buildResolvedFromRow(query);

        const auto customModels = getCustomResolvedModels(
            db,
            QStringLiteral("ucm.provider_id = ? AND ucm.remote_model_id = ?"),
            {providerId, remoteModelId});
        if (!customModels.isEmpty()) return customModels.first();
        return std::nullopt;
    }

} // namespace data::repository
