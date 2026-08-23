#include "OllamaProtocolAdapter.h"
#include "OllamaStreamParser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace llm::protocol::ollama {

    OllamaProtocolAdapter::OllamaProtocolAdapter() = default;
    OllamaProtocolAdapter::~OllamaProtocolAdapter() = default;

    network::HttpRequest OllamaProtocolAdapter::buildChatRequest(
        const domain::model::ModelProvider &provider,
        const domain::llm::ChatRequest &request) const {
        
        network::HttpRequest netReq;
        QString baseUrl = provider.baseUrl.isEmpty() ? "http://localhost:11434" : provider.baseUrl;
        if (baseUrl.endsWith('/')) baseUrl.chop(1);
        
        netReq.url = baseUrl + "/api/chat";
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
            msgsArray.append(msgObj);
        }
        bodyObj.insert("messages", msgsArray);

        // options
        QJsonObject optionsObj;
        if (request.temperature.has_value()) {
            optionsObj.insert("temperature", request.temperature.value());
        }
        if (request.maxTokens.has_value()) {
            optionsObj.insert("num_predict", request.maxTokens.value());
        }
        if (!optionsObj.isEmpty()) {
            bodyObj.insert("options", optionsObj);
        }

        QJsonDocument doc(bodyObj);
        netReq.body = doc.toJson(QJsonDocument::Compact);

        return netReq;
    }

    std::unique_ptr<IStreamParser> OllamaProtocolAdapter::createStreamParser() const {
        return std::make_unique<OllamaStreamParser>();
    }

    domain::llm::ChatError OllamaProtocolAdapter::parseError(int httpStatusCode, const QByteArray &responseBody) const {
        domain::llm::ChatError error;
        error.originalText = QString::fromUtf8(responseBody);

        switch (httpStatusCode) {
            case 404: error.type = domain::llm::ChatErrorType::ModelNotFound; break;
            case 401:
            case 403: error.type = domain::llm::ChatErrorType::Unauthorized; break;
            case 400: error.type = domain::llm::ChatErrorType::InvalidRequest; break;
            default:
                if (httpStatusCode >= 500) error.type = domain::llm::ChatErrorType::ServerError;
                else if (httpStatusCode == 0) error.type = domain::llm::ChatErrorType::NetworkError;
                else error.type = domain::llm::ChatErrorType::Unknown;
        }

        // 解析 Ollama 错误 {"error": "model 'xxx' not found"}
        QJsonParseError parseErr;
        QJsonDocument doc = QJsonDocument::fromJson(responseBody, &parseErr);
        if (parseErr.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("error")) {
                error.message = obj.value("error").toString();
            }
        }

        if (error.message.isEmpty()) {
            error.message = QString("Ollama HTTP %1 Error").arg(httpStatusCode);
        }

        return error;
    }

    network::HttpRequest OllamaProtocolAdapter::buildListModelsRequest(const domain::model::ModelProvider &provider) const {
        network::HttpRequest netReq;
        QString baseUrl = provider.baseUrl.isEmpty() ? "http://localhost:11434" : provider.baseUrl;
        if (baseUrl.endsWith('/')) baseUrl.chop(1);
        netReq.url = baseUrl + "/api/tags";
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

    QList<domain::model::Model> OllamaProtocolAdapter::parseListModelsResponse(
        const QByteArray &responseBody,
        const QString &providerId) const {
        
        QList<domain::model::Model> models;
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(responseBody, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            return models;
        }

        QJsonObject rootObj = doc.object();
        QJsonArray modelsArray = rootObj.value("models").toArray();
        for (const auto &val : modelsArray) {
            if (!val.isObject()) continue;
            QJsonObject mObj = val.toObject();
            QString name = mObj.value("name").toString();
            if (name.isEmpty()) {
                name = mObj.value("model").toString();
            }
            if (name.isEmpty()) continue;

            domain::model::Model model;
            model.id = name;
            model.providerId = providerId;
            model.displayName = name;
            model.openWeights = true;
            models.append(model);
        }

        return models;
    }

} // namespace llm::protocol::ollama
