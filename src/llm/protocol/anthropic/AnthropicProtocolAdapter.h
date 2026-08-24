#pragma once
#include "llm/protocol/IProtocolAdapter.h"

namespace llm::protocol::anthropic {

    /**
     * @brief Anthropic 原生 Messages 协议适配器
     */
    class AnthropicProtocolAdapter : public IProtocolAdapter {
    public:
        AnthropicProtocolAdapter();
        ~AnthropicProtocolAdapter() override;

        network::HttpRequest buildChatRequest(
            const domain::model::ModelProvider &provider,
            const domain::llm::ChatRequest &request) const override;

        std::unique_ptr<IStreamParser> createStreamParser() const override;

        domain::llm::ChatError parseError(
            int httpStatusCode,
            const QByteArray &responseBody) const override;

        network::HttpRequest buildListModelsRequest(
            const domain::model::ModelProvider &provider) const override;

        QList<domain::model::ProviderModel> parseListModelsResponse(
            const QByteArray &responseBody,
            const QString &providerId) const override;
    };

} // namespace llm::protocol::anthropic
