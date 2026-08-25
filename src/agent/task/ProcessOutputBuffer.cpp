#include "ProcessOutputBuffer.h"
#include <algorithm>

namespace agent::task {

    ProcessOutputBuffer::ProcessOutputBuffer(int maxCapacityBytes)
        : m_maxCapacityBytes(maxCapacityBytes > 0 ? maxCapacityBytes : 4 * 1024 * 1024) {
    }

    void ProcessOutputBuffer::append(const QByteArray& data) {
        if (data.isEmpty()) return;

        m_totalProducedBytes += static_cast<quint64>(data.size());
        m_buffer.append(data);

        if (m_buffer.size() > m_maxCapacityBytes) {
            const int overflow = m_buffer.size() - m_maxCapacityBytes;
            m_buffer.remove(0, overflow);
            m_headOffset += static_cast<quint64>(overflow);
            m_hasTruncated = true;
        }
    }

    quint64 ProcessOutputBuffer::totalProducedBytes() const {
        return m_totalProducedBytes;
    }

    quint64 ProcessOutputBuffer::availableHeadOffset() const {
        return m_headOffset;
    }

    bool ProcessOutputBuffer::hasTruncated() const {
        return m_hasTruncated;
    }

    QByteArray ProcessOutputBuffer::readBytesFrom(
        quint64 cursor,
        int maxBytes,
        bool* cursorLost,
        quint64* availableFrom,
        quint64* nextCursor
    ) const {
        if (cursorLost) *cursorLost = false;
        if (availableFrom) *availableFrom = m_headOffset;

        if (maxBytes <= 0) maxBytes = 32768;

        // 如果请求的游标超前于当前产出的最大值，修正到尾部
        if (cursor >= m_totalProducedBytes) {
            if (nextCursor) *nextCursor = m_totalProducedBytes;
            return QByteArray();
        }

        // 如果请求的游标已经滑出当前内存窗口，标记游标丢失并从当前最早可用位置读取
        quint64 effectiveCursor = cursor;
        if (cursor < m_headOffset) {
            if (cursorLost) *cursorLost = true;
            effectiveCursor = m_headOffset;
        }

        const int localOffset = static_cast<int>(effectiveCursor - m_headOffset);
        if (localOffset < 0 || localOffset >= m_buffer.size()) {
            if (nextCursor) *nextCursor = m_totalProducedBytes;
            return QByteArray();
        }

        const int bytesToRead = std::min<int>(maxBytes, static_cast<int>(m_buffer.size() - localOffset));
        const QByteArray chunk = m_buffer.mid(localOffset, bytesToRead);

        if (nextCursor) {
            *nextCursor = effectiveCursor + static_cast<quint64>(bytesToRead);
        }

        return chunk;
    }

    QByteArray ProcessOutputBuffer::fullBufferedBytes() const {
        return m_buffer;
    }

    void ProcessOutputBuffer::clear() {
        m_buffer.clear();
        m_headOffset = 0;
        m_totalProducedBytes = 0;
        m_hasTruncated = false;
    }

} // namespace agent::task
