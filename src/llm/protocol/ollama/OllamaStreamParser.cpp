#include "OllamaStreamParser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace llm::protocol::ollama {

    OllamaStreamParser::OllamaStreamParser() = default;
    OllamaStreamParser::~OllamaStreamParser() = default;

    void OllamaStreamParser::reset() {
        m_buffer.clear();
        m_isFinished = false;
    }

    QList<domain::llm::ChatEvent> OllamaStreamParser::feed(const QByteArray &chunk) {
        QList<domain::llm::ChatEvent> events;
        if (m_isFinished || chunk.isEmpty()) return events;

        m_buffer.append(chunk);

        while (true) {
            int newlineIndex = m_buffer.indexOf('\n');
            if (newlineIndex == -1) {
                break;
            }

            QByteArray line = m_buffer.left(newlineIndex).trimmed();
            m_buffer.remove(0, newlineIndex + 1);

            if (!line.isEmpty()) {
                events.append(parseLine(line));
            }
        }

        return events;
    }

    QList<domain::llm::ChatEvent> OllamaStreamParser::finish() {
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

    QList<domain::llm::ChatEvent> OllamaStreamParser::parseLine(const QByteArray &line) {
        QList<domain::llm::ChatEvent> events;

        QByteArray data = line;
        // 兼容带 data: 前缀的 SSE 网关代理
        if (data.startsWith("data: ")) {
            data = data.mid(6).trimmed();
        }

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            return events;
        }

        QJsonObject obj = doc.object();

        // 1. 错误处理
        if (obj.contains("error") && !obj.value("error").isNull()) {
            domain::llm::ChatError err;
            err.type = domain::llm::ChatErrorType::ServerError;
            err.message = obj.value("error").toString();
            events.append(domain::llm::EventError{err});
            return events;
        }

        // 2. 解析 message (content / thinking)
        if (obj.contains("message") && obj.value("message").isObject()) {
            QJsonObject msgObj = obj.value("message").toObject();
            
            // 思考字段 (DeepSeek-R1 / Qwen / Ollama thinking)
            if (msgObj.contains("thinking") && !msgObj.value("thinking").isNull()) {
                QString thinking = msgObj.value("thinking").toString();
                if (!thinking.isEmpty()) {
                    events.append(domain::llm::EventThinkingDelta{thinking});
                }
            }

            if (msgObj.contains("content") && !msgObj.value("content").isNull()) {
                QString content = msgObj.value("content").toString();
                if (!content.isEmpty()) {
                    events.append(domain::llm::EventTextDelta{content});
                }
            }
        }

        // 3. 解析 done
        bool isDone = obj.value("done").toBool(false);
        if (isDone) {
            // Usage
            int promptTokens = obj.value("prompt_eval_count").toInt(0);
            int evalTokens = obj.value("eval_count").toInt(0);
            if (promptTokens > 0 || evalTokens > 0) {
                domain::llm::TokenUsage usage;
                usage.inputTokens = promptTokens;
                usage.outputTokens = evalTokens;
                usage.totalTokens = promptTokens + evalTokens;
                events.append(domain::llm::EventUsageUpdated{usage});
            }

            QString doneReason = obj.value("done_reason").toString("stop");
            events.append(domain::llm::EventFinished{doneReason});
        }

        return events;
    }

} // namespace llm::protocol::ollama
