#include "ModelProviderService.h"
#include "llm/runtime/ChatOperation.h"
#include "protocol/IProtocolAdapter.h"
#include <algorithm>

namespace llm {

    ModelProviderService::ModelProviderService(
        std::shared_ptr<network::IHttpClient> httpClient,
        std::shared_ptr<ProtocolRegistry> registry)
        : m_httpClient(std::move(httpClient))
        , m_registry(std::move(registry)) {
    }

    ModelProviderService::~ModelProviderService() = default;

    application::ports::IChatOperation* ModelProviderService::sendRequest(
        const domain::model::ResolvedModel &model,
        const domain::llm::ChatRequest &request) {
        
        if (!m_registry || !m_httpClient) {
            return nullptr;
        }

        const auto &provider = model.provider;
        auto adapter = m_registry->adapter(provider.protocol);
        if (!adapter) {
            auto op = new runtime::ChatOperation(m_httpClient, nullptr, model, request);
            domain::llm::ChatError err;
            err.category = domain::llm::ChatErrorCategory::Configuration;
            err.code = QStringLiteral("UnsupportedProtocol");
            err.message = QString("Unsupported protocol type: %1").arg(static_cast<int>(provider.protocol));
            err.userMessage = QStringLiteral("不支持的模型提供商协议。");
            QMetaObject::invokeMethod(op, [op, err]() {
                emit op->eventReceived(domain::llm::EventError{err});
            }, Qt::QueuedConnection);
            return op;
        }

        runtime::TimeoutPolicy timeoutPolicy;
        if (provider.timeoutMs > 0) {
            timeoutPolicy.connectTimeoutMs = std::min(15000, provider.timeoutMs);
            timeoutPolicy.firstTokenTimeoutMs = provider.timeoutMs;
        }

        runtime::RetryPolicy retryPolicy;

        auto op = new runtime::ChatOperation(m_httpClient, adapter, model, request, timeoutPolicy, retryPolicy);
        
        QMetaObject::invokeMethod(op, [op]() {
            emit op->eventReceived(domain::llm::EventStarted{});
            op->start();
        }, Qt::QueuedConnection);

        return op;
    }

} // namespace llm
