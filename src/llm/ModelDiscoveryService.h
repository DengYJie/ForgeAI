#pragma once
#include <QObject>
#include <memory>
#include "application/ports/IModelDiscoveryGateway.h"
#include "network/IHttpClient.h"
#include "llm/ProtocolRegistry.h"

namespace llm {

    /**
     * @brief 模型发现服务，通过对应的协议适配器拉取远端 Provider 的可用模型列表
     */
    class ModelDiscoveryService : public application::ports::IModelDiscoveryGateway {
    public:
        ModelDiscoveryService(
            std::shared_ptr<network::IHttpClient> httpClient,
            std::shared_ptr<ProtocolRegistry> registry);
            
        ~ModelDiscoveryService() override;

        application::ports::IModelDiscoveryOperation* fetchModels(
            const domain::model::ModelProvider &provider) override;

    private:
        std::shared_ptr<network::IHttpClient> m_httpClient;
        std::shared_ptr<ProtocolRegistry> m_registry;
    };

} // namespace llm
