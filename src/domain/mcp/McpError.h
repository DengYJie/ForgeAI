#pragma once

#include <QString>

namespace domain::mcp {

    /**
     * @brief 结构化 MCP 错误类型枚举
     */
    enum class McpErrorCode {
        None,
        ConfigError,            ///< 配置文件解析/合法性错误
        TransportError,         ///< 传输层通信错误 (如无法启动进程、网络断开)
        ProtocolError,          ///< JSON-RPC 格式解析或协议规范错误
        InitializationError,    ///< 初始化/握手失败或 capabilities 协商异常
        RequestTimeout,         ///< 请求超时
        ServerCrashed,          ///< 服务端进程异常崩溃
        ToolCallError,          ///< 工具调用返回错误
        Disconnected,           ///< 通道已断开
        UnsupportedVersion      ///< 协议版本不兼容
    };

    /**
     * @brief 结构化 MCP 错误详情
     */
    struct McpError {
        McpErrorCode code = McpErrorCode::None;
        QString message;
        int rpcErrorCode = 0;   ///< JSON-RPC 标准错误码 (如 -32601)

        bool hasError() const {
            return code != McpErrorCode::None;
        }
    };

} // namespace domain::mcp
