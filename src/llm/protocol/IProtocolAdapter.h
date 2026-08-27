#pragma once
#include <memory>
#include <QByteArray>
#include "network/HttpRequest.h"
#include "domain/llm/ChatRequest.h"
#include "domain/llm/ResolvedChatOptions.h"
#include "domain/llm/ChatError.h"
#include "domain/model/ModelProvider.h"
#include "domain/model/ResolvedModel.h"
#include "IStreamParser.h"

namespace llm::protocol {

    /**
     * @brief 服务商协议适配器接口
     */
    class IProtocolAdapter {
    public:
        virtual ~IProtocolAdapter() = default;

        /**
         * @brief 将语义选项与领域请求转换为具体的 HTTP 请求载荷
         * @param model 运行时聚合模型实体
         * @param request 领域层的统一请求意图
         * @param options 经过 Resolver 解析后的最终语义参数
         * @return 构造好的 HTTP 请求对象
         */
        virtual network::HttpRequest buildChatRequest(
            const domain::model::ResolvedModel &model,
            const domain::llm::ChatRequest &request,
            const domain::llm::ResolvedChatOptions &options) const = 0;

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

        /**
         * @brief 检查该协议是否支持模型自动发现
         */
        virtual bool supportsModelDiscovery() const { return true; }

        /**
         * @brief 构建查询远程模型列表的 HTTP 请求
         */
        virtual network::HttpRequest buildListModelsRequest(
            const domain::model::ModelProvider &provider) const = 0;

        /**
         * @brief 解析远程模型列表响应
         */
        virtual QList<domain::model::ProviderModel> parseListModelsResponse(
            const QByteArray &responseBody,
            const QString &providerId) const = 0;
    };

} // namespace llm::protocol
