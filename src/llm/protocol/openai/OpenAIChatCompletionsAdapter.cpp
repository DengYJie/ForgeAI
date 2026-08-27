#include "OpenAIChatCompletionsAdapter.h"
#include "OpenAIStreamParser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace llm::protocol::openai {

    OpenAIChatCompletionsAdapter::OpenAIChatCompletionsAdapter() = default;
    OpenAIChatCompletionsAdapter::~OpenAIChatCompletionsAdapter() = default;

    network::HttpRequest OpenAIChatCompletionsAdapter::buildChatRequest(
        const domain::model::ResolvedModel &model,
        const domain::llm::ChatRequest &request,
        const domain::llm::ResolvedChatOptions &options) const {
        
        const auto &provider = model.provider;
        network::HttpRequest netReq;
        // 拼接 endpoint
        QString baseUrl = provider.baseUrl;
        if (baseUrl.endsWith('/')) baseUrl.chop(1);
        if (baseUrl.endsWith(QStringLiteral("/v1"))) {
            netReq.url = baseUrl + "/chat/completions";
        } else {
            netReq.url = baseUrl + "/v1/chat/completions";
        }
        netReq.method = network::HttpMethod::Post;
        netReq.timeoutMs = provider.timeoutMs;

        // 设置 headers
        netReq.headers.insert("Content-Type", "application/json");
        if (!provider.apiKey.isEmpty()) {
            netReq.headers.insert("Authorization", "Bearer " + provider.apiKey);
        }
        for (auto it = provider.customHeaders.constBegin(); it != provider.customHeaders.constEnd(); ++it) {
            netReq.headers.insert(it.key(), it.value());
        }

        // 构建 JSON Body
        QJsonObject bodyObj;
        bodyObj.insert("model", request.model);
        
        QJsonArray msgsArray;
        for (const auto &msg : request.messages) {
            QJsonObject msgObj;
            switch (msg.role) {
                case domain::MessageRole::System: msgObj.insert("role", "system"); break;
                case domain::MessageRole::User: msgObj.insert("role", "user"); break;
                case domain::MessageRole::Assistant: msgObj.insert("role", "assistant"); break;
                case domain::MessageRole::Tool: msgObj.insert("role", "tool"); break;
            }
            msgObj.insert("content", msg.content);
            
            if (msg.role == domain::MessageRole::Tool && !msg.toolCallId.isEmpty()) {
                msgObj.insert("tool_call_id", msg.toolCallId);
            }
            if (msg.role == domain::MessageRole::Assistant && !msg.reasoningContent.isEmpty()) {
                msgObj.insert("reasoning_content", msg.reasoningContent);
            }
            if (msg.role == domain::MessageRole::Assistant && msg.toolCalls.has_value() && !msg.toolCalls->isEmpty()) {
                QJsonArray tcArr;
                for (const auto &tc : msg.toolCalls.value()) {
                    QJsonObject tcObj;
                    tcObj.insert("id", tc.id);
                    tcObj.insert("type", "function");
                    QJsonObject fObj;
                    fObj.insert("name", tc.name);
                    fObj.insert("arguments", tc.arguments);
                    tcObj.insert("function", fObj);
                    tcArr.append(tcObj);
                }
                msgObj.insert("tool_calls", tcArr);
            }
            
            msgsArray.append(msgObj);
        }
        bodyObj.insert("messages", msgsArray);

        if (options.toolsEnabled && request.tools.has_value() && !request.tools->isEmpty()) {
            QJsonArray toolsArr;
            for (const auto &tool : request.tools.value()) {
                QJsonObject toolObj;
                toolObj.insert("type", "function");
                QJsonObject funcObj;
                funcObj.insert("name", tool.name);
                funcObj.insert("description", tool.description);
                funcObj.insert("parameters", tool.parameters);
                toolObj.insert("function", funcObj);
                toolsArr.append(toolObj);
            }
            bodyObj.insert("tools", toolsArr);
        }

        if (request.stream.value_or(true)) {
            bodyObj.insert("stream", true);
        }
        if (options.temperature.has_value()) {
            bodyObj.insert("temperature", options.temperature.value());
        }
        if (options.maxOutputTokens.has_value()) {
            bodyObj.insert("max_tokens", options.maxOutputTokens.value());
        }
        if (!options.reasoningEffort.isEmpty() && options.reasoningEffort != QStringLiteral("none")) {
            bodyObj.insert("reasoning_effort", options.reasoningEffort);
        }

        QJsonDocument doc(bodyObj);
        netReq.body = doc.toJson(QJsonDocument::Compact);

        return netReq;
    }

    std::unique_ptr<IStreamParser> OpenAIChatCompletionsAdapter::createStreamParser() const {
        return std::make_unique<OpenAIStreamParser>();
    }

    domain::llm::ChatError OpenAIChatCompletionsAdapter::parseError(int httpStatusCode, const QByteArray &responseBody) const {
        domain::llm::ChatError error;
        error.httpStatus = httpStatusCode;
        error.originalText = QString::fromUtf8(responseBody);

        switch (httpStatusCode) {
            case 400:
                error.category = domain::llm::ChatErrorCategory::Request;
                error.code = QStringLiteral("InvalidRequest");
                error.userMessage = QStringLiteral("请求参数无效或不被支持。");
                break;
            case 401:
                error.category = domain::llm::ChatErrorCategory::Authentication;
                error.code = QStringLiteral("ApiKeyInvalid");
                error.userMessage = QStringLiteral("API Key 无效或缺失，请在设置中检查配置。");
                error.suggestedAction = QStringLiteral("OpenSettings");
                break;
            case 403:
                error.category = domain::llm::ChatErrorCategory::Authorization;
                error.code = QStringLiteral("Forbidden");
                error.userMessage = QStringLiteral("无权访问该模型或资源。");
                break;
            case 404:
                error.category = domain::llm::ChatErrorCategory::Model;
                error.code = QStringLiteral("ModelNotFound");
                error.userMessage = QStringLiteral("指定的模型不存在或已被下线。");
                error.suggestedAction = QStringLiteral("ChangeModel");
                break;
            case 429:
                error.category = domain::llm::ChatErrorCategory::RateLimit;
                error.code = QStringLiteral("TooManyRequests");
                error.userMessage = QStringLiteral("请求频率过高，正在尝试重试...");
                error.retryable = true;
                error.suggestedAction = QStringLiteral("Retry");
                break;
            default:
                if (httpStatusCode >= 500) {
                    error.category = domain::llm::ChatErrorCategory::Provider;
                    error.code = QStringLiteral("ServerError");
                    error.userMessage = QStringLiteral("大模型服务商内部故障或网关错误。");
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

        // 解析 OpenAI 风格的错误 {"error": {"message": "...", "type": "...", "code": "..."}}
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
                        error.userMessage = QStringLiteral("API 账户额度不足或已欠费，请充值后重试。");
                        error.retryable = false;
                        error.suggestedAction = QStringLiteral("OpenSettings");
                    }
                }
                
                if (error.providerErrorCode == "context_length_exceeded" ||
                    error.message.contains("context_length_exceeded", Qt::CaseInsensitive) ||
                    error.message.contains("maximum context length", Qt::CaseInsensitive)) {
                    error.category = domain::llm::ChatErrorCategory::Context;
                    error.code = QStringLiteral("ContextLengthExceeded");
                    error.userMessage = QStringLiteral("当前会话已超出模型最大上下文长度限制。");
                    error.retryable = false;
                    error.suggestedAction = QStringLiteral("CompressContext");
                }
            }
        }

        if (error.message.isEmpty()) {
            error.message = QString("HTTP %1 Error").arg(httpStatusCode);
        }

        return error;
    }

    network::HttpRequest OpenAIChatCompletionsAdapter::buildListModelsRequest(const domain::model::ModelProvider &provider) const {
        network::HttpRequest netReq;
        QString baseUrl = provider.baseUrl;
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

    QList<domain::model::ProviderModel> OpenAIChatCompletionsAdapter::parseListModelsResponse(
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

} // namespace llm::protocol::openai
