#pragma once
#include <QMap>
#include <memory>
#include "domain/model/ModelProvider.h"
#include "protocol/IProtocolAdapter.h"

namespace llm {

    /**
     * @brief 协议适配器注册中心
     * @details 负责管理所有已实现的 LLM 协议，将领域类型映射到具体的 Adapter。
     */
    class ProtocolRegistry {
    public:
        ProtocolRegistry();
        ~ProtocolRegistry();

        /**
         * @brief 注册一个新的适配器工厂
         * @param type 协议类型
         * @param adapter 对应的适配器实例
         */
        void registerAdapter(domain::model::ProviderType type, std::shared_ptr<protocol::IProtocolAdapter> adapter);

        /**
         * @brief 获取指定协议的适配器
         * @param type 协议类型
         * @return 对应的适配器实例指针（若未注册则返回 nullptr）
         */
        std::shared_ptr<protocol::IProtocolAdapter> adapter(domain::model::ProviderType type) const;

    private:
        QMap<domain::model::ProviderType, std::shared_ptr<protocol::IProtocolAdapter>> m_adapters;
    };

} // namespace llm
