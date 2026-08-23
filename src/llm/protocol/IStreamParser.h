#pragma once
#include <QByteArray>
#include <QList>
#include "domain/llm/ChatEvent.h"

namespace llm::protocol {

    /**
     * @brief 流解析器接口
     * @details 负责处理底层的半包、粘包，并提取出结构化的领域事件
     */
    class IStreamParser {
    public:
        virtual ~IStreamParser() = default;

        /**
         * @brief 喂入一段网络数据
         * @param chunk 来自网络的字节流
         * @return QList<domain::llm::ChatEvent> 成功解析出的一批事件
         */
        virtual QList<domain::llm::ChatEvent> feed(const QByteArray &chunk) = 0;

        /**
         * @brief 网络流结束时调用，要求解析器把缓冲区残余的数据强行处理
         * @return QList<domain::llm::ChatEvent> 剩余的事件
         */
        virtual QList<domain::llm::ChatEvent> finish() = 0;

        /**
         * @brief 重置解析器状态，准备应对下一次全新的请求
         */
        virtual void reset() = 0;
    };

} // namespace llm::protocol
