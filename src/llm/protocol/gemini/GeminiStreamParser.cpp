#include "GeminiStreamParser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

namespace llm::protocol::gemini {

    GeminiStreamParser::GeminiStreamParser() = default;
    GeminiStreamParser::~GeminiStreamParser() = default;

    void GeminiStreamParser::reset() {
        m_buffer.clear();
        m_isFinished = false;
    }

    QList<domain::llm::ChatEvent> GeminiStreamParser::feed(const QByteArray &chunk) {
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

    QList<domain::llm::ChatEvent> GeminiStreamParser::finish() {
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

    QList<domain::llm::ChatEvent> GeminiStreamParser::parseLine(const QByteArray &line) {
        QList<domain::llm::ChatEvent> events;

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

        // 1. 解析 usageMetadata
        if (obj.contains("usageMetadata") && obj.value("usageMetadata").isObject()) {
            QJsonObject usageObj = obj.value("usageMetadata").toObject();
            domain::llm::TokenUsage usage;
            usage.inputTokens = usageObj.value("promptTokenCount").toInt();
            usage.outputTokens = usageObj.value("candidatesTokenCount").toInt();
            usage.totalTokens = usageObj.value("totalTokenCount").toInt();
            events.append(domain::llm::EventUsageUpdated{usage});
        }

        // 2. 解析 candidates
        QJsonArray candidates = obj.value("candidates").toArray();
        if (candidates.isEmpty()) {
            return events;
        }

        QJsonObject candidate = candidates.first().toObject();
        if (candidate.contains("content") && candidate.value("content").isObject()) {
            QJsonObject content = candidate.value("content").toObject();
            QJsonArray parts = content.value("parts").toArray();
            for (const auto &partVal : parts) {
                if (!partVal.isObject()) continue;
                QJsonObject part = partVal.toObject();

                // 判断是否是 thinking/thought
                if (part.contains("thought") && part.value("thought").isBool() && part.value("thought").toBool()) {
                    events.append(domain::llm::EventThinkingDelta{part.value("text").toString()});
                } else if (part.contains("text")) {
                    events.append(domain::llm::EventTextDelta{part.value("text").toString()});
                } else if (part.contains("functionCall") && part.value("functionCall").isObject()) {
                    QJsonObject fnCall = part.value("functionCall").toObject();
                    QString name = fnCall.value("name").toString();
                    QJsonObject args = fnCall.value("args").toObject();
                    QJsonDocument argsDoc(args);
                    QString argsStr = QString::fromUtf8(argsDoc.toJson(QJsonDocument::Compact));
                    QString callId = "gemini_call_" + name;
                    events.append(domain::llm::EventToolCallStarted{callId, name});
                    events.append(domain::llm::EventToolCallDelta{callId, argsStr});
                    events.append(domain::llm::EventToolCallFinished{callId});
                }
            }
        }

        if (candidate.contains("finishReason") && !candidate.value("finishReason").isNull()) {
            QString finishReason = candidate.value("finishReason").toString();
            if (!finishReason.isEmpty()) {
                events.append(domain::llm::EventFinished{finishReason});
            }
        }

        return events;
    }

} // namespace llm::protocol::gemini
