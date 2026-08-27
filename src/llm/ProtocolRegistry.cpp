#include "ProtocolRegistry.h"

namespace llm {

    ProtocolRegistry::ProtocolRegistry() = default;
    ProtocolRegistry::~ProtocolRegistry() = default;

    void ProtocolRegistry::registerAdapter(domain::model::ProtocolType type, std::shared_ptr<protocol::IProtocolAdapter> adapter) {
        if (adapter) {
            m_adapters.insert(type, std::move(adapter));
        }
    }

    std::shared_ptr<protocol::IProtocolAdapter> ProtocolRegistry::adapter(domain::model::ProtocolType type) const {
        return m_adapters.value(type, nullptr);
    }

} // namespace llm
