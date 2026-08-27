#pragma once

#include <QString>

namespace services::process {

    /**
     * @brief 命令风险评估结果
     */
    struct CommandRiskAssessment {
        bool destructive = false;   ///< 是否包含破坏性操作
        QString reason;             ///< 风险原因描述
    };

    /**
     * @brief Shell 命令行静态风险分析器
     */
    class ShellCommandRiskAnalyzer {
    public:
        /**
         * @brief 静态分析待执行的 Shell 命令行文本
         */
        CommandRiskAssessment analyze(const QString& command) const;
    };

} // namespace services::process
