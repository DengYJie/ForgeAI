#pragma once
#include <QString>
#include <QDateTime>

namespace llm::runtime {

    struct RequestMetrics {
        QString requestId;
        QString providerId;
        QString modelId;

        qint64 createdAt = 0;       ///< 请求创建时间戳 (ms)
        qint64 connectedAt = 0;     ///< 建连完成时间戳 (ms)
        qint64 firstTokenAt = 0;    ///< 收到第一个有效 Token/Block 的时间戳 (ms)
        qint64 finishedAt = 0;      ///< 请求结束时间戳 (ms)

        int retryCount = 0;         ///< 发生重试的次数
        int totalTokens = 0;        ///< 产生的总 Token 估计数
        int chunkCount = 0;         ///< 接收的流式数据块分片总数
        qint64 receivedBytes = 0;   ///< 接收的总字节数
        int eventCount = 0;         ///< 解析出的流式事件总数

        qint64 ttftMs() const {
            return (firstTokenAt > 0 && createdAt > 0) ? (firstTokenAt - createdAt) : 0;
        }

        qint64 totalLatencyMs() const {
            return (finishedAt > 0 && createdAt > 0) ? (finishedAt - createdAt) : 0;
        }
    };

} // namespace llm::runtime
