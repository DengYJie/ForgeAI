#pragma once
#include <QFlags>

namespace domain::model {

    /**
     * @brief 模型基础特性标志位（对齐 models.dev 多模态与能力规范）
     */
    enum class ModelCapability {
        None              = 0,
        Chat              = 1 << 0,  ///< 基础文本对话
        Streaming         = 1 << 1,  ///< 支持 SSE 流式输出
        ToolCalling       = 1 << 2,  ///< 支持原生 Function / Tool Calling
        Vision            = 1 << 3,  ///< 支持图像输入 (image)
        Audio             = 1 << 4,  ///< 支持音频输入/输出 (audio)
        Video             = 1 << 5,  ///< 支持视频分析 (video)
        Pdf               = 1 << 6,  ///< 原生支持 PDF 文档解析
        Thinking          = 1 << 7,  ///< 支持深度推理与思考流输出
        StructuredOutputs = 1 << 8,  ///< 支持严格 JSON Schema 结构化输出
        Embedding         = 1 << 9   ///< 向量嵌入模型（用于知识库 RAG）
    };
    Q_DECLARE_FLAGS(ModelCapabilities, ModelCapability)
    Q_DECLARE_OPERATORS_FOR_FLAGS(ModelCapabilities)

} // namespace domain::model
