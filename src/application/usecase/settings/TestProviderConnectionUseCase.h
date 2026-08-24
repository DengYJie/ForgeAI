#pragma once

#include <QObject>
#include <QString>

#include "application/ports/IModelDiscoveryGateway.h"

namespace application::usecase::settings {

    /**
     * @brief Verifies a provider configuration without persisting it.
     *
     * Model discovery is used as the protocol-agnostic authenticated request.
     */
    class TestProviderConnectionUseCase : public QObject {
        Q_OBJECT

    public:
        explicit TestProviderConnectionUseCase(
            ports::IModelDiscoveryGateway *discoveryGateway,
            QObject *parent = nullptr);
        ~TestProviderConnectionUseCase() override;

        void execute(const domain::model::ModelProvider &provider);
        void cancel();

    Q_SIGNALS:
        void testStarted(const QString &providerId);
        void testSucceeded(const QString &providerId);
        void testFailed(const QString &providerId, const QString &errorMessage);

    private Q_SLOTS:
        void onModelsFetched(const QList<domain::model::ProviderModel> &models);
        void onFetchFailed(const QString &errorMessage);

    private:
        ports::IModelDiscoveryGateway *m_discoveryGateway = nullptr;
        ports::IModelDiscoveryOperation *m_currentOp = nullptr;
        QString m_currentProviderId;
    };

} // namespace application::usecase::settings
