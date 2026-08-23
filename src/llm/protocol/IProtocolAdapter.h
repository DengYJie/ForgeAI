#pragma once
#include <memory>
#include <QByteArray>
#include "network/HttpRequest.h"
#include "domain/llm/ChatRequest.h"
#include "domain/llm/ChatError.h"
#include "domain/model/ModelProvider.h"
#include "IStreamParser.h"

namespace llm::protocol {

    /**
     * @brief 服务商协议适配器接口
     */
    class IProtocolAdapter {
    public:
        virtual ~IProtocolAdapter() = default;

        /**
         * @brief 将统一的请求抽象转换成具体的 HTTP 请求载荷
         * @param provider 配置信息 (包含 baseUrl, apiKey 等)
         * @param request 领域层的统一请求实体
         * @return 构造好的 HTTP 请求对象
         */
        virtual network::HttpRequest buildChatRequest(
            const domain::model::ModelProvider &provider,
            const domain::llm::ChatRequest &request) const = 0;

        /**
         * @brief 创建处理该协议特有流格式的解析器
         */
        virtual std::unique_ptr<IStreamParser> createStreamParser() const = 0;

        /**
         * @brief 解析非 200 HTTP 响应带来的错误内容
         * @param httpStatusCode 状态码
         * @param responseBody 响应体
         * @return 统一的 ChatError 结构
         */
        virtual domain::llm::ChatError parseError(
            int httpStatusCode,
            const QByteArray &responseBody) const = 0;
    };

} // namespace llm::protocol
