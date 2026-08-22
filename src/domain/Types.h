#pragma once

namespace domain {
    /**
     * @brief 会话模式
     */
    enum class ConversationMode {
        Normal, ///< 普通对话模式（单轮/多轮直接对话）
        Agent ///< 智能体模式（支持规划、工具调用、思考链）
    };

    /**
     * @brief 交互回合状态
     */
    enum class TurnStatus {
        Pending, ///< 排队等待处理
        Running, ///< 正在生成或执行工具
        Waiting, ///< 等待外部输入或用户确认
        Completed, ///< 本回合已顺利完成
        Failed, ///< 执行失败或网络异常
        Cancelled ///< 用户手动取消或中止
    };

    /**
     * @brief 消息发送者角色
     */
    enum class MessageRole {
        System, ///< 系统提示词或上下文约束
        User, ///< 用户输入
        Assistant, ///< 模型生成的回复或工具调用指令
        Tool ///< 本地工具执行返回的结果载荷
    };

    /**
     * @brief 消息状态
     */
    enum class MessageStatus {
        Pending, ///< 等待发送
        Sending, ///< 正在发送或流式接收中
        Sent, ///< 已完整接收并持久化
        Failed, ///< 发送或生成失败
        Interrupted ///< 生成被主动中止或意外中断
    };

    /**
     * @brief 消息块类型（用于细粒度多模态和工具载荷分发）
     */
    enum class BlockType {
        Text, ///< 纯文本或 Markdown 内容
        Thought,    ///< 深度思考与推理过程链
        Image, ///< 图片附件
        File, ///< 文档附件
        ToolCall, ///< 模型发起的工具调用请求
        ToolResult ///< 本地工具执行结果
    };
} // namespace domain
