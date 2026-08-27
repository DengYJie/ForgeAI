#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <memory>
#include "IHttpClient.h"

namespace network {

    class QtHttpOperation : public HttpOperation {
        Q_OBJECT
    public:
        explicit QtHttpOperation(QNetworkReply *reply, QObject *parent = nullptr);
        ~QtHttpOperation() override;

        void cancel() override;

    private Q_SLOTS:
        void onReadyRead();
        void onFinished();
        void onErrorOccurred(QNetworkReply::NetworkError code);

    private:
        QNetworkReply *m_reply;
        QByteArray m_receivedBuffer;
        bool m_isCancelled = false;
    };

    /**
     * @brief 基于 QNetworkAccessManager 的 HTTP 客户端实现
     */
    class QtHttpClient : public QObject, public IHttpClient {
        Q_OBJECT
    public:
        explicit QtHttpClient(QObject *parent = nullptr);
        ~QtHttpClient() override;

        HttpOperation* send(const HttpRequest &request) override;

    private:
        QNetworkAccessManager *m_nam;
    };

} // namespace network
