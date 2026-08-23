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

        // 分离 system prompt 与普通消息
        QString systemPrompt;
        QJsonArray msgsArray;

        for (const auto &msg : request.messages) {
            if (msg.role == domain::MessageRole::System) {
                if (!systemPrompt.isEmpty()) systemPrompt += "\n\n";
                systemPrompt += msg.content;
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

        QJsonDocument doc(bodyObj);
        netReq.body = doc.toJson(QJsonDocument::Compact);

        return netReq;
    }

    std::unique_ptr<IStreamParser> AnthropicProtocolAdapter::createStreamParser() const {
        return std::make_unique<AnthropicStreamParser>();
    }

    domain::llm::ChatError AnthropicProtocolAdapter::parseError(int httpStatusCode, const QByteArray &responseBody) const {
        domain::llm::ChatError error;
        error.originalText = QString::fromUtf8(responseBody);

        switch (httpStatusCode) {
            case 401: error.type = domain::llm::ChatErrorType::Unauthorized; break;
            case 403: error.type = domain::llm::ChatErrorType::Unauthorized; break;
            case 404: error.type = domain::llm::ChatErrorType::ModelNotFound; break;
            case 429: error.type = domain::llm::ChatErrorType::RateLimited; break;
            case 400: error.type = domain::llm::ChatErrorType::InvalidRequest; break;
            default:
                if (httpStatusCode >= 500) error.type = domain::llm::ChatErrorType::ServerError;
                else if (httpStatusCode == 0) error.type = domain::llm::ChatErrorType::NetworkError;
                else error.type = domain::llm::ChatErrorType::Unknown;
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
            }
        }

        if (error.message.isEmpty()) {
            error.message = QString("Anthropic HTTP %1 Error").arg(httpStatusCode);
        }

        return error;
    }

} // namespace llm::protocol::anthropic
