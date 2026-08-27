#pragma once
#include <QString>
#include <QByteArray>
#include <QList>
#include "llm/protocol/IStreamParser.h"

namespace llm::protocol::openai_responses {

    /**
     * @brief OpenAI Responses API SSE 流解析器
     * @details 解析 /v1/responses 产生的事件流 (response.created, response.output_text.delta, response.completed 等)
     */
    class OpenAIResponsesStreamParser : public IStreamParser {
    public:
        OpenAIResponsesStreamParser();
        ~OpenAIResponsesStreamParser() override;

        QList<domain::llm::ChatEvent> feed(const QByteArray &chunk) override;
        QList<domain::llm::ChatEvent> finish() override;
        void reset() override;

    private:
        QList<domain::llm::ChatEvent> parseLine(const QByteArray &line);

        QByteArray m_buffer;
        QString m_currentEventType;
        QHash<QString, QString> m_itemIdToCallId;
        QString m_currentCallId;
        bool m_isFinished = false;
    };

} // namespace llm::protocol::openai_responses
