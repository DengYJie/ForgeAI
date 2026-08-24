#pragma once
#include "llm/protocol/IProtocolAdapter.h"

namespace llm::protocol::openai_responses {

    /**
     * @brief OpenAI Responses API 协议适配器
     */
    class OpenAIResponsesAdapter : public IProtocolAdapter {
    public:
        OpenAIResponsesAdapter();
        ~OpenAIResponsesAdapter() override;
        bool supportsModelDiscovery() const override { return true; }

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

} // namespace llm::protocol::openai_responses
