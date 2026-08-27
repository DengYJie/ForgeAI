#include "ModelsDevImporter.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

namespace data::importer {

    static const QHash<QString, domain::model::ProtocolType> kProviderOverrides{
        {QStringLiteral("openai"), domain::model::ProtocolType::OpenAIResponses},
        {QStringLiteral("deepseek"), domain::model::ProtocolType::OpenAIResponses},
        {QStringLiteral("ollama"), domain::model::ProtocolType::OllamaChat}
    };

    domain::model::ProtocolType ModelsDevImporter::resolveProtocol(
        const QString &providerId,
        const QJsonObject &providerObj
    ) {
        // 1. ForgeAI 显式内置 Provider Override 优先
        if (const auto it = kProviderOverrides.constFind(providerId); it != kProviderOverrides.cend()) {
            return it.value();
        }

        // 2. models.dev npm 适配器家族映射（作为 AdapterFamilyHint）
        // 注意：Google Vertex AI (@ai-sdk/google-vertex/*) 虽在 Body 格式上接近，
        // 但其 Endpoint URL 结构（projects/{p}/locations/{l}/publishers/...）与 GCP IAM OAuth2 Bearer 认证
        // 与公开 API 存在根本差异，当前适配器未支持 GCP 鉴权体系前，不可混淆映射。
        const QString npm = providerObj.value(QStringLiteral("npm")).toString();
        if (npm == u"@ai-sdk/anthropic") {
            return domain::model::ProtocolType::AnthropicMessages;
        }
        if (npm == u"@ai-sdk/google") {
            return domain::model::ProtocolType::GeminiGenerateContent;
        }
        if (npm == u"@ai-sdk/ollama") {
            return domain::model::ProtocolType::OllamaChat;
        }
        if (npm == u"@ai-sdk/azure") {
            return domain::model::ProtocolType::AzureOpenAI;
        }
        if (npm == u"@ai-sdk/amazon-bedrock") {
            return domain::model::ProtocolType::AmazonBedrock;
        }
        if (npm == u"@ai-sdk/openai-compatible" ||
            npm == u"@ai-sdk/openai" ||
            npm == u"@openrouter/ai-sdk-provider" ||
            npm == u"@ai-sdk/groq" ||
            npm == u"@ai-sdk/xai" ||
            npm == u"@ai-sdk/togetherai" ||
            npm == u"@ai-sdk/deepinfra" ||
            npm == u"@ai-sdk/cerebras" ||
            npm == u"@ai-sdk/mistral" ||
            npm == u"@ai-sdk/perplexity" ||
            npm == u"@aihubmix/ai-sdk-provider" ||
            npm == u"@saladtechnologies-oss/ai-sdk-provider" ||
            npm == u"ai-gateway-provider" ||
            npm == u"merge-gateway-ai-sdk-provider" ||
            npm == u"venice-ai-sdk-provider") {
            return domain::model::ProtocolType::OpenAIChatCompletions;
        }

        // 3. 真正未知/不支持的专属包（如 @ai-sdk/cohere、gitlab-ai-provider 等）坚决返回 Unknown
        return domain::model::ProtocolType::Unknown;
    }

    domain::model::CanonicalModel ModelsDevImporter::parseCanonicalModel(
        const QString &modelKey,
        const QJsonObject &modelObj
    ) {
        domain::model::CanonicalModel model;
        model.id = modelObj.value(QStringLiteral("id")).toString(modelKey);
        model.name = modelObj.value(QStringLiteral("name")).toString(model.id);
        model.family = modelObj.value(QStringLiteral("family")).toString();
        model.description = modelObj.value(QStringLiteral("description")).toString();

        QJsonObject limitObj = modelObj.value(QStringLiteral("limit")).toObject();
        model.limits.context = limitObj.value(QStringLiteral("context")).toInt(128000);
        model.limits.maxInput = limitObj.value(QStringLiteral("input")).toInt(model.limits.context);
        model.limits.maxOutput = limitObj.value(QStringLiteral("output")).toInt(8192);

        domain::model::ModelCapabilities caps = domain::model::ModelCapability::Chat | domain::model::ModelCapability::Streaming;
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
        if (!inputArray.isEmpty()) {
            model.modalities.input.clear();
            for (const auto &val : inputArray) {
                QString mod = val.toString();
                model.modalities.input.append(mod);
                if (mod == QStringLiteral("image")) caps |= domain::model::ModelCapability::Vision;
                else if (mod == QStringLiteral("audio")) caps |= domain::model::ModelCapability::Audio;
                else if (mod == QStringLiteral("video")) caps |= domain::model::ModelCapability::Video;
                else if (mod == QStringLiteral("pdf")) caps |= domain::model::ModelCapability::Pdf;
            }
        }

        QJsonArray outputArray = modalitiesObj.value(QStringLiteral("output")).toArray();
        if (!outputArray.isEmpty()) {
            model.modalities.output.clear();
            for (const auto &val : outputArray) {
                model.modalities.output.append(val.toString());
            }
        }
        model.capabilities = caps;

        model.defaultParams.temperature = 0.7;
        model.defaultParams.topP = 1.0;
        model.defaultParams.enableThinking = modelObj.value(QStringLiteral("reasoning")).toBool(false);
        model.defaultParams.thinkingBudgetTokens = 4096;

        model.openWeights = modelObj.value(QStringLiteral("open_weights")).toBool(false);
        model.knowledgeCutoff = modelObj.value(QStringLiteral("knowledge")).toString();
        model.releaseDate = modelObj.value(QStringLiteral("release_date")).toString();

        return model;
    }

    domain::model::ModelProvider ModelsDevImporter::parseProvider(const QString &id, const QJsonObject &providerObj) {
        domain::model::ModelProvider provider;
        provider.id = id;
        provider.name = providerObj.value(QStringLiteral("name")).toString(id);
        provider.docUrl = providerObj.value(QStringLiteral("doc")).toString();
        provider.baseUrl = providerObj.value(QStringLiteral("api")).toString();
        provider.sdkPackage = providerObj.value(QStringLiteral("npm")).toString();

        QJsonArray envArray = providerObj.value(QStringLiteral("env")).toArray();
        if (!envArray.isEmpty()) {
            provider.envVarName = envArray.first().toString();
        }

        provider.protocol = resolveProtocol(id, providerObj);
        provider.timeoutMs = 60000;
        provider.isEnabled = false;
        provider.isCustom = false;
        provider.origin = domain::model::DataOrigin::BuiltIn;

        return provider;
    }

    domain::model::ProviderModel ModelsDevImporter::parseProviderModel(
        const QString &providerId,
        const QString &providerName,
        const QString &modelKey,
        const QJsonObject &modelObj,
        const QHash<QString, QString> &canonicalKeyToId
    ) {
        Q_UNUSED(providerName);
        domain::model::ProviderModel binding;
        binding.providerId = providerId;
        binding.remoteModelId = modelObj.value(QStringLiteral("id")).toString(modelKey);
        if (canonicalKeyToId.contains(modelKey)) {
            binding.canonicalModelId = canonicalKeyToId.value(modelKey);
        } else {
            binding.canonicalModelId = binding.remoteModelId;
        }

        QJsonObject costObj = modelObj.value(QStringLiteral("cost")).toObject();
        binding.pricing.inputPrice = costObj.value(QStringLiteral("input")).toDouble(0.0);
        binding.pricing.outputPrice = costObj.value(QStringLiteral("output")).toDouble(0.0);
        binding.pricing.cacheReadPrice = costObj.value(QStringLiteral("cache_read")).toDouble(0.0);
        binding.pricing.cacheWritePrice = costObj.value(QStringLiteral("cache_write")).toDouble(0.0);
        binding.pricing.currency = QStringLiteral("USD");

        if (modelObj.contains(QStringLiteral("limit"))) {
            QJsonObject limitObj = modelObj.value(QStringLiteral("limit")).toObject();
            domain::model::ModelLimit lim;
            lim.context = limitObj.value(QStringLiteral("context")).toInt(128000);
            lim.maxInput = limitObj.value(QStringLiteral("input")).toInt(lim.context);
            lim.maxOutput = limitObj.value(QStringLiteral("output")).toInt(8192);
            binding.limitsOverride = lim;
        }

        QJsonObject interleavedObj = modelObj.value(QStringLiteral("interleaved")).toObject();
        binding.reasoningField = interleavedObj.value(QStringLiteral("field")).toString();
        const QJsonArray reasoningOptions = modelObj.value(QStringLiteral("reasoning_options")).toArray();
        binding.reasoningOptionsJson = QString::fromUtf8(QJsonDocument(reasoningOptions).toJson(QJsonDocument::Compact));

        // 解析结构化推理支持
        if (modelObj.value(QStringLiteral("reasoning")).toBool(false) || !reasoningOptions.isEmpty()) {
            domain::model::ReasoningSupport supp;
            supp.supported = true;
            for (const auto &optVal : reasoningOptions) {
                if (optVal.isObject()) {
                    QJsonObject optObj = optVal.toObject();
                    QString typeStr = optObj.value(QStringLiteral("type")).toString();
                    if (typeStr == QStringLiteral("effort")) {
                        for (const auto &e : optObj.value(QStringLiteral("effort")).toArray()) {
                            supp.effortLevels.append(e.toString());
                        }
                    } else if (typeStr == QStringLiteral("budget")) {
                        if (optObj.contains(QStringLiteral("min"))) {
                            supp.minBudgetTokens = optObj.value(QStringLiteral("min")).toInt();
                        }
                        if (optObj.contains(QStringLiteral("max"))) {
                            supp.maxBudgetTokens = optObj.value(QStringLiteral("max")).toInt();
                        }
                        if (optObj.contains(QStringLiteral("default"))) {
                            supp.defaultBudgetTokens = optObj.value(QStringLiteral("default")).toInt();
                        }
                    }
                }
            }
            binding.reasoningSupport = supp;
        }

        binding.isEnabled = true;
        binding.isCustom = false;
        binding.origin = domain::model::DataOrigin::BuiltIn;

        return binding;
    }

    QHash<QString, domain::model::CanonicalModel> ModelsDevImporter::parseCanonicalModels(const QJsonObject &modelsRoot) {
        QHash<QString, domain::model::CanonicalModel> canonicals;
        canonicals.reserve(modelsRoot.size());

        for (auto it = modelsRoot.begin(); it != modelsRoot.end(); ++it) {
            QString modelKey = it.key();
            QJsonObject modelObj = it.value().toObject();
            auto model = parseCanonicalModel(modelKey, modelObj);
            canonicals.insert(model.id, model);
        }

        return canonicals;
    }

    std::pair<QList<domain::model::ModelProvider>, QList<domain::model::ProviderModel>>
    ModelsDevImporter::parseProvidersAndBindings(const QJsonObject &apiRoot, const QHash<QString, QString> &canonicalKeyToId) {
        QList<domain::model::ModelProvider> providers;
        QList<domain::model::ProviderModel> providerModels;
        providers.reserve(apiRoot.size());

        for (auto providerIt = apiRoot.begin(); providerIt != apiRoot.end(); ++providerIt) {
            QString providerId = providerIt.key();
            QJsonObject providerObj = providerIt.value().toObject();

            auto provider = parseProvider(providerId, providerObj);

            QJsonObject modelsObj = providerObj.value(QStringLiteral("models")).toObject();
            for (auto modelIt = modelsObj.begin(); modelIt != modelsObj.end(); ++modelIt) {
                QString modelKey = modelIt.key();
                QJsonObject modelObj = modelIt.value().toObject();

                auto binding = parseProviderModel(providerId, provider.name, modelKey, modelObj, canonicalKeyToId);
                providerModels.append(binding);
            }

            providers.append(std::move(provider));
        }

        return { std::move(providers), std::move(providerModels) };
    }

    ModelsDevImportResult ModelsDevImporter::parseAll(const QJsonObject &apiRoot, const QJsonObject &modelsRoot) {
        ModelsDevImportResult result;

        QHash<QString, QString> canonicalKeyToId;
        result.canonicalModels.reserve(modelsRoot.size());

        for (auto it = modelsRoot.begin(); it != modelsRoot.end(); ++it) {
            QString modelKey = it.key();
            QJsonObject modelObj = it.value().toObject();
            auto model = parseCanonicalModel(modelKey, modelObj);
            canonicalKeyToId.insert(modelKey, model.id);
            result.canonicalModels.insert(model.id, model);
        }

        auto [providers, bindings] = parseProvidersAndBindings(apiRoot, canonicalKeyToId);
        result.providers = std::move(providers);
        result.providerModels = std::move(bindings);

        for (auto &binding : result.providerModels) {
            if (binding.canonicalModelId.has_value()) {
                const QString &cId = *binding.canonicalModelId;
                if (!result.canonicalModels.contains(cId)) {
                    result.unresolvedBindingsCount++;
                    binding.canonicalModelId = std::nullopt;
                }
            }
        }

        return result;
    }

} // namespace data::importer
