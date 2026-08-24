#include "TestProviderConnectionUseCase.h"

namespace application::usecase::settings {

    TestProviderConnectionUseCase::TestProviderConnectionUseCase(
        ports::IModelDiscoveryGateway *discoveryGateway,
        QObject *parent)
        : QObject(parent), m_discoveryGateway(discoveryGateway) {
    }

    TestProviderConnectionUseCase::~TestProviderConnectionUseCase() {
        cancel();
    }

    void TestProviderConnectionUseCase::execute(const domain::model::ModelProvider &provider) {
        if (provider.id.isEmpty() || !m_discoveryGateway) {
            emit testFailed(provider.id, tr("连接测试服务尚未就绪。"));
            return;
        }

        cancel();
        m_currentProviderId = provider.id;
        emit testStarted(m_currentProviderId);

        m_currentOp = m_discoveryGateway->fetchModels(provider);
        if (!m_currentOp) {
            const QString providerId = m_currentProviderId;
            m_currentProviderId.clear();
            emit testFailed(providerId, tr("无法启动连接测试。"));
            return;
        }

        m_currentOp->setParent(this);
        connect(m_currentOp, &ports::IModelDiscoveryOperation::finished,
                this, &TestProviderConnectionUseCase::onModelsFetched);
        connect(m_currentOp, &ports::IModelDiscoveryOperation::failed,
                this, &TestProviderConnectionUseCase::onFetchFailed);
    }

    void TestProviderConnectionUseCase::cancel() {
        if (m_currentOp) {
            m_currentOp->cancel();
            m_currentOp->deleteLater();
            m_currentOp = nullptr;
        }
        m_currentProviderId.clear();
    }

    void TestProviderConnectionUseCase::onModelsFetched(const QList<domain::model::ProviderModel> &models) {
        Q_UNUSED(models)
        if (m_currentProviderId.isEmpty()) return;

        const QString providerId = m_currentProviderId;
        if (m_currentOp) {
            m_currentOp->deleteLater();
            m_currentOp = nullptr;
        }
        m_currentProviderId.clear();
        emit testSucceeded(providerId);
    }

    void TestProviderConnectionUseCase::onFetchFailed(const QString &errorMessage) {
        if (m_currentProviderId.isEmpty()) return;

        const QString providerId = m_currentProviderId;
        if (m_currentOp) {
            m_currentOp->deleteLater();
            m_currentOp = nullptr;
        }
        m_currentProviderId.clear();
        emit testFailed(providerId, errorMessage);
    }

} // namespace application::usecase::settings
