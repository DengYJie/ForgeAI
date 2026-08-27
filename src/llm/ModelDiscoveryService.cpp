#include "ModelDiscoveryService.h"
#include "network/HttpOperation.h"
#include "protocol/IProtocolAdapter.h"
#include "domain/model/ProviderModel.h"

namespace llm {

    class ModelDiscoveryOperationImpl : public application::ports::IModelDiscoveryOperation {
        Q_OBJECT
    public:
        ModelDiscoveryOperationImpl(
            network::HttpOperation *httpOp,
            std::shared_ptr<protocol::IProtocolAdapter> adapter,
            const QString &providerId,
            QObject *parent = nullptr)
            : application::ports::IModelDiscoveryOperation(parent)
            , m_httpOp(httpOp)
            , m_adapter(adapter)
            , m_providerId(providerId) {

            if (m_httpOp) {
                m_httpOp->setParent(this);
                connect(m_httpOp, &network::HttpOperation::dataReceived, this, &ModelDiscoveryOperationImpl::onDataReceived);
                connect(m_httpOp, &network::HttpOperation::finished, this, &ModelDiscoveryOperationImpl::onFinished);
                connect(m_httpOp, &network::HttpOperation::failed, this, &ModelDiscoveryOperationImpl::onFailed);
            }
        }

        void cancel() override {
            if (m_httpOp) {
                m_httpOp->cancel();
            }
        }

    private Q_SLOTS:
        void onDataReceived(const QByteArray &data) {
            m_body.append(data);
        }

        void onFinished() {
            if (!m_adapter) {
                emit failed("Adapter is invalid");
                return;
            }
            auto models = m_adapter->parseListModelsResponse(m_body, m_providerId);
            emit finished(models);
        }

        void onFailed(const QString &errorMessage, int httpStatusCode) {
            Q_UNUSED(httpStatusCode)
            emit failed(errorMessage);
        }

    private:
        network::HttpOperation *m_httpOp;
        std::shared_ptr<protocol::IProtocolAdapter> m_adapter;
        QString m_providerId;
        QByteArray m_body;
    };

    // ==========================================

    ModelDiscoveryService::ModelDiscoveryService(
        std::shared_ptr<network::IHttpClient> httpClient,
        std::shared_ptr<ProtocolRegistry> registry)
        : m_httpClient(std::move(httpClient))
        , m_registry(std::move(registry)) {
    }

    ModelDiscoveryService::~ModelDiscoveryService() = default;

    application::ports::IModelDiscoveryOperation* ModelDiscoveryService::fetchModels(
        const domain::model::ModelProvider &provider) {
        
        if (!m_registry || !m_httpClient) {
            return nullptr;
        }

        auto adapter = m_registry->adapter(provider.protocol);
        if (!adapter || !adapter->supportsModelDiscovery()) {
            auto op = new ModelDiscoveryOperationImpl(nullptr, nullptr, provider.id);
            QMetaObject::invokeMethod(op, [op]() {
                emit op->failed("Provider does not support model discovery");
            }, Qt::QueuedConnection);
            return op;
        }

        network::HttpRequest netReq = adapter->buildListModelsRequest(provider);
        network::HttpOperation *netOp = m_httpClient->send(netReq);

        return new ModelDiscoveryOperationImpl(netOp, adapter, provider.id);
    }

} // namespace llm

#include "ModelDiscoveryService.moc"
