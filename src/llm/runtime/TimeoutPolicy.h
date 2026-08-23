#pragma once

namespace llm::runtime {

    /**
     * @brief 4级细粒度超时治理策略
     */
    struct TimeoutPolicy {
        int connectTimeoutMs = 15000;       ///< TCP/TLS 握手及建连超时 (默认 15s)
        int firstTokenTimeoutMs = 60000;    ///< 发送完毕到接收第一个 token 的超时 (默认 60s, 深度推理模型可达 180s+)
        int idleTimeoutMs = 30000;          ///< 流式传输中途持续无数据的空闲超时 (默认 30s)
        int overallTimeoutMs = 600000;      ///< 整个请求的最大绝对存活时间 (默认 10分钟)

        bool operator==(const TimeoutPolicy &other) const = default;
    };

} // namespace llm::runtime
