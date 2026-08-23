#include "ProtocolRegistry.h"

namespace llm {

    ProtocolRegistry::ProtocolRegistry() = default;
    ProtocolRegistry::~ProtocolRegistry() = default;

    void ProtocolRegistry::registerAdapter(domain::model::ProviderType type, std::shared_ptr<protocol::IProtocolAdapter> adapter) {
        if (adapter) {
            m_adapters.insert(type, std::move(adapter));
        }
    }

    std::shared_ptr<protocol::IProtocolAdapter> ProtocolRegistry::adapter(domain::model::ProviderType type) const {
        return m_adapters.value(type, nullptr);
    }

} // namespace llm
