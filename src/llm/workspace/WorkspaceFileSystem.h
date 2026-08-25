#pragma once

#include <QString>
#include <QStringList>

namespace llm::workspace {

    /**
     * @brief 工作区安全沙箱文件系统
     * @details 集中负责路径合法性检查、规范化、Windows 大小写不敏感匹配、符号链接防逃逸与忽略规则过滤。
     */
    class WorkspaceFileSystem {
    public:
        explicit WorkspaceFileSystem(QStringList ignorePatterns = {});
        ~WorkspaceFileSystem() = default;

        /**
         * @brief 解析并验证可读工作区路径
         * @param workspaceRoot 工作区绝对路径根目录
         * @param relativePath 用户或模型传入的相对路径
         * @param error 出错时输出的错误文本
         * @return 验证通过的绝对规范化路径；若越界或非法则返回空字符串
         */
        QString resolveReadablePath(
            const QString& workspaceRoot,
            const QString& relativePath,
            QString* error = nullptr
        ) const;

        /**
         * @brief 解析并验证可写工作区路径（允许文件尚不存在，但其所在父目录必须合法且位于工作区内）
         */
        QString resolveWritablePath(
            const QString& workspaceRoot,
            const QString& relativePath,
            QString* error = nullptr
        ) const;

        /**
         * @brief 判断相对路径是否匹配忽略规则（如 .git, node_modules, build/ 等）
         */
        bool isIgnored(const QString& relativePath) const;

        /**
         * @brief 获取当前设置的忽略规则列表
         */
        QStringList ignorePatterns() const;

    private:
        QStringList m_ignorePatterns;
    };

} // namespace llm::workspace
