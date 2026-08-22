#include "PromptBuilder.h"
#include <algorithm>
#include <QStringList>

namespace core::context {

    PromptBuilder &PromptBuilder::addSection(const QString &tag, const QString &content, int priority) {
        return addSection(tag, content, {}, priority);
    }

    PromptBuilder &PromptBuilder::addSection(const QString &tag, const QString &content, const QMap<QString, QString> &attributes, int priority) {
        if (content.trimmed().isEmpty()) {
            return *this; // 忽略空内容段落
        }
        m_sections.append(PromptSection{tag, content.trimmed(), attributes, priority});
        return *this;
    }

    void PromptBuilder::clear() {
        m_sections.clear();
    }

    QString PromptBuilder::wrapTag(const QString &tag, const QString &content, const QMap<QString, QString> &attributes) {
        if (tag.isEmpty() || content.trimmed().isEmpty()) {
            return content;
        }

        QString attrStr;
        if (!attributes.isEmpty()) {
            QStringList attrList;
            for (auto it = attributes.begin(); it != attributes.end(); ++it) {
                attrList.append(QString("%1=\"%2\"").arg(it.key(), it.value()));
            }
            attrStr = " " + attrList.join(" ");
        }

        return QString("<%1%2>\n%3\n</%1>").arg(tag, attrStr, content.trimmed());
    }

    QString PromptBuilder::build() const {
        if (m_sections.isEmpty()) {
            return QString();
        }

        // 按 priority 升序做稳定排序，保证静态人设在前、动态内容在后，完美对齐 Prompt Caching
        QList<PromptSection> sortedSections = m_sections;
        std::stable_sort(sortedSections.begin(), sortedSections.end(), [](const PromptSection &a, const PromptSection &b) {
            return a.priority < b.priority;
        });

        QStringList renderedList;
        for (const auto &section : sortedSections) {
            renderedList.append(wrapTag(section.tag, section.content, section.attributes));
        }

        return renderedList.join("\n\n");
    }

} // namespace core::context
