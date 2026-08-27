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

} // namespace llm::protocol::gemini
