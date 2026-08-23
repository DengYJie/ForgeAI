#pragma once
#include <QString>
#include <QByteArray>
#include <QList>
#include "llm/protocol/IStreamParser.h"

namespace llm::protocol::gemini {

    /**
     * @brief Google Gemini SSE 流解析器
     * @details 解析 candidates.content.parts (text / thought), finishReason, usageMetadata
     */
    class GeminiStreamParser : public IStreamParser {
    public:
        GeminiStreamParser();
        ~GeminiStreamParser() override;

        QList<domain::llm::ChatEvent> feed(const QByteArray &chunk) override;
        QList<domain::llm::ChatEvent> finish() override;
        void reset() override;

    private:
        QList<domain::llm::ChatEvent> parseLine(const QByteArray &line);

        QByteArray m_buffer;
        bool m_isFinished = false;
    };

} // namespace llm::protocol::gemini
