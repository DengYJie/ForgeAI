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
        bool supportsModelDiscovery() const override { return true; }

        network::HttpRequest buildChatRequest(
            const domain::model::ResolvedModel &model,
            const domain::llm::ChatRequest &request,
            const domain::llm::ResolvedChatOptions &options) const override;

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

} // namespace llm::protocol::ollama
