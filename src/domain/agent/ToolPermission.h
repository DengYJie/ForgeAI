#pragma once

#include <QString>
#include <QList>

namespace domain::agent {

    /**
     * @brief 工具所需权限类型枚举
     */
    enum class ToolPermissionType {
        ReadOnly,           ///< 只读操作（文件读取、列表查看等）
        WriteWorkspace,     ///< 工作区写操作（文件创建、修改、删除等）
        ExecuteProcess,     ///< 进程执行操作（命令行命令、子进程运行等）
        Network,            ///< 网络访问权限（HTTP 请求、外部 API 等）
        ExternalService     ///< 外部扩展协议服务（如第三方 MCP 服务器等）
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
     * @brief 工具声明的具体权限要求
     */
    struct ToolPermission {
        ToolPermissionType type = ToolPermissionType::ReadOnly;
        QString reason;     ///< 请求该权限的具体原因/说明

        bool operator==(const ToolPermission& other) const = default;
    };

} // namespace domain::agent
