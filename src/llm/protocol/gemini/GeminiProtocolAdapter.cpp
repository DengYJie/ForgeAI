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
                QJsonObject partObj;
                QJsonObject fnRes;
                fnRes.insert("name", msg.name.isEmpty() ? "tool" : msg.name);
                if (!msg.toolCallId.isEmpty()) {
                    fnRes.insert("id", msg.toolCallId);
                }
                QJsonObject resContent;
                QJsonDocument parsedRes = QJsonDocument::fromJson(msg.content.toUtf8());
                if (parsedRes.isObject()) {
                    resContent = parsedRes.object();
                } else {
                    resContent.insert("output", msg.content);
                }
                fnRes.insert("response", resContent);
                partObj.insert("functionResponse", fnRes);

                // Gemini: 如果上一条也是 functionResponse，合并到同一个 role: "function" / "user" turn
                if (!contentsArray.isEmpty() && contentsArray.last().toObject().value("role").toString() == "function") {
                    QJsonObject lastContent = contentsArray.last().toObject();
                    QJsonArray parts = lastContent.value("parts").toArray();
                    parts.append(partObj);
                    lastContent.insert("parts", parts);
                    contentsArray[contentsArray.size() - 1] = lastContent;
                } else {
                    QJsonObject contentObj;
                    contentObj.insert("role", "function");
                    QJsonArray partsArray;
                    partsArray.append(partObj);
                    contentObj.insert("parts", partsArray);
                    contentsArray.append(contentObj);
                }
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
                    if (!tc.id.isEmpty()) {
                        fnCall.insert("id", tc.id);
                    }
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
        if (request.useDeepThinking) {
            const QString effort = request.reasoningEffort;
            const int budget = effort == QStringLiteral("low") ? 1024
                : effort == QStringLiteral("high") ? 8192
                : effort == QStringLiteral("max") ? 16384 : 4096;
            genConfig.insert("thinkingConfig", QJsonObject{{"thinkingBudget", budget}});
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
        error.httpStatus = httpStatusCode;
        error.originalText = QString::fromUtf8(responseBody);

        switch (httpStatusCode) {
            case 400:
                error.category = domain::llm::ChatErrorCategory::Request;
                error.code = QStringLiteral("InvalidRequest");
                error.userMessage = QStringLiteral("Google Gemini 请求参数无效。");
                break;
            case 401:
            case 403:
                error.category = domain::llm::ChatErrorCategory::Authentication;
                error.code = QStringLiteral("ApiKeyInvalid");
                error.userMessage = QStringLiteral("Google Gemini API Key 无效或未启用 API。");
                error.suggestedAction = QStringLiteral("OpenSettings");
                break;
            case 404:
                error.category = domain::llm::ChatErrorCategory::Model;
                error.code = QStringLiteral("ModelNotFound");
                error.userMessage = QStringLiteral("指定的 Gemini 模型版本不存在。");
                error.suggestedAction = QStringLiteral("ChangeModel");
                break;
            case 429:
                error.category = domain::llm::ChatErrorCategory::RateLimit;
                error.code = QStringLiteral("TooManyRequests");
                error.userMessage = QStringLiteral("Gemini API 调用配额或频率超限，正在重试...");
                error.retryable = true;
                error.suggestedAction = QStringLiteral("Retry");
                break;
            default:
                if (httpStatusCode >= 500) {
                    error.category = domain::llm::ChatErrorCategory::Provider;
                    error.code = QStringLiteral("ServerError");
                    error.userMessage = QStringLiteral("Google Gemini 服务器内部错误。");
                    error.retryable = true;
                    error.suggestedAction = QStringLiteral("Retry");
                } else if (httpStatusCode == 0) {
                    error.category = domain::llm::ChatErrorCategory::Network;
                    error.code = QStringLiteral("NetworkError");
                    error.userMessage = QStringLiteral("网络连接失败，请检查网络或代理。");
                    error.retryable = true;
                } else {
                    error.category = domain::llm::ChatErrorCategory::Unknown;
                    error.code = QStringLiteral("Unknown");
                    error.userMessage = QStringLiteral("请求遇到未知错误 (HTTP %1)。").arg(httpStatusCode);
                }
                break;
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
                if (errObj.contains("status")) {
                    error.providerErrorCode = errObj.value("status").toString();
                    if (error.providerErrorCode == "RESOURCE_EXHAUSTED") {
                        error.category = domain::llm::ChatErrorCategory::Quota;
                        error.code = QStringLiteral("QuotaExceeded");
                        error.userMessage = QStringLiteral("Gemini 资源配额已耗尽。");
                        error.retryable = false;
                    }
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

    QList<domain::model::ProviderModel> GeminiProtocolAdapter::parseListModelsResponse(
        const QByteArray &responseBody,
        const QString &providerId) const {
        
        QList<domain::model::ProviderModel> models;
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
            
            domain::model::ProviderModel pm;
            pm.remoteModelId = id;
            pm.providerId = providerId;
            pm.isEnabled = true;
            pm.origin = domain::model::DataOrigin::User;
            models.append(pm);
        }

        return models;
    }

} // namespace llm::protocol::gemini
