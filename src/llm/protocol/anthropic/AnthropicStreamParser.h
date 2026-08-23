#pragma once
#include <QString>
#include <QByteArray>
#include <QList>
#include "llm/protocol/IStreamParser.h"

namespace llm::protocol::anthropic {

    /**
     * @brief Anthropic Messages SSE 流解析器
     * @details 解析 message_start, content_block_start, content_block_delta, content_block_stop, message_delta, message_stop
     */
    class AnthropicStreamParser : public IStreamParser {
    public:
        AnthropicStreamParser();
        ~AnthropicStreamParser() override;

        QList<domain::llm::ChatEvent> feed(const QByteArray &chunk) override;
        QList<domain::llm::ChatEvent> finish() override;
        void reset() override;

    private:
        QList<domain::llm::ChatEvent> parseLine(const QByteArray &line);

        QByteArray m_buffer;
        QString m_currentEventType;
        bool m_isFinished = false;
    };

} // namespace llm::protocol::anthropic
