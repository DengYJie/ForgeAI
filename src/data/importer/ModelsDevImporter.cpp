#include "ModelsDevImporter.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

namespace data::importer {

    domain::model::ProviderType ModelsDevImporter::mapNpmToProviderType(QStringView npm) {
        if (npm == u"@ai-sdk/anthropic") {
            return domain::model::ProviderType::Anthropic;
        }
        if (npm == u"@ai-sdk/google") {
            return domain::model::ProviderType::GoogleGemini;
        }
        if (npm == u"@ai-sdk/azure") {
            return domain::model::ProviderType::AzureOpenAI;
        }
        if (npm == u"@ai-sdk/amazon-bedrock") {
            return domain::model::ProviderType::AmazonBedrock;
        }
        if (npm == u"@ai-sdk/openai") {
            return domain::model::ProviderType::OpenAIResponses;
        }
        return domain::model::ProviderType::OpenAIChatCompletionsCompatible;
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

        QJsonArray envArray = providerObj.value(QStringLiteral("env")).toArray();
        if (!envArray.isEmpty()) {
            provider.envVarName = envArray.first().toString();
        }

        QString npm = providerObj.value(QStringLiteral("npm")).toString();
        provider.type = mapNpmToProviderType(npm);
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
        const QJsonObject &modelObj
    ) {
        domain::model::ProviderModel binding;
        binding.providerId = providerId;
        binding.remoteModelId = modelObj.value(QStringLiteral("id")).toString(modelKey);
        binding.canonicalModelId = binding.remoteModelId;

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
    ModelsDevImporter::parseProvidersAndBindings(const QJsonObject &apiRoot) {
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

                auto binding = parseProviderModel(providerId, provider.name, modelKey, modelObj);
                providerModels.append(binding);
            }

            providers.append(std::move(provider));
        }

        return { std::move(providers), std::move(providerModels) };
    }

    ModelsDevImportResult ModelsDevImporter::parseAll(const QJsonObject &apiRoot, const QJsonObject &modelsRoot) {
        ModelsDevImportResult result;

        result.canonicalModels = parseCanonicalModels(modelsRoot);

        auto [providers, bindings] = parseProvidersAndBindings(apiRoot);
        result.providers = std::move(providers);
        result.providerModels = std::move(bindings);

        for (auto &binding : result.providerModels) {
            if (binding.canonicalModelId.has_value()) {
                const QString &cId = *binding.canonicalModelId;
                if (!result.canonicalModels.contains(cId)) {
                    result.unresolvedBindingsCount++;
                }
            }
        }

        return result;
    }

} // namespace data::importer
