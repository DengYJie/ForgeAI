#pragma once
#include "llm/protocol/IProtocolAdapter.h"

namespace llm::protocol::gemini {

    /**
     * @brief Google Gemini 原生协议适配器
     */
    class GeminiProtocolAdapter : public IProtocolAdapter {
    public:
        GeminiProtocolAdapter();
        ~GeminiProtocolAdapter() override;

        network::HttpRequest buildChatRequest(
            const domain::model::ModelProvider &provider,
            const domain::llm::ChatRequest &request) const override;

        std::unique_ptr<IStreamParser> createStreamParser() const override;

        domain::llm::ChatError parseError(
            int httpStatusCode,
            const QByteArray &responseBody) const override;
    };

} // namespace llm::protocol::gemini
