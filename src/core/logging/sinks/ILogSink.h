#pragma once
#include "core/logging/LogRecord.h"
#include <vector>

namespace core::logging {

    /**
     * @brief 日志输出目标抽象接口
     */
    class ILogSink {
    public:
        virtual ~ILogSink() = default;

        /**
         * @brief 写入一条日志记录
         */
        virtual void write(const LogRecord &record) = 0;

        /**
         * @brief 批量写入日志记录（子类可重载以实现高效批量写入与单次系统调用）
         */
        virtual void writeBatch(const std::vector<LogRecord> &records) {
            for (const auto &record : records) {
                write(record);
            }
        }

        /**
         * @brief 立即刷新底层缓冲区
         */
        virtual void flush() = 0;

        /**
         * @brief 重置/清空底层输出目标资源（如安全截断日志文件）
         */
        virtual void reset() {}
    };

} // namespace core::logging
