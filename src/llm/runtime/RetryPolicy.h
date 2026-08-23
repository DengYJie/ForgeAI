#pragma once

namespace llm::runtime {

    /**
     * @brief 自动重试治理策略
     */
    struct RetryPolicy {
        bool enabled = true;             ///< 是否启用瞬态错误自动重试
        int maxAttempts = 3;             ///< 最大尝试总次数 (1次初始请求 + 最多2次重试)
        int baseDelayMs = 1000;          ///< 基础退避时间 (默认 1s)
        int maxDelayMs = 10000;          ///< 最大退避封顶时间 (默认 10s)
        double backoffFactor = 2.0;      ///< 指数退避乘数
        double jitterFactor = 0.2;       ///< 抖动因子 (+/- 20%)

        bool operator==(const RetryPolicy& other) const = default;
    };

} // namespace llm::runtime
