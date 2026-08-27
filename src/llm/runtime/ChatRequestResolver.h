#pragma once

#include "domain/model/ResolvedModel.h"
#include "domain/llm/ChatRequest.h"
#include "domain/llm/ResolvedChatOptions.h"

namespace llm::runtime {

    /**
     * @brief 纯计算逻辑解析器：将业务/调用方意图（ChatRequest）结合聚合模型元数据（ResolvedModel），
     *        推导出面向底层协议的标准语义选项（ResolvedChatOptions）。
     * @note 此类无状态、无 QObject、无网络和外部依赖。
     */
    class ChatRequestResolver {
    public:
        static domain::llm::ResolvedChatOptions resolve(
            const domain::model::ResolvedModel &model,
            const domain::llm::ChatRequest &request
        );
    };

} // namespace llm::runtime
