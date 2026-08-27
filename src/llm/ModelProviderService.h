#pragma once
#include <QObject>
#include <memory>
#include "application/ports/IChatModelGateway.h"
#include "network/IHttpClient.h"
#include "llm/ProtocolRegistry.h"
#include "domain/model/ResolvedModel.h"

namespace llm {

    /**
     * @brief 模型提供商服务，负责将业务层的统一下发分流到对应的协议 Adapter 和 HttpClient
     */
    class ModelProviderService : public application::ports::IChatModelGateway {
    public:
        ModelProviderService(
            std::shared_ptr<network::IHttpClient> httpClient,
            std::shared_ptr<ProtocolRegistry> registry);
            
        ~ModelProviderService() override;

        application::ports::IChatOperation* sendRequest(
            const domain::model::ResolvedModel &model,
            const domain::llm::ChatRequest &request) override;

    private:
        std::shared_ptr<network::IHttpClient> m_httpClient;
        std::shared_ptr<ProtocolRegistry> m_registry;
    };

} // namespace llm
