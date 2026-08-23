#pragma once
#include "llm/protocol/IProtocolAdapter.h"

namespace llm::protocol::openai {

    /**
     * @brief OpenAI Chat Completions 协议适配器
     */
    class OpenAIChatCompletionsAdapter : public IProtocolAdapter {
    public:
        OpenAIChatCompletionsAdapter();
        ~OpenAIChatCompletionsAdapter() override;

        network::HttpRequest buildChatRequest(
            const domain::model::ModelProvider &provider,
            const domain::llm::ChatRequest &request) const override;

        std::unique_ptr<IStreamParser> createStreamParser() const override;

        domain::llm::ChatError parseError(
            int httpStatusCode,
            const QByteArray &responseBody) const override;

        network::HttpRequest buildListModelsRequest(
            const domain::model::ModelProvider &provider) const override;

        QList<domain::model::Model> parseListModelsResponse(
            const QByteArray &responseBody,
            const QString &providerId) const override;
    };

} // namespace llm::protocol::openai
