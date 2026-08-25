#pragma once

namespace domain::mcp {

    /**
     * @brief MCP 服务端连接生命周期状态
     */
    enum class McpConnectionState {
        Stopped,        ///< 未启动
        Starting,       ///< 传输通道正在启动 (如 QProcess 启动中)
        Initializing,   ///< 正在进行 MCP 握手 (initialize / initialized)
        Ready,          ///< 就绪，可提供 Tool/Resource/Prompt 调用
        Stopping,       ///< 正在停止
        Failed          ///< 异常中断或连接失败
    };

} // namespace domain::mcp
