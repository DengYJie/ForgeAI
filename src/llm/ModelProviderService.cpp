#include "ModelProviderService.h"
#include "network/HttpOperation.h"
#include "protocol/IStreamParser.h"
#include "protocol/IProtocolAdapter.h"

namespace llm {

    class ChatOperationImpl : public application::ports::IChatOperation {
        Q_OBJECT
    public:
        ChatOperationImpl(
            network::HttpOperation *httpOp,
            std::unique_ptr<protocol::IStreamParser> parser,
            std::shared_ptr<protocol::IProtocolAdapter> adapter,
            QObject *parent = nullptr)
            : application::ports::IChatOperation(parent)
            , m_httpOp(httpOp)
            , m_parser(std::move(parser))
            , m_adapter(adapter) {
            
            if (m_httpOp) {
                m_httpOp->setParent(this);
                connect(m_httpOp, &network::HttpOperation::dataReceived, this, &ChatOperationImpl::onDataReceived);
                connect(m_httpOp, &network::HttpOperation::finished, this, &ChatOperationImpl::onFinished);
                connect(m_httpOp, &network::HttpOperation::failed, this, &ChatOperationImpl::onFailed);
            }
        }

        void cancel() override {
            if (m_httpOp) {
                m_httpOp->cancel();
            }
        }

    private Q_SLOTS:
        void onDataReceived(const QByteArray &data) {
            if (!m_parser) return;
            auto events = m_parser->feed(data);
            for (const auto &evt : events) {
                emit eventReceived(evt);
            }
        }

        void onFinished() {
            if (m_parser) {
                auto events = m_parser->finish();
                for (const auto &evt : events) {
                    emit eventReceived(evt);
                }
            }
            // 确保总会抛出一个 Finished 事件，如果 parser 没有产生的话
            // 但最好依靠 parser 的正常流程返回
            emit eventReceived(domain::llm::EventFinished{"stop"});
        }

        void onFailed(const QString &errorMessage, int httpStatusCode) {
            domain::llm::ChatError err;
            if (m_adapter && httpStatusCode > 0) {
                // 有具体的 HTTP 错误，交给 adapter 解析
                err = m_adapter->parseError(httpStatusCode, errorMessage.toUtf8());
            } else {
                err.type = (errorMessage == "Cancelled") ? domain::llm::ChatErrorType::Cancelled : domain::llm::ChatErrorType::NetworkError;
                err.message = errorMessage;
            }
            emit eventReceived(domain::llm::EventError{err});
        }

    private:
        network::HttpOperation *m_httpOp;
        std::unique_ptr<protocol::IStreamParser> m_parser;
        std::shared_ptr<protocol::IProtocolAdapter> m_adapter;
    };

    // ==========================================

    ModelProviderService::ModelProviderService(
        std::shared_ptr<network::IHttpClient> httpClient,
        std::shared_ptr<ProtocolRegistry> registry)
        : m_httpClient(std::move(httpClient))
        , m_registry(std::move(registry)) {
    }

    ModelProviderService::~ModelProviderService() = default;

    application::ports::IChatOperation* ModelProviderService::sendRequest(
        const domain::model::ModelProvider &provider,
        const domain::llm::ChatRequest &request) {
        
        if (!m_registry || !m_httpClient) {
            return nullptr; // 依赖不完整
        }

        auto adapter = m_registry->adapter(provider.type);
        if (!adapter) {
            // 没有找到协议适配器
            auto op = new ChatOperationImpl(nullptr, nullptr, nullptr);
            domain::llm::ChatError err;
            err.type = domain::llm::ChatErrorType::ProtocolError;
            err.message = QString("Unsupported protocol type: %1").arg(static_cast<int>(provider.type));
            QMetaObject::invokeMethod(op, [op, err]() {
                emit op->eventReceived(domain::llm::EventError{err});
            }, Qt::QueuedConnection);
            return op;
        }

        network::HttpRequest netReq = adapter->buildChatRequest(provider, request);
        network::HttpOperation *netOp = m_httpClient->send(netReq);
        auto parser = adapter->createStreamParser();
        
        // 刚开始请求，发一个 Started 事件，UI 方便切换状态
        auto op = new ChatOperationImpl(netOp, std::move(parser), adapter);
        QMetaObject::invokeMethod(op, [op]() {
            emit op->eventReceived(domain::llm::EventStarted{});
        }, Qt::QueuedConnection);

        return op;
    }

} // namespace llm

#include "ModelProviderService.moc"
