#pragma once
#include <QString>
#include <QByteArray>
#include <QList>
#include "llm/protocol/IStreamParser.h"

#include <QHash>

namespace llm::protocol::openai {

    struct ToolCallState {
        QString id;
        QString name;
        bool started = false;
    };

    /**
     * @brief OpenAI SSE 数据流解析器
     * @details 处理 `data: {...}` 格式。支持半包组装和粘包拆解。
     */
    class OpenAIStreamParser : public IStreamParser {
    public:
        OpenAIStreamParser();
        ~OpenAIStreamParser() override;

        QList<domain::llm::ChatEvent> feed(const QByteArray &chunk) override;
        QList<domain::llm::ChatEvent> finish() override;
        void reset() override;

    private:
        QList<domain::llm::ChatEvent> parseLine(const QByteArray &line);

        QByteArray m_buffer; // 处理跨 chunk 半包的缓冲区
        QHash<int, ToolCallState> m_toolCalls;
        bool m_hasFinished = false;
        bool m_isFinished = false;
    };

} // namespace llm::protocol::openai
