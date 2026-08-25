#pragma once

#include <QByteArray>
#include <QString>
#include <QStringDecoder>

namespace agent::task {

    /**
     * @brief 解码结果结构
     */
    struct ProcessDecodeResult {
        QString text;               ///< 解码后的文本
        int bytesConsumed = 0;      ///< 成功消费的有效字节数（未完成的多字节尾部字节不被计入）
        bool hasError = false;      ///< 是否遇到无法解析的非法/损坏字节
    };

    /**
     * @brief 进程输出文本有状态/跨边界解码器
     * @details 支持 UTF-8, System, GB18030, GBK, Shift-JIS, Windows-1252 等编码，
     *          并在 chunk 边界自动检测多字节截断，确保游标按完整字符边界推进。
     */
    class ProcessOutputDecoder {
    public:
        /**
         * @brief 对字节切片进行安全解码并处理末尾多字节边界
         * @param rawChunk 待解码的原始字节切片
         * @param encoding 目标编码名称 (如 "utf-8", "system", "gb18030", "gbk", "shift-jis")
         * @param isFinal 是否为流的末尾（末尾时不再等待后续字节）
         */
        static ProcessDecodeResult decodeChunk(
            const QByteArray& rawChunk,
            const QString& encoding = QStringLiteral("utf-8"),
            bool isFinal = false
        );

        /**
         * @brief 解析规范化的编码名称
         */
        static QString normalizeEncoding(const QString& name);
    };

} // namespace agent::task
