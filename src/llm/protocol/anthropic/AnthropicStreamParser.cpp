#include "AnthropicStreamParser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace llm::protocol::anthropic {

    AnthropicStreamParser::AnthropicStreamParser() = default;
    AnthropicStreamParser::~AnthropicStreamParser() = default;

    void AnthropicStreamParser::reset() {
        m_buffer.clear();
        m_currentEventType.clear();
        m_isFinished = false;
    }

    QList<domain::llm::ChatEvent> AnthropicStreamParser::feed(const QByteArray &chunk) {
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

    QList<domain::llm::ChatEvent> AnthropicStreamParser::finish() {
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

    QList<domain::llm::ChatEvent> AnthropicStreamParser::parseLine(const QByteArray &line) {
        QList<domain::llm::ChatEvent> events;

        if (line.startsWith("event: ")) {
            m_currentEventType = QString::fromUtf8(line.mid(7).trimmed());
            return events;
        }

        if (!line.startsWith("data: ")) {
            return events;
        }

        QByteArray data = line.mid(6).trimmed();
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

        if (type == "message_start") {
            events.append(domain::llm::EventStarted{});
            if (obj.contains("message") && obj.value("message").isObject()) {
                QJsonObject msgObj = obj.value("message").toObject();
                if (msgObj.contains("usage") && msgObj.value("usage").isObject()) {
                    QJsonObject usageObj = msgObj.value("usage").toObject();
                    domain::llm::TokenUsage usage;
                    usage.inputTokens = usageObj.value("input_tokens").toInt();
                    events.append(domain::llm::EventUsageUpdated{usage});
                }
            }
        } else if (type == "content_block_start") {
            if (obj.contains("content_block") && obj.value("content_block").isObject()) {
                QJsonObject cb = obj.value("content_block").toObject();
                if (cb.value("type").toString() == "tool_use") {
                    m_currentToolCallId = cb.value("id").toString();
                    QString name = cb.value("name").toString();
                    events.append(domain::llm::EventToolCallStarted{m_currentToolCallId, name});
                }
            }
        } else if (type == "content_block_delta") {
            if (obj.contains("delta") && obj.value("delta").isObject()) {
                QJsonObject delta = obj.value("delta").toObject();
                QString deltaType = delta.value("type").toString();
                if (deltaType == "text_delta" && delta.contains("text")) {
                    events.append(domain::llm::EventTextDelta{delta.value("text").toString()});
                } else if (deltaType == "thinking_delta" && delta.contains("thinking")) {
                    events.append(domain::llm::EventThinkingDelta{delta.value("thinking").toString()});
                } else if (deltaType == "input_json_delta" && delta.contains("partial_json")) {
                    events.append(domain::llm::EventToolCallDelta{m_currentToolCallId, delta.value("partial_json").toString()});
                }
            }
        } else if (type == "content_block_stop") {
            if (!m_currentToolCallId.isEmpty()) {
                events.append(domain::llm::EventToolCallFinished{m_currentToolCallId});
                m_currentToolCallId.clear();
            }
        } else if (type == "message_delta") {
            if (obj.contains("usage") && obj.value("usage").isObject()) {
                QJsonObject usageObj = obj.value("usage").toObject();
                domain::llm::TokenUsage usage;
                usage.outputTokens = usageObj.value("output_tokens").toInt();
                events.append(domain::llm::EventUsageUpdated{usage});
            }
            if (obj.contains("delta") && obj.value("delta").isObject()) {
                QJsonObject delta = obj.value("delta").toObject();
                if (delta.contains("stop_reason") && !delta.value("stop_reason").isNull()) {
                    events.append(domain::llm::EventFinished{delta.value("stop_reason").toString()});
                }
            }
        } else if (type == "message_stop") {
            events.append(domain::llm::EventFinished{"stop"});
        } else if (type == "error") {
            domain::llm::ChatError err;
            err.category = domain::llm::ChatErrorCategory::Provider;
            err.code = QStringLiteral("StreamError");
            if (obj.contains("error") && obj.value("error").isObject()) {
                QJsonObject errObj = obj.value("error").toObject();
                err.message = errObj.value("message").toString();
                err.userMessage = err.message;
            } else {
                err.message = "Anthropic API Stream Error";
                err.userMessage = QStringLiteral("Anthropic 流式响应中返回错误。");
            }
            events.append(domain::llm::EventError{err});
        }

        return events;
    }

} // namespace llm::protocol::anthropic
