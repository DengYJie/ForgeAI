#include "DiagnosticExporter.h"
#include "SensitiveDataFilter.h"
#include "LoggingService.h"
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QSysInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDataStream>

namespace core::logging {

    namespace {
        // 标准 CRC-32 计算
        uint32_t computeCrc32(const QByteArray &data) {
            static uint32_t table[256];
            static bool initialized = false;
            if (!initialized) {
                for (uint32_t i = 0; i < 256; ++i) {
                    uint32_t c = i;
                    for (int j = 0; j < 8; ++j) {
                        c = (c & 1) ? (0xEDB88320L ^ (c >> 1)) : (c >> 1);
                    }
                    table[i] = c;
                }
                initialized = true;
            }

            uint32_t crc = 0xFFFFFFFF;
            const auto *ptr = reinterpret_cast<const uint8_t *>(data.constData());
            for (int i = 0; i < data.size(); ++i) {
                crc = table[(crc ^ ptr[i]) & 0xFF] ^ (crc >> 8);
            }
            return crc ^ 0xFFFFFFFF;
        }

        struct ZipEntry {
            QString name;
            QByteArray data;
            uint32_t crc32 = 0;
            uint32_t offset = 0;
        };

        // 简易自包含标准 PKZIP 写入器 (无外部库依赖)
        QByteArray createZipArchive(const QList<ZipEntry> &entries) {
            QByteArray buffer;
            QDataStream stream(&buffer, QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::LittleEndian);

            QList<ZipEntry> writtenEntries;
            writtenEntries.reserve(entries.size());

            // 1. Local File Headers + File Data
            for (auto entry : entries) {
                entry.offset = static_cast<uint32_t>(buffer.size());
                entry.crc32 = computeCrc32(entry.data);
                QByteArray nameUtf8 = entry.name.toUtf8();

                // Local Header Signature (0x04034b50)
                stream << static_cast<uint32_t>(0x04034b50);
                stream << static_cast<uint16_t>(20);           // Version needed: 2.0
                stream << static_cast<uint16_t>(0);            // General purpose bit flag
                stream << static_cast<uint16_t>(0);            // Compression method: 0 (Stored / uncompressed)
                stream << static_cast<uint16_t>(0);            // Last mod time
                stream << static_cast<uint16_t>(0);            // Last mod date
                stream << static_cast<uint32_t>(entry.crc32);  // CRC-32
                stream << static_cast<uint32_t>(entry.data.size()); // Compressed size
                stream << static_cast<uint32_t>(entry.data.size()); // Uncompressed size
                stream << static_cast<uint16_t>(nameUtf8.size());   // Filename length
                stream << static_cast<uint16_t>(0);            // Extra field length

                stream.writeRawData(nameUtf8.constData(), nameUtf8.size());
                stream.writeRawData(entry.data.constData(), entry.data.size());

                writtenEntries.append(entry);
            }

            uint32_t centralDirOffset = static_cast<uint32_t>(buffer.size());

            // 2. Central Directory
            for (const auto &entry : writtenEntries) {
                QByteArray nameUtf8 = entry.name.toUtf8();

                // Central Directory Header Signature (0x02014b50)
                stream << static_cast<uint32_t>(0x02014b50);
                stream << static_cast<uint16_t>(20);           // Version made by
                stream << static_cast<uint16_t>(20);           // Version needed
                stream << static_cast<uint16_t>(0);            // Flags
                stream << static_cast<uint16_t>(0);            // Compression method
                stream << static_cast<uint16_t>(0);            // Mod time
                stream << static_cast<uint16_t>(0);            // Mod date
                stream << static_cast<uint32_t>(entry.crc32);  // CRC-32
                stream << static_cast<uint32_t>(entry.data.size()); // Compressed size
                stream << static_cast<uint32_t>(entry.data.size()); // Uncompressed size
                stream << static_cast<uint16_t>(nameUtf8.size());   // Filename length
                stream << static_cast<uint16_t>(0);            // Extra field length
                stream << static_cast<uint16_t>(0);            // Comment length
                stream << static_cast<uint16_t>(0);            // Disk number start
                stream << static_cast<uint16_t>(0);            // Internal file attributes
                stream << static_cast<uint32_t>(0);            // External file attributes
                stream << static_cast<uint32_t>(entry.offset); // Relative offset of local header

                stream.writeRawData(nameUtf8.constData(), nameUtf8.size());
            }

            uint32_t centralDirSize = static_cast<uint32_t>(buffer.size()) - centralDirOffset;

            // 3. End of Central Directory Record (EOCD)
            // EOCD Signature (0x06054b50)
            stream << static_cast<uint32_t>(0x06054b50);
            stream << static_cast<uint16_t>(0);            // Number of this disk
            stream << static_cast<uint16_t>(0);            // Disk where CD starts
            stream << static_cast<uint16_t>(writtenEntries.size()); // Total entries on this disk
            stream << static_cast<uint16_t>(writtenEntries.size()); // Total entries in CD
            stream << static_cast<uint32_t>(centralDirSize);        // Size of CD
            stream << static_cast<uint32_t>(centralDirOffset);      // Offset of start of CD
            stream << static_cast<uint16_t>(0);            // Comment length

            return buffer;
        }
    } // namespace

    QString DiagnosticExporter::generateSystemInfoJson() {
        QJsonObject root;

        QJsonObject appObj;
        appObj[QStringLiteral("name")] = QStringLiteral("ForgeAI");
        appObj[QStringLiteral("version")] = QStringLiteral("1.0.0");
        appObj[QStringLiteral("qtVersion")] = QString::fromLatin1(qVersion());
        appObj[QStringLiteral("buildAbi")] = QSysInfo::buildAbi();
        root[QStringLiteral("app")] = appObj;

        QJsonObject sysObj;
        sysObj[QStringLiteral("os")] = QSysInfo::prettyProductName();
        sysObj[QStringLiteral("kernelType")] = QSysInfo::kernelType();
        sysObj[QStringLiteral("kernelVersion")] = QSysInfo::kernelVersion();
        sysObj[QStringLiteral("currentCpuArchitecture")] = QSysInfo::currentCpuArchitecture();
        sysObj[QStringLiteral("buildCpuArchitecture")] = QSysInfo::buildCpuArchitecture();
        root[QStringLiteral("system")] = sysObj;

        QJsonObject runtimeObj;
        runtimeObj[QStringLiteral("exportedAt")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        runtimeObj[QStringLiteral("logDirectory")] = QDir::homePath() + QStringLiteral("/.forgeai/logs");
        root[QStringLiteral("runtime")] = runtimeObj;

        return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }

    QString DiagnosticExporter::generateProvidersSummaryJson(const QList<domain::model::ModelProvider> &providers) {
        QJsonArray provArray;

        for (const auto &p : providers) {
            QJsonObject obj;
            obj[QStringLiteral("id")] = p.id;
            obj[QStringLiteral("name")] = p.name;
            obj[QStringLiteral("protocol")] = static_cast<int>(p.protocol);
            obj[QStringLiteral("sdkPackage")] = p.sdkPackage;
            obj[QStringLiteral("isEnabled")] = p.isEnabled;
            obj[QStringLiteral("isCustom")] = p.isCustom;
            obj[QStringLiteral("origin")] = p.origin == domain::model::DataOrigin::User ? QStringLiteral("User") : QStringLiteral("BuiltIn");
            
            // 安全过滤：Base URL 移除潜在敏感 Query 参数
            obj[QStringLiteral("baseUrl")] = SensitiveDataFilter::sanitizeUrl(p.baseUrl);

            provArray.append(obj);
        }

        return QString::fromUtf8(QJsonDocument(provArray).toJson(QJsonDocument::Indented));
    }

    bool DiagnosticExporter::exportDiagnosticZip(
        const QString &destinationZipPath,
        const QList<domain::model::ModelProvider> &providers) {
        
        LoggingService::instance().flush();

        QList<ZipEntry> entries;

        // 1. system_info.json
        entries.append(ZipEntry{
            QStringLiteral("system_info.json"),
            generateSystemInfoJson().toUtf8()
        });

        // 2. providers_summary.json (如果传入了 providers)
        if (!providers.isEmpty()) {
            entries.append(ZipEntry{
                QStringLiteral("providers_summary.json"),
                generateProvidersSummaryJson(providers).toUtf8()
            });
        }

        // 3. 读取本地日志文件 (经过 SensitiveDataFilter 深度脱敏)
        QDir logDir(QDir::homePath() + QStringLiteral("/.forgeai/logs"));
        if (logDir.exists()) {
            const auto logFiles = logDir.entryInfoList(
                QStringList{QStringLiteral("app.log"), QStringLiteral("app.*.log")},
                QDir::Files | QDir::NoSymLinks,
                QDir::Name
            );

            for (const auto &info : logFiles) {
                QFile f(info.absoluteFilePath());
                if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QString rawLog = QString::fromUtf8(f.readAll());
                    QString sanitizedLog = SensitiveDataFilter::redactText(rawLog);
                    entries.append(ZipEntry{
                        info.fileName(),
                        sanitizedLog.toUtf8()
                    });
                }
            }
        }

        // 4. README.txt 说明文档
        QString readme = QStringLiteral(
            "ForgeAI Diagnostic Archive\n"
            "==========================\n"
            "Generated: %1\n\n"
            "Privacy Notice:\n"
            "All API keys, Authorization headers, and confidential tokens have been automatically sanitized.\n"
            "No prompt or LLM response body contents are included in this package.\n"
        ).arg(QDateTime::currentDateTime().toString(Qt::ISODate));

        entries.append(ZipEntry{
            QStringLiteral("README.txt"),
            readme.toUtf8()
        });

        // 生成 ZIP 并写入目标路径
        QByteArray zipBytes = createZipArchive(entries);
        if (zipBytes.isEmpty()) {
            return false;
        }

        QFile outFile(destinationZipPath);
        if (!outFile.open(QIODevice::WriteOnly)) {
            return false;
        }

        qint64 written = outFile.write(zipBytes);
        outFile.close();
        return written == zipBytes.size();
    }

} // namespace core::logging
