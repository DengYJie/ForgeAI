#include "AnthropicProtocolAdapter.h"
#include "AnthropicStreamParser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace llm::protocol::anthropic {

    AnthropicProtocolAdapter::AnthropicProtocolAdapter() = default;
    AnthropicProtocolAdapter::~AnthropicProtocolAdapter() = default;

    network::HttpRequest AnthropicProtocolAdapter::buildChatRequest(
        const domain::model::ModelProvider &provider,
        const domain::llm::ChatRequest &request) const {
        
        network::HttpRequest netReq;
        QString baseUrl = provider.baseUrl;
        if (baseUrl.endsWith('/')) baseUrl.chop(1);
        netReq.url = baseUrl + "/v1/messages";
        netReq.method = network::HttpMethod::Post;
        netReq.timeoutMs = provider.timeoutMs;

        netReq.headers.insert("Content-Type", "application/json");
        netReq.headers.insert("anthropic-version", "2023-06-01");
        if (!provider.apiKey.isEmpty()) {
            netReq.headers.insert("x-api-key", provider.apiKey);
        }
        for (auto it = provider.customHeaders.constBegin(); it != provider.customHeaders.constEnd(); ++it) {
            netReq.headers.insert(it.key(), it.value());
        }

        QJsonObject bodyObj;
        bodyObj.insert("model", request.model);
        
        // Anthropic 要求必须指定 max_tokens
        int maxTokens = request.maxTokens.value_or(4096);
        bodyObj.insert("max_tokens", maxTokens);

        if (request.stream.value_or(true)) {
            bodyObj.insert("stream", true);
        }
        if (request.temperature.has_value()) {
            bodyObj.insert("temperature", request.temperature.value());
        }
        if (request.useDeepThinking) {
            const QString effort = request.reasoningEffort;
            const int budget = effort == QStringLiteral("low") ? 1024
                : effort == QStringLiteral("high") ? 8192
                : effort == QStringLiteral("max") ? 16384 : 4096;
            bodyObj.insert("thinking", QJsonObject{{"type", "enabled"}, {"budget_tokens", budget}});
        }

        // 分离 system prompt 与普通消息
        QString systemPrompt;
        QJsonArray msgsArray;

        for (const auto &msg : request.messages) {
            if (msg.role == domain::MessageRole::System) {
                if (!systemPrompt.isEmpty()) systemPrompt += "\n\n";
                systemPrompt += msg.content;
            } else if (msg.role == domain::MessageRole::Tool) {
                QJsonObject msgObj;
                msgObj.insert("role", "user");
                QJsonArray contentArr;
                QJsonObject toolResObj;
                toolResObj.insert("type", "tool_result");
                toolResObj.insert("tool_use_id", msg.toolCallId);
                toolResObj.insert("content", msg.content);
                contentArr.append(toolResObj);
                msgObj.insert("content", contentArr);
                msgsArray.append(msgObj);
            } else if (msg.role == domain::MessageRole::Assistant && msg.toolCalls.has_value() && !msg.toolCalls->isEmpty()) {
                QJsonObject msgObj;
                msgObj.insert("role", "assistant");
                QJsonArray contentArr;
                if (!msg.content.isEmpty()) {
                    QJsonObject txtObj;
                    txtObj.insert("type", "text");
                    txtObj.insert("text", msg.content);
                    contentArr.append(txtObj);
                }
                for (const auto &tc : msg.toolCalls.value()) {
                    QJsonObject tuObj;
                    tuObj.insert("type", "tool_use");
                    tuObj.insert("id", tc.id);
                    tuObj.insert("name", tc.name);
                    QJsonDocument argsDoc = QJsonDocument::fromJson(tc.arguments.toUtf8());
                    tuObj.insert("input", argsDoc.isObject() ? argsDoc.object() : QJsonObject{});
                    contentArr.append(tuObj);
                }
                msgObj.insert("content", contentArr);
                msgsArray.append(msgObj);
            } else {
                QJsonObject msgObj;
                msgObj.insert("role", (msg.role == domain::MessageRole::Assistant) ? "assistant" : "user");
                msgObj.insert("content", msg.content);
                msgsArray.append(msgObj);
            }
        }

        if (!systemPrompt.isEmpty()) {
            bodyObj.insert("system", systemPrompt);
        }
        bodyObj.insert("messages", msgsArray);

        if (request.tools.has_value() && !request.tools->isEmpty()) {
            QJsonArray toolsArr;
            for (const auto &tool : request.tools.value()) {
                QJsonObject toolObj;
                toolObj.insert("name", tool.name);
                toolObj.insert("description", tool.description);
                toolObj.insert("input_schema", tool.parameters);
                toolsArr.append(toolObj);
            }
            bodyObj.insert("tools", toolsArr);
        }

        QJsonDocument doc(bodyObj);
        netReq.body = doc.toJson(QJsonDocument::Compact);

        return netReq;
    }

    std::unique_ptr<IStreamParser> AnthropicProtocolAdapter::createStreamParser() const {
        return std::make_unique<AnthropicStreamParser>();
    }

    domain::llm::ChatError AnthropicProtocolAdapter::parseError(int httpStatusCode, const QByteArray &responseBody) const {
        domain::llm::ChatError error;
        error.httpStatus = httpStatusCode;
        error.originalText = QString::fromUtf8(responseBody);

        switch (httpStatusCode) {
            case 400:
                error.category = domain::llm::ChatErrorCategory::Request;
                error.code = QStringLiteral("InvalidRequest");
                error.userMessage = QStringLiteral("请求参数或消息格式不合规。");
                break;
            case 401:
                error.category = domain::llm::ChatErrorCategory::Authentication;
                error.code = QStringLiteral("ApiKeyInvalid");
                error.userMessage = QStringLiteral("Anthropic API Key 无效或未授权。");
                error.suggestedAction = QStringLiteral("OpenSettings");
                break;
            case 403:
                error.category = domain::llm::ChatErrorCategory::Authorization;
                error.code = QStringLiteral("Forbidden");
                error.userMessage = QStringLiteral("无权访问该 Anthropic 资源或已被封禁。");
                break;
            case 404:
                error.category = domain::llm::ChatErrorCategory::Model;
                error.code = QStringLiteral("ModelNotFound");
                error.userMessage = QStringLiteral("指定的 Claude 模型不存在或不可用。");
                error.suggestedAction = QStringLiteral("ChangeModel");
                break;
            case 429:
                error.category = domain::llm::ChatErrorCategory::RateLimit;
                error.code = QStringLiteral("TooManyRequests");
                error.userMessage = QStringLiteral("请求频率超限，正在退避重试...");
                error.retryable = true;
                error.suggestedAction = QStringLiteral("Retry");
                break;
            default:
                if (httpStatusCode >= 500) {
                    error.category = domain::llm::ChatErrorCategory::Provider;
                    error.code = QStringLiteral("ServerError");
                    error.userMessage = QStringLiteral("Anthropic 服务端暂时过载或故障。");
                    error.retryable = true;
                    error.suggestedAction = QStringLiteral("Retry");
                } else if (httpStatusCode == 0) {
                    error.category = domain::llm::ChatErrorCategory::Network;
                    error.code = QStringLiteral("NetworkError");
                    error.userMessage = QStringLiteral("网络连接失败，请检查网络。");
                    error.retryable = true;
                } else {
                    error.category = domain::llm::ChatErrorCategory::Unknown;
                    error.code = QStringLiteral("Unknown");
                    error.userMessage = QStringLiteral("请求遇到未知错误 (HTTP %1)。").arg(httpStatusCode);
                }
                break;
        }

        // 解析 Anthropic 错误格式 {"type": "error", "error": {"type": "...", "message": "..."}}
        QJsonParseError parseErr;
        QJsonDocument doc = QJsonDocument::fromJson(responseBody, &parseErr);
        if (parseErr.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("error") && obj.value("error").isObject()) {
                QJsonObject errObj = obj.value("error").toObject();
                if (errObj.contains("message")) {
                    error.message = errObj.value("message").toString();
                }
                if (errObj.contains("type")) {
                    error.providerErrorCode = errObj.value("type").toString();
                    if (error.providerErrorCode == "overloaded_error") {
                        error.category = domain::llm::ChatErrorCategory::Provider;
                        error.code = QStringLiteral("Overloaded");
                        error.userMessage = QStringLiteral("Anthropic 服务器当前负载过高，稍后自动重试。");
                        error.retryable = true;
                    }
                }
                if (error.message.contains("prompt is too long", Qt::CaseInsensitive) ||
                    error.message.contains("maximum context length", Qt::CaseInsensitive)) {
                    error.category = domain::llm::ChatErrorCategory::Context;
                    error.code = QStringLiteral("ContextLengthExceeded");
                    error.userMessage = QStringLiteral("当前会话内容已超出 Claude 最大上下文长度。");
                    error.retryable = false;
                    error.suggestedAction = QStringLiteral("CompressContext");
                }
            }
        }

        if (error.message.isEmpty()) {
            error.message = QString("Anthropic HTTP %1 Error").arg(httpStatusCode);
        }

        return error;
    }

    network::HttpRequest AnthropicProtocolAdapter::buildListModelsRequest(const domain::model::ModelProvider &provider) const {
        network::HttpRequest netReq;
        QString baseUrl = provider.baseUrl.isEmpty() ? "https://api.anthropic.com" : provider.baseUrl;
        if (baseUrl.endsWith('/')) baseUrl.chop(1);
        netReq.url = baseUrl + "/v1/models";
        netReq.method = network::HttpMethod::Get;
        netReq.timeoutMs = provider.timeoutMs;

        netReq.headers.insert("anthropic-version", "2023-06-01");
        if (!provider.apiKey.isEmpty()) {
            netReq.headers.insert("x-api-key", provider.apiKey);
        }
        for (auto it = provider.customHeaders.constBegin(); it != provider.customHeaders.constEnd(); ++it) {
            netReq.headers.insert(it.key(), it.value());
        }

        return netReq;
    }

    QList<domain::model::ProviderModel> AnthropicProtocolAdapter::parseListModelsResponse(
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

} // namespace llm::protocol::anthropic
