#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>

namespace agent::task {

    /**
     * @brief 基于单调递增绝对字节游标的滑动窗口输出缓冲区
     * @details 维护最近的内存日志切片（默认上限 4MB），支持绝对游标增量消费与游标过期检测。
     */
    class ProcessOutputBuffer {
    public:
        explicit ProcessOutputBuffer(int maxCapacityBytes = 4 * 1024 * 1024);

        /**
         * @brief 追加新产出的字节流
         */
        void append(const QByteArray& data);

        /**
         * @brief 累计产出的总字节数（单调递增绝对值）
         */
        quint64 totalProducedBytes() const;

        /**
         * @brief 内存缓冲区中最早可读字节的绝对偏移位置
         */
        quint64 availableHeadOffset() const;

        /**
         * @brief 是否曾发生历史缓冲区移除
         */
        bool hasTruncated() const;

        /**
         * @brief 从指定绝对游标位置读取切片数据
         * @param cursor 请求的绝对字节游标
         * @param maxBytes 单次最大读取字节数
         * @param cursorLost 输出：请求的游标是否已滑出缓冲区被丢弃
         * @param availableFrom 输出：当前缓冲区最早可读的游标
         * @param nextCursor 输出：下一次读取应使用的游标值
         */
        QString readFrom(
            quint64 cursor,
            int maxBytes,
            bool* cursorLost = nullptr,
            quint64* availableFrom = nullptr,
            quint64* nextCursor = nullptr
        ) const;

        /**
         * @brief 获取当前保留的全部缓冲区文本（如用于快照与前台模式全量回传）
         */
        QString fullBufferedText() const;

        void clear();

    private:
        int m_maxCapacityBytes;
        quint64 m_headOffset = 0;
        quint64 m_totalProducedBytes = 0;
        QByteArray m_buffer;
        bool m_hasTruncated = false;
    };

} // namespace agent::task
