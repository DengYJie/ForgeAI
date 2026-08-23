#include "OpenAIResponsesStreamParser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace llm::protocol::openai_responses {

    OpenAIResponsesStreamParser::OpenAIResponsesStreamParser() = default;
    OpenAIResponsesStreamParser::~OpenAIResponsesStreamParser() = default;

    void OpenAIResponsesStreamParser::reset() {
        m_buffer.clear();
        m_currentEventType.clear();
        m_isFinished = false;
    }

    QList<domain::llm::ChatEvent> OpenAIResponsesStreamParser::feed(const QByteArray &chunk) {
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

    QList<domain::llm::ChatEvent> OpenAIResponsesStreamParser::finish() {
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

    QList<domain::llm::ChatEvent> OpenAIResponsesStreamParser::parseLine(const QByteArray &line) {
        QList<domain::llm::ChatEvent> events;

        if (line.startsWith("event: ")) {
            m_currentEventType = QString::fromUtf8(line.mid(7).trimmed());
            return events;
        }

        if (!line.startsWith("data: ")) {
            return events;
        }

        QByteArray data = line.mid(6).trimmed();
        if (data == "[DONE]") {
            events.append(domain::llm::EventFinished{"stop"});
            return events;
        }

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            return events;
        }

        QJsonObject obj = doc.object();
        QString type = obj.value("type").toString();
        if (type.isEmpty()) {
            type = m_currentEventType;
        }

        if (type == "response.created") {
            events.append(domain::llm::EventStarted{});
        } else if (type == "response.output_text.delta" || type == "response.text.delta") {
            if (obj.contains("delta") && !obj.value("delta").isNull()) {
                events.append(domain::llm::EventTextDelta{obj.value("delta").toString()});
            }
        } else if (type == "response.reasoning_text.delta" || type == "response.thought.delta") {
            if (obj.contains("delta") && !obj.value("delta").isNull()) {
                events.append(domain::llm::EventThinkingDelta{obj.value("delta").toString()});
            }
        } else if (type == "response.completed") {
            if (obj.contains("response") && obj.value("response").isObject()) {
                QJsonObject respObj = obj.value("response").toObject();
                if (respObj.contains("usage") && respObj.value("usage").isObject()) {
                    QJsonObject usageObj = respObj.value("usage").toObject();
                    domain::llm::TokenUsage usage;
                    usage.inputTokens = usageObj.value("input_tokens").toInt();
                    usage.outputTokens = usageObj.value("output_tokens").toInt();
                    usage.totalTokens = usageObj.value("total_tokens").toInt();
                    events.append(domain::llm::EventUsageUpdated{usage});
                }
            }
            events.append(domain::llm::EventFinished{"stop"});
        } else if (type == "error") {
            domain::llm::ChatError err;
            err.type = domain::llm::ChatErrorType::ServerError;
            if (obj.contains("error") && obj.value("error").isObject()) {
                QJsonObject errObj = obj.value("error").toObject();
                err.message = errObj.value("message").toString();
            } else {
                err.message = "OpenAI Responses API Error";
            }
            events.append(domain::llm::EventError{err});
        }

        return events;
    }

} // namespace llm::protocol::openai_responses
