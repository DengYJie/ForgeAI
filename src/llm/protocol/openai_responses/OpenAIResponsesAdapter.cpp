#include "OpenAIResponsesAdapter.h"
#include "OpenAIResponsesStreamParser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace llm::protocol::openai_responses {

    OpenAIResponsesAdapter::OpenAIResponsesAdapter() = default;
    OpenAIResponsesAdapter::~OpenAIResponsesAdapter() = default;

    network::HttpRequest OpenAIResponsesAdapter::buildChatRequest(
        const domain::model::ModelProvider &provider,
        const domain::llm::ChatRequest &request) const {
        
        network::HttpRequest netReq;
        QString baseUrl = provider.baseUrl.isEmpty() ? "https://api.openai.com" : provider.baseUrl;
        if (baseUrl.endsWith('/')) baseUrl.chop(1);
        
        netReq.url = baseUrl + "/v1/responses";
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
            QJsonObject msgObj;
            switch (msg.role) {
                case domain::MessageRole::System: msgObj.insert("role", "system"); break;
                case domain::MessageRole::User: msgObj.insert("role", "user"); break;
                case domain::MessageRole::Assistant: msgObj.insert("role", "assistant"); break;
                case domain::MessageRole::Tool: msgObj.insert("role", "tool"); break;
            }
            if (msg.role == domain::MessageRole::Tool && !msg.toolCallId.isEmpty()) {
                msgObj.insert("tool_call_id", msg.toolCallId);
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
            inputArray.append(msgObj);
        }
        bodyObj.insert("input", inputArray);

        if (request.tools.has_value() && !request.tools->isEmpty()) {
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

        if (request.temperature.has_value()) {
            bodyObj.insert("temperature", request.temperature.value());
        }
        if (request.maxTokens.has_value()) {
            bodyObj.insert("max_output_tokens", request.maxTokens.value());
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
        error.originalText = QString::fromUtf8(responseBody);

        switch (httpStatusCode) {
            case 401:
            case 403: error.type = domain::llm::ChatErrorType::Unauthorized; break;
            case 429: error.type = domain::llm::ChatErrorType::RateLimited; break;
            case 404: error.type = domain::llm::ChatErrorType::ModelNotFound; break;
            case 400: error.type = domain::llm::ChatErrorType::InvalidRequest; break;
            default:
                if (httpStatusCode >= 500) error.type = domain::llm::ChatErrorType::ServerError;
                else if (httpStatusCode == 0) error.type = domain::llm::ChatErrorType::NetworkError;
                else error.type = domain::llm::ChatErrorType::Unknown;
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
        netReq.url = baseUrl + "/v1/models";
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

    QList<domain::model::Model> OpenAIResponsesAdapter::parseListModelsResponse(
        const QByteArray &responseBody,
        const QString &providerId) const {
        
        QList<domain::model::Model> models;
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

            domain::model::Model model;
            model.id = id;
            model.providerId = providerId;
            model.displayName = id;
            models.append(model);
        }

        return models;
    }

} // namespace llm::protocol::openai_responses
