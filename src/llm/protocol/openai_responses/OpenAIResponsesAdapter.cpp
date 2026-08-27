#include "OpenAIResponsesAdapter.h"
#include "OpenAIResponsesStreamParser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace llm::protocol::openai_responses {

    OpenAIResponsesAdapter::OpenAIResponsesAdapter() = default;
    OpenAIResponsesAdapter::~OpenAIResponsesAdapter() = default;

    network::HttpRequest OpenAIResponsesAdapter::buildChatRequest(
        const domain::model::ResolvedModel &model,
        const domain::llm::ChatRequest &request,
        const domain::llm::ResolvedChatOptions &options) const {
        
        const auto &provider = model.provider;
        network::HttpRequest netReq;
        QString baseUrl = provider.baseUrl.isEmpty() ? "https://api.openai.com" : provider.baseUrl;
        if (baseUrl.endsWith('/')) baseUrl.chop(1);
        if (baseUrl.endsWith(QStringLiteral("/v1"))) {
            netReq.url = baseUrl + "/responses";
        } else {
            netReq.url = baseUrl + "/v1/responses";
        }
        netReq.method = network::HttpMethod::Post;
        netReq.timeoutMs = provider.timeoutMs;

        netReq.headers.insert("Content-Type", "application/json");
        if (!provider.apiKey.isEmpty()) {
            netReq.headers.insert("Authorization", "Bearer " + provider.apiKey);
        }
        for (auto it = provider.customHeaders.constBegin(); it != provider.customHeaders.constEnd(); ++it) {
            netReq.headers.insert(it.key(), it.value());
        }

        QJsonObject bodyObj;
        bodyObj.insert("model", request.model);
        bodyObj.insert("stream", request.stream.value_or(true));

        QJsonArray inputArray;
        for (const auto &msg : request.messages) {
            if (msg.role == domain::MessageRole::Tool) {
                // Responses API represents continuation results as standalone
                // function_call_output items, not Chat Completions' role=tool.
                QJsonObject output;
                output.insert("type", "function_call_output");
                output.insert("call_id", msg.toolCallId);
                output.insert("output", msg.content);
                inputArray.append(output);
                continue;
            }
            if (msg.role == domain::MessageRole::Assistant && msg.toolCalls.has_value() && !msg.toolCalls->isEmpty()) {
                if (!msg.content.isEmpty()) {
                    inputArray.append(QJsonObject{{"role", "assistant"}, {"content", msg.content}});
                }
                for (const auto& call : msg.toolCalls.value()) {
                    inputArray.append(QJsonObject{{"type", "function_call"}, {"call_id", call.id},
                                                  {"name", call.name}, {"arguments", call.arguments}});
                }
                continue;
            }
            QJsonObject msgObj;
            switch (msg.role) {
                case domain::MessageRole::System: msgObj.insert("role", "system"); break;
                case domain::MessageRole::User: msgObj.insert("role", "user"); break;
                case domain::MessageRole::Assistant: msgObj.insert("role", "assistant"); break;
                case domain::MessageRole::Tool: msgObj.insert("role", "tool"); break;
            }
            msgObj.insert("content", msg.content);
            inputArray.append(msgObj);
        }
        bodyObj.insert("input", inputArray);

        if (options.toolsEnabled && request.tools.has_value() && !request.tools->isEmpty()) {
            QJsonArray toolsArr;
            for (const auto &tool : request.tools.value()) {
                QJsonObject toolObj;
                toolObj.insert("type", "function");
                toolObj.insert("name", tool.name);
                toolObj.insert("description", tool.description);
                toolObj.insert("parameters", tool.parameters);
                toolsArr.append(toolObj);
            }
            bodyObj.insert("tools", toolsArr);
        }
        if (options.webSearchEnabled) {
            QJsonArray tools = bodyObj.value("tools").toArray();
            tools.append(QJsonObject{{"type", "web_search_preview"}});
            bodyObj.insert("tools", tools);
        }
        if (options.thinkingEnabled) {
            QJsonObject reasoning;
            if (!options.reasoningEffort.isEmpty()) {
                reasoning.insert("effort", options.reasoningEffort);
            }
            bodyObj.insert("reasoning", reasoning);
        }

        if (options.temperature.has_value()) {
            bodyObj.insert("temperature", options.temperature.value());
        }
        if (options.maxOutputTokens.has_value()) {
            bodyObj.insert("max_output_tokens", options.maxOutputTokens.value());
        }

        QJsonDocument doc(bodyObj);
        netReq.body = doc.toJson(QJsonDocument::Compact);

        return netReq;
    }

    std::unique_ptr<IStreamParser> OpenAIResponsesAdapter::createStreamParser() const {
        return std::make_unique<OpenAIResponsesStreamParser>();
    }

    domain::llm::ChatError OpenAIResponsesAdapter::parseError(int httpStatusCode, const QByteArray &responseBody) const {
        domain::llm::ChatError error;
        error.httpStatus = httpStatusCode;
        error.originalText = QString::fromUtf8(responseBody);

        switch (httpStatusCode) {
            case 400:
                error.category = domain::llm::ChatErrorCategory::Request;
                error.code = QStringLiteral("InvalidRequest");
                error.userMessage = QStringLiteral("OpenAI Responses 请求参数无效。");
                break;
            case 401:
                error.category = domain::llm::ChatErrorCategory::Authentication;
                error.code = QStringLiteral("ApiKeyInvalid");
                error.userMessage = QStringLiteral("API Key 无效或缺失。");
                error.suggestedAction = QStringLiteral("OpenSettings");
                break;
            case 403:
                error.category = domain::llm::ChatErrorCategory::Authorization;
                error.code = QStringLiteral("Forbidden");
                error.userMessage = QStringLiteral("无权访问该 OpenAI 资源。");
                break;
            case 404:
                error.category = domain::llm::ChatErrorCategory::Model;
                error.code = QStringLiteral("ModelNotFound");
                error.userMessage = QStringLiteral("指定的模型不存在。");
                error.suggestedAction = QStringLiteral("ChangeModel");
                break;
            case 429:
                error.category = domain::llm::ChatErrorCategory::RateLimit;
                error.code = QStringLiteral("TooManyRequests");
                error.userMessage = QStringLiteral("请求频率过高，正在重试...");
                error.retryable = true;
                error.suggestedAction = QStringLiteral("Retry");
                break;
            default:
                if (httpStatusCode >= 500) {
                    error.category = domain::llm::ChatErrorCategory::Provider;
                    error.code = QStringLiteral("ServerError");
                    error.userMessage = QStringLiteral("OpenAI 服务端故障。");
                    error.retryable = true;
                    error.suggestedAction = QStringLiteral("Retry");
                } else if (httpStatusCode == 0) {
                    error.category = domain::llm::ChatErrorCategory::Network;
                    error.code = QStringLiteral("NetworkError");
                    error.userMessage = QStringLiteral("网络连接失败。");
                    error.retryable = true;
                } else {
                    error.category = domain::llm::ChatErrorCategory::Unknown;
                    error.code = QStringLiteral("Unknown");
                    error.userMessage = QStringLiteral("遇到未知错误 (HTTP %1)。").arg(httpStatusCode);
                }
                break;
        }

        QJsonParseError parseErr;
        QJsonDocument doc = QJsonDocument::fromJson(responseBody, &parseErr);
        if (parseErr.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("error") && obj.value("error").isObject()) {
                QJsonObject errObj = obj.value("error").toObject();
                if (errObj.contains("message")) {
                    error.message = errObj.value("message").toString();
                }
                if (errObj.contains("code")) {
                    error.providerErrorCode = errObj.value("code").toVariant().toString();
                }
                if (errObj.contains("type")) {
                    QString errType = errObj.value("type").toString();
                    if (errType == "insufficient_quota") {
                        error.category = domain::llm::ChatErrorCategory::Quota;
                        error.code = QStringLiteral("InsufficientQuota");
                        error.userMessage = QStringLiteral("OpenAI 账户额度不足或欠费。");
                        error.retryable = false;
                        error.suggestedAction = QStringLiteral("OpenSettings");
                    }
                }
            }
        }

        if (error.message.isEmpty()) {
            error.message = QString("OpenAI Responses HTTP %1 Error").arg(httpStatusCode);
        }

        return error;
    }

    network::HttpRequest OpenAIResponsesAdapter::buildListModelsRequest(const domain::model::ModelProvider &provider) const {
        network::HttpRequest netReq;
        QString baseUrl = provider.baseUrl.isEmpty() ? "https://api.openai.com" : provider.baseUrl;
        if (baseUrl.endsWith('/')) baseUrl.chop(1);
        if (baseUrl.endsWith(QStringLiteral("/v1"))) {
            netReq.url = baseUrl + "/models";
        } else {
            netReq.url = baseUrl + "/v1/models";
        }
        netReq.method = network::HttpMethod::Get;
        netReq.timeoutMs = provider.timeoutMs;

        if (!provider.apiKey.isEmpty()) {
            netReq.headers.insert("Authorization", "Bearer " + provider.apiKey);
        }
        for (auto it = provider.customHeaders.constBegin(); it != provider.customHeaders.constEnd(); ++it) {
            netReq.headers.insert(it.key(), it.value());
        }

        return netReq;
    }

    QList<domain::model::ProviderModel> OpenAIResponsesAdapter::parseListModelsResponse(
        const QByteArray &responseBody,
        const QString &providerId) const {
        
        QList<domain::model::ProviderModel> models;
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(responseBody, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            return models;
        }

        QJsonObject rootObj = doc.object();
        QJsonArray dataArray = rootObj.value("data").toArray();
        for (const auto &val : dataArray) {
            if (!val.isObject()) continue;
            QJsonObject mObj = val.toObject();
            QString id = mObj.value("id").toString();
            if (id.isEmpty()) continue;

            domain::model::ProviderModel pm;
            pm.remoteModelId = id;
            pm.providerId = providerId;
            pm.isEnabled = true;
            pm.origin = domain::model::DataOrigin::User;
            models.append(pm);
        }

        return models;
    }

} // namespace llm::protocol::openai_responses
