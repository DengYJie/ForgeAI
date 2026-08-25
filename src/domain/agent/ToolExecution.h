#pragma once
#include <QString>
#include <QJsonObject>

namespace domain::agent {
    /**
     * @brief 大模型发起的工具调用指令实体
     */
    struct ToolCall {
        QString id; ///< 调用的唯一标识符 (如 "call_abc123")
        QString name; ///< 调用的工具名 (如 "get_weather")
        QString arguments; ///< 参数 JSON 字符串 (如 "{\"location\":\"Beijing\"}")

        bool operator==(const ToolCall &other) const = default;
    };

    /**
     * @brief 本地工具执行后返回的执行结果实体
     */
    struct ToolResult {
        QString toolCallId; ///< 对应的 ToolCall 标识符
        QString content; ///< 工具执行产出的结果文本或 JSON 串
        bool isError = false; ///< 是否执行失败（便于模型自主进行错误重试/修正）
        QString errorCode; ///< 结构化错误代码（如 "UnsupportedBinaryFile", "PatchContextNotFound", "FileAlreadyExists"）
        QJsonObject metadata; ///< 结构化元数据（如 {"path": "...", "bytes": 1024, "truncated": false}）

        bool operator==(const ToolResult &other) const = default;
    };
} // namespace domain::agent
