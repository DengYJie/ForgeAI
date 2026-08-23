#include "GeminiProtocolAdapter.h"
#include "GeminiStreamParser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace llm::protocol::gemini {

    GeminiProtocolAdapter::GeminiProtocolAdapter() = default;
    GeminiProtocolAdapter::~GeminiProtocolAdapter() = default;

    network::HttpRequest GeminiProtocolAdapter::buildChatRequest(
        const domain::model::ModelProvider &provider,
        const domain::llm::ChatRequest &request) const {
        
        network::HttpRequest netReq;
        QString baseUrl = provider.baseUrl.isEmpty() ? "https://generativelanguage.googleapis.com" : provider.baseUrl;
        if (baseUrl.endsWith('/')) baseUrl.chop(1);
        
        netReq.url = QString("%1/v1beta/models/%2:streamGenerateContent?alt=sse").arg(baseUrl, request.model);
        netReq.method = network::HttpMethod::Post;
        netReq.timeoutMs = provider.timeoutMs;

        netReq.headers.insert("Content-Type", "application/json");
        if (!provider.apiKey.isEmpty()) {
            netReq.headers.insert("x-goog-api-key", provider.apiKey);
        }
        for (auto it = provider.customHeaders.constBegin(); it != provider.customHeaders.constEnd(); ++it) {
            netReq.headers.insert(it.key(), it.value());
        }

        QJsonObject bodyObj;

        // 分离 system instruction 与 contents
        QString systemPrompt;
        QJsonArray contentsArray;

        for (const auto &msg : request.messages) {
            if (msg.role == domain::MessageRole::System) {
                if (!systemPrompt.isEmpty()) systemPrompt += "\n\n";
                systemPrompt += msg.content;
            } else if (msg.role == domain::MessageRole::Tool) {
                QJsonObject contentObj;
                contentObj.insert("role", "user");
                QJsonArray partsArray;
                QJsonObject partObj;
                QJsonObject fnRes;
                fnRes.insert("name", msg.name.isEmpty() ? "tool" : msg.name);
                QJsonObject resContent;
                QJsonDocument parsedRes = QJsonDocument::fromJson(msg.content.toUtf8());
                if (parsedRes.isObject()) {
                    resContent = parsedRes.object();
                } else {
                    resContent.insert("output", msg.content);
                }
                fnRes.insert("response", resContent);
                partObj.insert("functionResponse", fnRes);
                partsArray.append(partObj);
                contentObj.insert("parts", partsArray);
                contentsArray.append(contentObj);
            } else if (msg.role == domain::MessageRole::Assistant && msg.toolCalls.has_value() && !msg.toolCalls->isEmpty()) {
                QJsonObject contentObj;
                contentObj.insert("role", "model");
                QJsonArray partsArray;
                if (!msg.content.isEmpty()) {
                    QJsonObject partObj;
                    partObj.insert("text", msg.content);
                    partsArray.append(partObj);
                }
                for (const auto &tc : msg.toolCalls.value()) {
                    QJsonObject partObj;
                    QJsonObject fnCall;
                    fnCall.insert("name", tc.name);
                    QJsonDocument argsDoc = QJsonDocument::fromJson(tc.arguments.toUtf8());
                    fnCall.insert("args", argsDoc.isObject() ? argsDoc.object() : QJsonObject{});
                    partObj.insert("functionCall", fnCall);
                    partsArray.append(partObj);
                }
                contentObj.insert("parts", partsArray);
                contentsArray.append(contentObj);
            } else {
                QJsonObject contentObj;
                contentObj.insert("role", (msg.role == domain::MessageRole::Assistant) ? "model" : "user");
                
                QJsonArray partsArray;
                QJsonObject partObj;
                partObj.insert("text", msg.content);
                partsArray.append(partObj);

                contentObj.insert("parts", partsArray);
                contentsArray.append(contentObj);
            }
        }

        if (!systemPrompt.isEmpty()) {
            QJsonObject sysObj;
            QJsonArray sysParts;
            QJsonObject sysPart;
            sysPart.insert("text", systemPrompt);
            sysParts.append(sysPart);
            sysObj.insert("parts", sysParts);
            bodyObj.insert("systemInstruction", sysObj);
        }

        bodyObj.insert("contents", contentsArray);

        if (request.tools.has_value() && !request.tools->isEmpty()) {
            QJsonArray toolsArr;
            QJsonObject toolItem;
            QJsonArray fnDecls;
            for (const auto &tool : request.tools.value()) {
                QJsonObject fnDecl;
                fnDecl.insert("name", tool.name);
                fnDecl.insert("description", tool.description);
                fnDecl.insert("parameters", tool.parameters);
                fnDecls.append(fnDecl);
            }
            toolItem.insert("functionDeclarations", fnDecls);
            toolsArr.append(toolItem);
            bodyObj.insert("tools", toolsArr);
        }

        // generationConfig
        QJsonObject genConfig;
        if (request.temperature.has_value()) {
            genConfig.insert("temperature", request.temperature.value());
        }
        if (request.maxTokens.has_value()) {
            genConfig.insert("maxOutputTokens", request.maxTokens.value());
        }
        if (!genConfig.isEmpty()) {
            bodyObj.insert("generationConfig", genConfig);
        }

        QJsonDocument doc(bodyObj);
        netReq.body = doc.toJson(QJsonDocument::Compact);

        return netReq;
    }

    std::unique_ptr<IStreamParser> GeminiProtocolAdapter::createStreamParser() const {
        return std::make_unique<GeminiStreamParser>();
    }

    domain::llm::ChatError GeminiProtocolAdapter::parseError(int httpStatusCode, const QByteArray &responseBody) const {
        domain::llm::ChatError error;
        error.originalText = QString::fromUtf8(responseBody);

        switch (httpStatusCode) {
            case 401:
            case 403: error.type = domain::llm::ChatErrorType::Unauthorized; break;
            case 404: error.type = domain::llm::ChatErrorType::ModelNotFound; break;
            case 429: error.type = domain::llm::ChatErrorType::RateLimited; break;
            case 400: error.type = domain::llm::ChatErrorType::InvalidRequest; break;
            default:
                if (httpStatusCode >= 500) error.type = domain::llm::ChatErrorType::ServerError;
                else if (httpStatusCode == 0) error.type = domain::llm::ChatErrorType::NetworkError;
                else error.type = domain::llm::ChatErrorType::Unknown;
        }

        // 解析 Gemini 错误 {"error": {"code": 400, "message": "...", "status": "..."}}
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
            error.message = QString("Google Gemini HTTP %1 Error").arg(httpStatusCode);
        }

        return error;
    }

    network::HttpRequest GeminiProtocolAdapter::buildListModelsRequest(const domain::model::ModelProvider &provider) const {
        network::HttpRequest netReq;
        QString baseUrl = provider.baseUrl.isEmpty() ? "https://generativelanguage.googleapis.com" : provider.baseUrl;
        if (baseUrl.endsWith('/')) baseUrl.chop(1);
        netReq.url = baseUrl + "/v1beta/models";
        netReq.method = network::HttpMethod::Get;
        netReq.timeoutMs = provider.timeoutMs;

        if (!provider.apiKey.isEmpty()) {
            netReq.headers.insert("x-goog-api-key", provider.apiKey);
        }
        for (auto it = provider.customHeaders.constBegin(); it != provider.customHeaders.constEnd(); ++it) {
            netReq.headers.insert(it.key(), it.value());
        }

        return netReq;
    }

    QList<domain::model::Model> GeminiProtocolAdapter::parseListModelsResponse(
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
            QString rawName = mObj.value("name").toString();
            if (rawName.isEmpty()) continue;

            // 去除 "models/" 前缀
            QString id = rawName.startsWith("models/") ? rawName.mid(7) : rawName;
            
            domain::model::Model model;
            model.id = id;
            model.providerId = providerId;
            model.displayName = mObj.value("displayName").toString(id);
            model.description = mObj.value("description").toString();
            models.append(model);
        }

        return models;
    }

} // namespace llm::protocol::gemini
