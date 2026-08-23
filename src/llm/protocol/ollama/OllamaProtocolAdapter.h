#pragma once
#include "llm/protocol/IProtocolAdapter.h"

namespace llm::protocol::ollama {

    /**
     * @brief Ollama 原生 `/api/chat` 协议适配器
     */
    class OllamaProtocolAdapter : public IProtocolAdapter {
    public:
        OllamaProtocolAdapter();
        ~OllamaProtocolAdapter() override;

        network::HttpRequest buildChatRequest(
            const domain::model::ModelProvider &provider,
            const domain::llm::ChatRequest &request) const override;

        std::unique_ptr<IStreamParser> createStreamParser() const override;

        domain::llm::ChatError parseError(
            int httpStatusCode,
            const QByteArray &responseBody) const override;
    };

} // namespace llm::protocol::ollama
