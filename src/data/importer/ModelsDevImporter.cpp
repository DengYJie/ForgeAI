#include "ModelsDevImporter.h"
#include <QJsonArray>
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
        provider.isEnabled = true;

        return provider;
    }

    domain::model::Model ModelsDevImporter::parseModel(
        const QString &providerId,
        const QString &providerName,
        const QString &modelKey,
        const QJsonObject &modelObj
    ) {
        domain::model::Model model;
        model.id = modelObj.value(QStringLiteral("id")).toString(modelKey);
        model.providerId = providerId;
        model.displayName = modelObj.value(QStringLiteral("name")).toString(model.id);
        model.description = modelObj.value(QStringLiteral("description")).toString();
        model.family = modelObj.value(QStringLiteral("family")).toString();
        model.group = providerName.isEmpty() ? providerId : providerName;

        // 上下文与输出限制
        QJsonObject limitObj = modelObj.value(QStringLiteral("limit")).toObject();
        model.limits.context = limitObj.value(QStringLiteral("context")).toInt(128000);
        model.limits.maxInput = limitObj.value(QStringLiteral("input")).toInt(model.limits.context);
        model.limits.maxOutput = limitObj.value(QStringLiteral("output")).toInt(8192);

        // 能力标志
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
        for (const auto &val : inputArray) {
            QString mod = val.toString();
            if (mod == QStringLiteral("image")) caps |= domain::model::ModelCapability::Vision;
            else if (mod == QStringLiteral("audio")) caps |= domain::model::ModelCapability::Audio;
            else if (mod == QStringLiteral("video")) caps |= domain::model::ModelCapability::Video;
            else if (mod == QStringLiteral("pdf")) caps |= domain::model::ModelCapability::Pdf;
        }
        model.capabilities = caps;

        // 默认推理参数
        model.defaultParams.temperature = 0.7;
        model.defaultParams.topP = 1.0;
        model.defaultParams.enableThinking = modelObj.value(QStringLiteral("reasoning")).toBool(false);
        model.defaultParams.thinkingBudgetTokens = 4096;

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

        model.isEnabled = true;
        model.isCustom = false;

        return model;
    }

    ModelsDevImporter::ParseResult ModelsDevImporter::parseAll(const QJsonObject &root) {
        ParseResult result;
        result.providers.reserve(root.size());

        for (auto providerIt = root.begin(); providerIt != root.end(); ++providerIt) {
            QString providerId = providerIt.key();
            QJsonObject providerObj = providerIt.value().toObject();

            auto provider = parseProvider(providerId, providerObj);

            QJsonObject modelsObj = providerObj.value(QStringLiteral("models")).toObject();
            for (auto modelIt = modelsObj.begin(); modelIt != modelsObj.end(); ++modelIt) {
                QString modelKey = modelIt.key();
                QJsonObject modelObj = modelIt.value().toObject();

                auto model = parseModel(providerId, provider.name, modelKey, modelObj);
                provider.models.append(model);
                result.models.append(model);
            }

            result.providers.append(std::move(provider));
        }

        return result;
    }

} // namespace data::importer
