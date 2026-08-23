#pragma once
#include "HttpRequest.h"
#include "HttpOperation.h"

namespace network {

    /**
     * @brief HTTP 客户端接口，负责发起请求并返回操作句柄
     */
    class IHttpClient {
    public:
        virtual ~IHttpClient() = default;

        /**
         * @brief 发起 HTTP 请求
         * @param request 构造好的 HTTP 请求对象
         * @return HttpOperation* 代表该请求的句柄（调用方通常需将其纳入自动释放或绑定到特定父对象）
         */
        virtual HttpOperation* send(const HttpRequest &request) = 0;
    };

} // namespace network
