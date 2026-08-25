#include "ProcessOutputDecoder.h"
#include <QStringConverter>
#include <algorithm>

namespace agent::task {

    QString ProcessOutputDecoder::normalizeEncoding(const QString& name) {
        const QString lower = name.trimmed().toLower();
        if (lower.isEmpty() || lower == QStringLiteral("utf-8") || lower == QStringLiteral("utf8")) {
            return QStringLiteral("utf-8");
        }
        if (lower == QStringLiteral("system") || lower == QStringLiteral("locale")) {
            return QStringLiteral("system");
        }
        if (lower == QStringLiteral("gbk") || lower == QStringLiteral("gb2312") || lower == QStringLiteral("gb18030")) {
            return QStringLiteral("gb18030");
        }
        if (lower == QStringLiteral("shift-jis") || lower == QStringLiteral("shift_jis") || lower == QStringLiteral("sjis")) {
            return QStringLiteral("shift-jis");
        }
        if (lower == QStringLiteral("windows-1252") || lower == QStringLiteral("cp1252")) {
            return QStringLiteral("windows-1252");
        }
        return lower;
    }

    namespace {
        // UTF-8 尾部未完成字节检测
        int getUtf8IncompleteTailBytes(const QByteArray& chunk) {
            if (chunk.isEmpty()) return 0;
            const int len = chunk.size();

            // 检查末尾 1~3 字节是否为多字节截断
            for (int i = 1; i <= 4 && i <= len; ++i) {
                const unsigned char b = static_cast<unsigned char>(chunk.at(len - i));
                if ((b & 0xC0) != 0x80) { // 找到引导字节 (Lead byte)
                    int expected = 1;
                    if ((b & 0xE0) == 0xC0) expected = 2;
                    else if ((b & 0xF0) == 0xE0) expected = 3;
                    else if ((b & 0xF8) == 0xF0) expected = 4;

                    const int actual = i;
                    if (actual < expected) {
                        return actual; // 末尾有 actual 个未完成的字节
                    }
                    return 0; // 完整字符
                }
            }
            return 0;
        }

        QStringDecoder createDecoder(const QString& normEnc) {
            if (normEnc == QStringLiteral("system")) {
                return QStringDecoder(QStringDecoder::System);
            }
            if (normEnc == QStringLiteral("gb18030")) {
                auto dec = QStringDecoder("GB18030");
                if (dec.isValid()) return dec;
                auto decGbk = QStringDecoder("GBK");
                if (decGbk.isValid()) return decGbk;
            }
            if (normEnc == QStringLiteral("shift-jis")) {
                auto dec = QStringDecoder("Shift-JIS");
                if (dec.isValid()) return dec;
            }
            if (normEnc == QStringLiteral("windows-1252")) {
                auto dec = QStringDecoder("windows-1252");
                if (dec.isValid()) return dec;
            }

            // 默认 Utf8
            auto decNamed = QStringDecoder(normEnc.toUtf8().constData());
            if (decNamed.isValid()) return decNamed;
            return QStringDecoder(QStringDecoder::Utf8);
        }
    } // namespace

    ProcessDecodeResult ProcessOutputDecoder::decodeChunk(
        const QByteArray& rawChunk,
        const QString& encoding,
        bool isFinal
    ) {
        if (rawChunk.isEmpty()) {
            return {QString(), 0, false};
        }

        const QString normEnc = normalizeEncoding(encoding);
        ProcessDecodeResult result;

        // 1. 如果是 UTF-8 编码且非末尾，优先通过快速位运算裁剪末尾残缺字节
        if ((normEnc == QStringLiteral("utf-8") || normEnc.isEmpty()) && !isFinal) {
            const int tailIncomplete = getUtf8IncompleteTailBytes(rawChunk);
            if (tailIncomplete > 0 && tailIncomplete < rawChunk.size()) {
                const QByteArray safeChunk = rawChunk.left(rawChunk.size() - tailIncomplete);
                QStringDecoder decoder(QStringDecoder::Utf8);
                result.text = decoder(safeChunk);
                result.bytesConsumed = safeChunk.size();
                result.hasError = decoder.hasError();
                return result;
            }
        }

        // 2. 通用解码逻辑（含 System / GB18030 / Shift-JIS 等）
        QStringDecoder decoder = createDecoder(normEnc);
        result.text = decoder(rawChunk);
        result.bytesConsumed = rawChunk.size();
        result.hasError = decoder.hasError();

        // 3. 如果非末尾且出现解码错误，尝试回退末尾 1~3 个字节以防是跨 chunk 的多字节字符
        if (!isFinal && result.hasError && rawChunk.size() > 1) {
            for (int trim = 1; trim <= 3 && trim < rawChunk.size(); ++trim) {
                const QByteArray trimmed = rawChunk.left(rawChunk.size() - trim);
                QStringDecoder retryDecoder = createDecoder(normEnc);
                const QString retryText = retryDecoder(trimmed);
                if (!retryDecoder.hasError()) {
                    result.text = retryText;
                    result.bytesConsumed = trimmed.size();
                    result.hasError = false;
                    return result;
                }
            }
        }

        return result;
    }

} // namespace agent::task
