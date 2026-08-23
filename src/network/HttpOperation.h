#pragma once
#include <QObject>
#include <QByteArray>
#include <QString>

namespace network {

    /**
     * @brief 表示一个正在执行中的 HTTP 请求操作
     */
    class HttpOperation : public QObject {
        Q_OBJECT

    public:
        explicit HttpOperation(QObject *parent = nullptr) : QObject(parent) {}
        virtual ~HttpOperation() = default;

        /**
         * @brief 主动取消正在进行的请求
         */
        virtual void cancel() = 0;

    Q_SIGNALS:
        /**
         * @brief 收到新的数据块（适用于流式传输）
         */
        void dataReceived(const QByteArray &data);

        /**
         * @brief 请求完全成功结束
         */
        void finished();

        /**
         * @brief 请求失败或被异常终止
         * @param errorMessage 错误消息说明
         * @param httpStatusCode HTTP 状态码（若有）
         */
        void failed(const QString &errorMessage, int httpStatusCode);
    };

} // namespace network
