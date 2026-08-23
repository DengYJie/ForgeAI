#include "OpenAIStreamParser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

namespace llm::protocol::openai {

    OpenAIStreamParser::OpenAIStreamParser() = default;
    OpenAIStreamParser::~OpenAIStreamParser() = default;

    void OpenAIStreamParser::reset() {
        m_buffer.clear();
        m_isFinished = false;
    }

    QList<domain::llm::ChatEvent> OpenAIStreamParser::feed(const QByteArray &chunk) {
        QList<domain::llm::ChatEvent> events;
        if (m_isFinished || chunk.isEmpty()) return events;

        m_buffer.append(chunk);

        // Server-Sent Events (SSE) 总是以 \n\n 结尾，但也可能单行发 \n
        while (true) {
            int newlineIndex = m_buffer.indexOf('\n');
            if (newlineIndex == -1) {
                break; // 没有完整的一行，等待下一个 chunk
            }

            QByteArray line = m_buffer.left(newlineIndex).trimmed();
            m_buffer.remove(0, newlineIndex + 1); // 移除已读行和 \n

            if (!line.isEmpty()) {
                events.append(parseLine(line));
            }
        }

        return events;
    }

    QList<domain::llm::ChatEvent> OpenAIStreamParser::finish() {
        QList<domain::llm::ChatEvent> events;
        if (m_isFinished) return events;

        m_isFinished = true;
        
        QByteArray remaining = m_buffer.trimmed();
        m_buffer.clear();
        
        if (!remaining.isEmpty()) {
            events.append(parseLine(remaining));
        }

        return events;
    }

    QList<domain::llm::ChatEvent> OpenAIStreamParser::parseLine(const QByteArray &line) {
        QList<domain::llm::ChatEvent> events;

        if (!line.startsWith("data: ")) {
            return events; // 忽略非 data 行 (例如心跳或注释)
        }

        QByteArray data = line.mid(6).trimmed();
        if (data == "[DONE]") {
            // OpenAI 约定的正常结束标记
            events.append(domain::llm::EventFinished{"stop"});
            return events;
        }

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            // 考虑容错，可能是因为流格式损坏
            return events; 
        }

        QJsonObject obj = doc.object();
        QJsonArray choices = obj.value("choices").toArray();
        if (choices.isEmpty()) {
            // 某些实现 (如 DeepSeek/Ollama) 可能首先返回一个没有 choice 的 chunk 表示 Started
            // 或者是单纯的 Usage chunk
            if (obj.contains("usage") && !obj.value("usage").isNull()) {
                QJsonObject usageObj = obj.value("usage").toObject();
                domain::llm::TokenUsage usage;
                usage.inputTokens = usageObj.value("prompt_tokens").toInt();
                usage.outputTokens = usageObj.value("completion_tokens").toInt();
                usage.totalTokens = usageObj.value("total_tokens").toInt();
                events.append(domain::llm::EventUsageUpdated{usage});
            }
            return events;
        }

        QJsonObject choice = choices.first().toObject();
        QJsonObject delta = choice.value("delta").toObject();

        if (delta.contains("content") && !delta.value("content").isNull()) {
            events.append(domain::llm::EventTextDelta{delta.value("content").toString()});
        }

        if (choice.contains("finish_reason") && !choice.value("finish_reason").isNull()) {
            events.append(domain::llm::EventFinished{choice.value("finish_reason").toString()});
        }

        // tool_calls / usage 处理可以在此处进一步扩充

        return events;
    }

} // namespace llm::protocol::openai
