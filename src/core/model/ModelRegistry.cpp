#include "ModelRegistry.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

namespace core::model {

    ModelRegistry::ModelRegistry(std::shared_ptr<domain::repository::IModelRepository> repository, QObject *parent)
        : QObject(parent), m_repository(std::move(repository)) {
    }

    bool ModelRegistry::initialize(const QString &presetJsonPath) {
        if (!presetJsonPath.isEmpty() && QFile::exists(presetJsonPath)) {
            loadPresetJson(presetJsonPath);
        }

        if (m_repository) {
            auto dbProviders = m_repository->getAllProviders();
            for (const auto &provider : dbProviders) {
                m_providers.insert(provider.id, provider);
            }
        }

        rebuildModelIndex();

        emit providersChanged();
        emit modelsChanged();
        return true;
    }

    void ModelRegistry::loadPresetJson(const QString &jsonPath) {
        QFile file(jsonPath);
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }

        QByteArray data = file.readAll();
        file.close();

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            return;
        }

        QJsonObject rootObj = doc.object();
        for (auto providerIt = rootObj.begin(); providerIt != rootObj.end(); ++providerIt) {
            QString providerId = providerIt.key();
            QJsonObject providerObj = providerIt.value().toObject();

            QJsonObject modelsObj = providerObj.value(QStringLiteral("models")).toObject();
            for (auto modelIt = modelsObj.begin(); modelIt != modelsObj.end(); ++modelIt) {
                QString modelKey = modelIt.key();
                QJsonObject modelObj = modelIt.value().toObject();

                domain::model::Model model;
                model.id = modelObj.value(QStringLiteral("id")).toString(modelKey);
                model.providerId = providerId;
                model.displayName = modelObj.value(QStringLiteral("name")).toString(model.id);
                model.description = modelObj.value(QStringLiteral("description")).toString();
                model.family = modelObj.value(QStringLiteral("family")).toString();
                model.group = providerObj.value(QStringLiteral("name")).toString(providerId);

                // 上下文限制
                QJsonObject limitObj = modelObj.value(QStringLiteral("limit")).toObject();
                model.limits.context = limitObj.value(QStringLiteral("context")).toInt(128000);
                model.limits.maxInput = limitObj.value(QStringLiteral("input")).toInt(model.limits.context);
                model.limits.maxOutput = limitObj.value(QStringLiteral("output")).toInt(8192);

                // 能力标志解析
                domain::model::ModelCapabilities caps = domain::model::ModelCapability::Chat;
                if (modelObj.value(QStringLiteral("tool_call")).toBool(false)) {
                    caps |= domain::model::ModelCapability::ToolCalling;
                }
                if (modelObj.value(QStringLiteral("reasoning")).toBool(false)) {
                    caps |= domain::model::ModelCapability::Thinking;
                }
                if (modelObj.value(QStringLiteral("structured_output")).toBool(false)) {
                    caps |= domain::model::ModelCapability::StructuredOutputs;
                }

                QJsonObject modalitiesObj = modelObj.value(QStringLiteral("modalities")).toObject();
                QJsonArray inputArray = modalitiesObj.value(QStringLiteral("input")).toArray();
                for (const auto &val : inputArray) {
                    QString mod = val.toString();
                    if (mod == QStringLiteral("image")) caps |= domain::model::ModelCapability::Vision;
                    else if (mod == QStringLiteral("audio")) caps |= domain::model::ModelCapability::Audio;
                    else if (mod == QStringLiteral("video")) caps |= domain::model::ModelCapability::Video;
                    else if (mod == QStringLiteral("pdf")) caps |= domain::model::ModelCapability::Pdf;
                }
                model.capabilities = caps;

                // 计费解析
                QJsonObject costObj = modelObj.value(QStringLiteral("cost")).toObject();
                model.pricing.inputPrice = costObj.value(QStringLiteral("input")).toDouble(0.0);
                model.pricing.outputPrice = costObj.value(QStringLiteral("output")).toDouble(0.0);
                model.pricing.cacheReadPrice = costObj.value(QStringLiteral("cache_read")).toDouble(0.0);
                model.pricing.cacheWritePrice = costObj.value(QStringLiteral("cache_write")).toDouble(0.0);
                model.pricing.currency = QStringLiteral("USD");

                // 思考流字段与发布信息
                QJsonObject interleavedObj = modelObj.value(QStringLiteral("interleaved")).toObject();
                model.reasoningField = interleavedObj.value(QStringLiteral("field")).toString();
                model.openWeights = modelObj.value(QStringLiteral("open_weights")).toBool(false);
                model.knowledgeCutoff = modelObj.value(QStringLiteral("knowledge")).toString();

                m_presetTemplates.insert(model.id, model);
            }
        }
    }

    void ModelRegistry::rebuildModelIndex() {
        m_models.clear();

        for (const auto &provider : m_providers) {
            if (!provider.isEnabled) {
                continue;
            }
            for (const auto &model : provider.models) {
                if (model.isEnabled) {
                    m_models.insert(model.id, model);
                }
            }
        }
    }

    QList<domain::model::ModelProvider> ModelRegistry::getActiveProviders() const {
        return m_providers.values();
    }

    std::optional<domain::model::ModelProvider> ModelRegistry::getProvider(const QString &providerId) const {
        auto it = m_providers.find(providerId);
        if (it != m_providers.end()) {
            return it.value();
        }
        return std::nullopt;
    }

    void ModelRegistry::saveProvider(const domain::model::ModelProvider &provider) {
        m_providers.insert(provider.id, provider);

        if (m_repository) {
            m_repository->saveProvider(provider);
        }

        rebuildModelIndex();
        emit providersChanged();
        emit modelsChanged();
    }

    void ModelRegistry::deleteProvider(const QString &providerId) {
        if (m_providers.remove(providerId)) {
            if (m_repository) {
                m_repository->deleteProvider(providerId);
            }
            rebuildModelIndex();
            emit providersChanged();
            emit modelsChanged();
        }
    }

    QList<domain::model::Model> ModelRegistry::getEnabledModels() const {
        return m_models.values();
    }

    std::optional<std::pair<domain::model::Model, domain::model::ModelProvider>> ModelRegistry::resolve(const QString &modelId) const {
        auto modelIt = m_models.find(modelId);
        if (modelIt == m_models.end()) {
            return std::nullopt;
        }

        const domain::model::Model &model = modelIt.value();
        auto providerIt = m_providers.find(model.providerId);
        if (providerIt == m_providers.end()) {
            return std::nullopt;
        }

        return std::make_pair(model, providerIt.value());
    }

    bool ModelRegistry::hasCapability(const QString &modelId, domain::model::ModelCapability cap) const {
        auto it = m_models.find(modelId);
        if (it != m_models.end()) {
            return it.value().capabilities.testFlag(cap);
        }
        return false;
    }

    QList<domain::model::Model> ModelRegistry::hydrateDiscoveredModels(
        const QString &providerId,
        const QList<domain::model::Model> &discoveredModels) const {
        
        QList<domain::model::Model> result;
        QHash<QString, domain::model::Model> existingMap;

        auto providerIt = m_providers.find(providerId);
        if (providerIt != m_providers.end()) {
            for (const auto &m : providerIt.value().models) {
                existingMap.insert(m.id, m);
            }
        }

        for (const auto &raw : discoveredModels) {
            domain::model::Model hydrated;

            // 1. 优先匹配本地内置预设模板库
            auto templateIt = m_presetTemplates.find(raw.id);
            if (templateIt != m_presetTemplates.end()) {
                hydrated = templateIt.value();
                hydrated.providerId = providerId;
                if (!raw.displayName.isEmpty() && raw.displayName != raw.id) {
                    hydrated.displayName = raw.displayName;
                }
            } else if (existingMap.contains(raw.id)) {
                // 2. 复用该服务商已有配置
                hydrated = existingMap.value(raw.id);
            } else {
                // 3. 全新未知模型，赋予安全默认配置
                hydrated = raw;
                hydrated.providerId = providerId;
                if (hydrated.displayName.isEmpty()) {
                    hydrated.displayName = raw.id;
                }
                hydrated.capabilities = domain::model::ModelCapability::Chat |
                                        domain::model::ModelCapability::Streaming;
                hydrated.limits.context = 128000;
                hydrated.limits.maxInput = 128000;
                hydrated.limits.maxOutput = 4096;
            }

            // 4. 保留用户本地开关状态
            if (existingMap.contains(raw.id)) {
                hydrated.isEnabled = existingMap.value(raw.id).isEnabled;
            }

            result.append(hydrated);
        }

        return result;
    }

    void ModelRegistry::scanLocalOllamaModels(const QString &ollamaBaseUrl) {
        // Ollama 本地模型探测实现入口
        Q_UNUSED(ollamaBaseUrl)
    }

} // namespace core::model
