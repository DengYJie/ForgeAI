#pragma once
#include <QObject>
#include <QString>
#include <memory>
#include "domain/model/ResolvedModel.h"
#include "domain/llm/ChatRequest.h"
#include "domain/llm/ChatEvent.h"

namespace application::ports {

    /**
     * @brief 一次独立的 LLM 请求操作句柄
     */
    class IChatOperation : public QObject {
        Q_OBJECT
    public:
        explicit IChatOperation(QObject *parent = nullptr) : QObject(parent) {}
        virtual ~IChatOperation() = default;

        virtual void cancel() = 0;

    Q_SIGNALS:
        void eventReceived(const domain::llm::ChatEvent &event);
    };

    /**
     * @brief 面向业务层 (Application) 的纯粹的大模型交互网关
     * @details 不包含具体协议实现和 HTTP 底层细节
     */
    class IChatModelGateway {
    public:
        virtual ~IChatModelGateway() = default;

        /**
         * @brief 向模型发送聊天请求
         * @param model 包含服务商配置、模型绑定与本体能力的聚合实体
         * @param request 构造好的请求意图
         * @return IChatOperation* 请求操作句柄，调用方负责生命周期管理
         */
        virtual IChatOperation* sendRequest(
            const domain::model::ResolvedModel &model,
            const domain::llm::ChatRequest &request) = 0;
    };

} // namespace application::ports
