#include "ConsoleSink.h"
#include "core/logging/LogFormatter.h"
#include <iostream>

namespace core::logging {

    ConsoleSink::ConsoleSink(bool colorize) : m_colorize(colorize) {}

    void ConsoleSink::write(const LogRecord &record) {
        QString line = LogFormatter::format(record, m_colorize);
        QMutexLocker locker(&m_mutex);
        if (record.level >= LogLevel::Error) {
            std::cerr << line.toLocal8Bit().constData() << std::endl;
        } else {
            std::cout << line.toLocal8Bit().constData() << std::endl;
        }
    }

    void ConsoleSink::flush() {
        QMutexLocker locker(&m_mutex);
        std::cout.flush();
        std::cerr.flush();
    }

} // namespace core::logging
