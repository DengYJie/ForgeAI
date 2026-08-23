#pragma once
#include "ILogSink.h"
#include <QMutex>

namespace core::logging {

    class ConsoleSink : public ILogSink {
    public:
        explicit ConsoleSink(bool colorize = true);
        ~ConsoleSink() override = default;

        void write(const LogRecord &record) override;
        void flush() override;

    private:
        bool m_colorize = true;
        QMutex m_mutex;
    };

} // namespace core::logging
