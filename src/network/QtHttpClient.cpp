#include "QtHttpClient.h"
#include <QNetworkRequest>
#include <QTimer>

namespace network {

    QtHttpOperation::QtHttpOperation(QNetworkReply *reply, QObject *parent)
        : HttpOperation(parent), m_reply(reply) {
        if (!m_reply) return;
        m_reply->setParent(this); // 托管 reply 的生命周期

        connect(m_reply, &QNetworkReply::readyRead, this, &QtHttpOperation::onReadyRead);
        connect(m_reply, &QNetworkReply::finished, this, &QtHttpOperation::onFinished);
        connect(m_reply, &QNetworkReply::errorOccurred, this, &QtHttpOperation::onErrorOccurred);
    }

    QtHttpOperation::~QtHttpOperation() {
        if (m_reply && m_reply->isRunning()) {
            m_reply->abort();
        }
    }

    void QtHttpOperation::cancel() {
        m_isCancelled = true;
        if (m_reply && m_reply->isRunning()) {
            m_reply->abort(); // 触发 errorOccurred(OperationCanceledError)
        }
    }

    void QtHttpOperation::onReadyRead() {
        if (m_isCancelled) return;
        QByteArray data = m_reply->readAll();
        if (!data.isEmpty()) {
            emit dataReceived(data);
        }
    }

    void QtHttpOperation::onFinished() {
        if (m_isCancelled) return;
        if (m_reply->error() == QNetworkReply::NoError) {
            emit finished();
        }
    }

    void QtHttpOperation::onErrorOccurred(QNetworkReply::NetworkError code) {
        if (m_isCancelled) {
            emit failed(QStringLiteral("Cancelled"), 0, {}, static_cast<int>(code));
            return;
        }

        int httpStatusCode = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString errorMsg = m_reply->errorString();
        QByteArray responseBody = m_reply->readAll();
        emit failed(errorMsg, httpStatusCode, responseBody, static_cast<int>(code));
    }

    // ==========================================

    QtHttpClient::QtHttpClient(QObject *parent)
        : QObject(parent), m_nam(new QNetworkAccessManager(this)) {
    }

    QtHttpClient::~QtHttpClient() = default;

    HttpOperation* QtHttpClient::send(const HttpRequest &request) {
        QNetworkRequest netReq(QUrl(request.url));

        // 设置超时
        netReq.setTransferTimeout(request.timeoutMs);

        // 设置 headers
        for (auto it = request.headers.constBegin(); it != request.headers.constEnd(); ++it) {
            netReq.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
        }

        QNetworkReply *reply = nullptr;
        switch (request.method) {
            case HttpMethod::Get:
                reply = m_nam->get(netReq);
                break;
            case HttpMethod::Post:
                reply = m_nam->post(netReq, request.body);
                break;
            case HttpMethod::Put:
                reply = m_nam->put(netReq, request.body);
                break;
            case HttpMethod::Delete:
                reply = m_nam->deleteResource(netReq);
                break;
        }

        return new QtHttpOperation(reply);
    }

} // namespace network
