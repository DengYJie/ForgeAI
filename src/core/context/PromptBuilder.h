#pragma once
#include <QString>
#include <QMap>
#include <QList>

namespace core::context {

    /**
     * @brief 标准化提示词片段定义
     */
    struct PromptSection {
        QString tag;                        ///< XML 标签名称 (如 "system_instructions", "workspace")
        QString content;                    ///< 标签内部的 Markdown 内容
        QMap<QString, QString> attributes;  ///< 标签属性 (如 <skill name="android-cli">)
        int priority = 0;                   ///< 排序权重 (较小的排在前面，保证 Prompt Caching 前缀确定性)
    };

    /**
     * @brief XML 提示词构建器
     * @details 负责管理标签闭合、属性格式化、空内容修剪与优先级排序。
     */
    class PromptBuilder {
    public:
        PromptBuilder() = default;

        /**
         * @brief 添加一个提示词段落
         * @param tag XML 标签名
         * @param content 内容正文（若为空则自动忽略）
         * @param priority 排序优先级（数值越小越靠前，默认 0）
         */
        PromptBuilder &addSection(const QString &tag, const QString &content, int priority = 0);

        /**
         * @brief 添加带有属性的提示词段落
         */
        PromptBuilder &addSection(const QString &tag, const QString &content, const QMap<QString, QString> &attributes, int priority = 0);

        /**
         * @brief 渲染并输出完全符合标准的 XML 格式化提示词
         */
        QString build() const;

        /**
         * @brief 清空当前所有段落
         */
        void clear();

        /**
         * @brief 静态工具方法：将指定内容用标准 XML 标签包裹
         */
        static QString wrapTag(const QString &tag, const QString &content, const QMap<QString, QString> &attributes = {});
    
    private:
        QList<PromptSection> m_sections;
    };

} // namespace core::context
