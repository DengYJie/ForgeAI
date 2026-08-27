#include "OllamaProtocolAdapter.h"
#include "OllamaStreamParser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace llm::protocol::ollama {

    OllamaProtocolAdapter::OllamaProtocolAdapter() = default;
    OllamaProtocolAdapter::~OllamaProtocolAdapter() = default;

    network::HttpRequest OllamaProtocolAdapter::buildChatRequest(
        const domain::model::ResolvedModel &model,
        const domain::llm::ChatRequest &request,
        const domain::llm::ResolvedChatOptions &options) const {
        
        const auto &provider = model.provider;
        network::HttpRequest netReq;
        QString baseUrl = provider.baseUrl.isEmpty() ? QStringLiteral("http://localhost:11434") : provider.baseUrl;
        if (baseUrl.endsWith('/')) baseUrl.chop(1);
        if (baseUrl.endsWith(QStringLiteral("/api"))) {
            netReq.url = baseUrl + "/chat";
        } else {
            netReq.url = baseUrl + "/api/chat";
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
            if (msg.role == domain::MessageRole::Assistant && !msg.reasoningContent.isEmpty()) {
                msgObj.insert("thinking", msg.reasoningContent);
            }
            if (msg.role == domain::MessageRole::Assistant && msg.toolCalls.has_value() && !msg.toolCalls->isEmpty()) {
                QJsonArray tcArr;
                for (const auto &tc : msg.toolCalls.value()) {
                    QJsonObject tcObj;
                    tcObj.insert("id", tc.id);
                    tcObj.insert("type", "function");
                    QJsonObject fObj;
                    fObj.insert("name", tc.name);
                    QJsonDocument argsDoc = QJsonDocument::fromJson(tc.arguments.toUtf8());
                    fObj.insert("arguments", argsDoc.isObject() ? argsDoc.object() : QJsonObject{});
                    tcObj.insert("function", fObj);
                    tcArr.append(tcObj);
                }
                msgObj.insert("tool_calls", tcArr);
            }
            msgsArray.append(msgObj);
        }
        bodyObj.insert("messages", msgsArray);
        if (options.thinkingEnabled) {
            if (!options.reasoningEffort.isEmpty()) {
                bodyObj.insert("think", options.reasoningEffort);
            } else {
                bodyObj.insert("think", true);
            }
        }

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

        // options
        QJsonObject optionsObj;
        if (options.temperature.has_value()) {
            optionsObj.insert("temperature", options.temperature.value());
        }
        if (options.maxOutputTokens.has_value()) {
            optionsObj.insert("num_predict", options.maxOutputTokens.value());
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
        error.httpStatus = httpStatusCode;
        error.originalText = QString::fromUtf8(responseBody);

        switch (httpStatusCode) {
            case 404:
                error.category = domain::llm::ChatErrorCategory::Model;
                error.code = QStringLiteral("ModelNotFound");
                error.userMessage = QStringLiteral("本地 Ollama 模型未找到，请检查模型名称或运行 ollama pull 拉取。");
                error.suggestedAction = QStringLiteral("ChangeModel");
                break;
            case 400:
                error.category = domain::llm::ChatErrorCategory::Request;
                error.code = QStringLiteral("InvalidRequest");
                error.userMessage = QStringLiteral("Ollama 请求参数不合法。");
                break;
            default:
                if (httpStatusCode >= 500) {
                    error.category = domain::llm::ChatErrorCategory::Provider;
                    error.code = QStringLiteral("ServerError");
                    error.userMessage = QStringLiteral("Ollama 服务端执行出错。");
                    error.retryable = true;
                    error.suggestedAction = QStringLiteral("Retry");
                } else if (httpStatusCode == 0) {
                    error.category = domain::llm::ChatErrorCategory::Network;
                    error.code = QStringLiteral("ConnectionRefused");
                    error.userMessage = QStringLiteral("无法连接到本地 Ollama 服务，请确认 Ollama 正在运行 (http://localhost:11434)。");
                    error.retryable = true;
                    error.suggestedAction = QStringLiteral("Retry");
                } else {
                    error.category = domain::llm::ChatErrorCategory::Unknown;
                    error.code = QStringLiteral("Unknown");
                    error.userMessage = QStringLiteral("Ollama 遇到未知错误 (HTTP %1)。").arg(httpStatusCode);
                }
                break;
        }

        // 解析 Ollama 错误 {"error": "model 'xxx' not found"}
        QJsonParseError parseErr;
        QJsonDocument doc = QJsonDocument::fromJson(responseBody, &parseErr);
        if (parseErr.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("error")) {
                error.message = obj.value("error").toString();
                if (error.message.contains("not found", Qt::CaseInsensitive)) {
                    error.category = domain::llm::ChatErrorCategory::Model;
                    error.code = QStringLiteral("ModelNotFound");
                    error.userMessage = QStringLiteral("模型未安装或未找到：%1").arg(error.message);
                }
            }
        }

        if (error.message.isEmpty()) {
            error.message = QString("Ollama HTTP %1 Error").arg(httpStatusCode);
        }

        return error;
    }

    network::HttpRequest OllamaProtocolAdapter::buildListModelsRequest(const domain::model::ModelProvider &provider) const {
        network::HttpRequest netReq;
        QString baseUrl = provider.baseUrl.isEmpty() ? QStringLiteral("http://localhost:11434") : provider.baseUrl;
        if (baseUrl.endsWith('/')) baseUrl.chop(1);
        if (baseUrl.endsWith(QStringLiteral("/api"))) {
            netReq.url = baseUrl + "/tags";
        } else {
            netReq.url = baseUrl + "/api/tags";
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

    QList<domain::model::ProviderModel> OllamaProtocolAdapter::parseListModelsResponse(
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
            QString name = mObj.value("name").toString();
            if (name.isEmpty()) {
                name = mObj.value("model").toString();
            }
            if (name.isEmpty()) continue;

            domain::model::ProviderModel pm;
            pm.remoteModelId = name;
            pm.providerId = providerId;
            pm.isEnabled = true;
            pm.origin = domain::model::DataOrigin::User;
            models.append(pm);
        }

        return models;
    }

} // namespace llm::protocol::ollama
