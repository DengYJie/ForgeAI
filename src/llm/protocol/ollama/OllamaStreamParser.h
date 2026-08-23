#pragma once
#include <QString>
#include <QByteArray>
#include <QList>
#include "llm/protocol/IStreamParser.h"

namespace llm::protocol::ollama {

    /**
     * @brief Ollama NDJSON / SSE 流解析器
     * @details 解析单行 JSON (message.content, message.thinking, done, prompt_eval_count, eval_count)
     */
    class OllamaStreamParser : public IStreamParser {
    public:
        OllamaStreamParser();
        ~OllamaStreamParser() override;

        QList<domain::llm::ChatEvent> feed(const QByteArray &chunk) override;
        QList<domain::llm::ChatEvent> finish() override;
        void reset() override;

    private:
        QList<domain::llm::ChatEvent> parseLine(const QByteArray &line);

        QByteArray m_buffer;
        bool m_isFinished = false;
    };

} // namespace llm::protocol::ollama
