#include "ShellCommandRiskAnalyzer.h"
#include <QRegularExpression>

namespace services::process {

    CommandRiskAssessment ShellCommandRiskAnalyzer::analyze(const QString& command) const {
        CommandRiskAssessment assessment;
        const QString trimmed = command.trimmed();
        if (trimmed.isEmpty()) {
            return assessment;
        }

        const QString lower = trimmed.toLower();

        // 1. 递归删除与破坏性文件操作
        if (lower.contains(QStringLiteral("rm -rf")) ||
            lower.contains(QStringLiteral("rm -fr")) ||
            lower.contains(QStringLiteral("rmdir /s")) ||
            lower.contains(QStringLiteral("del /f")) ||
            lower.contains(QStringLiteral("del /s")) ||
            (lower.contains(QStringLiteral("remove-item")) && (lower.contains(QStringLiteral("-recurse")) || lower.contains(QStringLiteral("-force")))) ||
            lower.contains(QStringLiteral("format ")) ||
            lower.contains(QStringLiteral("diskpart"))) {
            assessment.destructive = true;
            assessment.reason = QStringLiteral("包含递归删除或磁盘格式化破坏性指令");
            return assessment;
        }

        // 2. Git 强行丢弃修改与覆盖操作
        if (lower.contains(QStringLiteral("git reset --hard")) ||
            lower.contains(QStringLiteral("git clean -f")) ||
            lower.contains(QStringLiteral("git clean -df")) ||
            lower.contains(QStringLiteral("git checkout -f")) ||
            lower.contains(QStringLiteral("git restore --staged --worktree")) ||
            lower.contains(QStringLiteral("git push --force")) ||
            lower.contains(QStringLiteral("git push -f"))) {
            assessment.destructive = true;
            assessment.reason = QStringLiteral("包含 Git 强制丢弃本地未提交代码或强制推送风险");
            return assessment;
        }

        // 3. 系统关机与重启
        if (lower.contains(QStringLiteral("shutdown")) ||
            lower.contains(QStringLiteral("reboot")) ||
            lower.contains(QStringLiteral("init 0")) ||
            lower.contains(QStringLiteral("init 6")) ||
            lower.contains(QStringLiteral("stop-computer")) ||
            lower.contains(QStringLiteral("restart-computer"))) {
            assessment.destructive = true;
            assessment.reason = QStringLiteral("包含系统关机或重启指令");
            return assessment;
        }

        return assessment;
    }

} // namespace services::process
