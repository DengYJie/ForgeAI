#pragma once

#include <QString>
#include <QList>

namespace domain::agent {

    /**
     * @brief 工具所需权限类型枚举（支持细粒度分类）
     */
    enum class ToolPermissionType {
        ReadOnly = 0,               ///< 只读操作（兼容通用只读）
        WriteWorkspace = 1,         ///< 工作区写操作（兼容通用写入）
        ExecuteProcess = 2,         ///< 进程执行操作
        Network = 3,                ///< 网络访问权限（通用）
        ExternalService = 4,        ///< 外部扩展协议服务（通用）

        // 细粒度扩展权限
        FileSystemRead = 10,        ///< 本地文件系统只读
        FileSystemWrite = 11,       ///< 本地文件系统写入/修改
        NetworkRead = 20,           ///< 外部网络读取 (HTTP GET)
        NetworkWrite = 21,          ///< 外部网络写入 (HTTP POST/PUT/DELETE)
        ExternalServiceRead = 30,   ///< MCP/外部服务只读工具
        ExternalServiceWrite = 31,  ///< MCP/外部服务写入工具
        ProcessExecute = 40,        ///< 命令行/子进程启动与执行
        DestructiveOperation = 50   ///< 破坏性操作（删库、强制清理、重置工作区）
    };

    /**
     * @brief 权限裁决结果
     */
    enum class PermissionDecision {
        Allow,              ///< 允许直接执行
        AskUser,            ///< 需要用户显式授权确认
        Deny                ///< 拒绝执行
    };

    /**
     * @brief 权限授权生命周期与记忆范围
     */
    enum class PermissionScope {
        Once,               ///< 仅允许当前单次调用
        Run,                ///< 在当前 Agent Run 会话周期内记住该工具的授权
        Project,            ///< 在当前项目全局持久化记住该工具的授权
        Global              ///< 全局持久化记住该工具的授权
    };

    /**
     * @brief 工具声明的具体权限要求
     */
    struct ToolPermission {
        ToolPermissionType type = ToolPermissionType::ReadOnly;
        QString reason;     ///< 请求该权限的具体原因/说明

        bool operator==(const ToolPermission& other) const = default;
    };

} // namespace domain::agent
