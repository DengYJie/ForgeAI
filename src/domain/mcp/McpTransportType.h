#pragma once

namespace domain::mcp {

    /**
     * @brief MCP 通信传输协议类型
     */
    enum class McpTransportType {
        Stdio,      ///< 基于标准输入输出进程通信
        Http,       ///< 基于 HTTP / SSE 流式通信
        WebSocket   ///< 基于 WebSocket 双向通信
    };

} // namespace domain::mcp
