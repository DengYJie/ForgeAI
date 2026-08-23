#pragma once
#include <QString>
#include <QList>
#include <QByteArray>
#include "domain/model/ModelProvider.h"

namespace core::logging {

    /**
     * @brief 诊断报告与脱敏归档导出器
     */
    class DiagnosticExporter {
    public:
        /**
         * @brief 生成系统与运行环境诊断 JSON 文本
         */
        static QString generateSystemInfoJson();

        /**
         * @brief 生成已配置服务商与模型的脱敏概览 JSON
         */
        static QString generateProvidersSummaryJson(const QList<domain::model::ModelProvider> &providers);

        /**
         * @brief 打包生成符合标准 PKZIP 规范的诊断压缩包（包含系统信息、脱敏日志与模型配置）
         * @param destinationZipPath 导出的 .zip 文件完整路径
         * @param providers 可选传入当前已加载的服务商列表
         * @return 是否导出成功
         */
        static bool exportDiagnosticZip(
            const QString &destinationZipPath,
            const QList<domain::model::ModelProvider> &providers = {}
        );
    };

} // namespace core::logging
