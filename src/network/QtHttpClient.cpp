#include "QtHttpClient.h"
#include "core/logging/LoggingService.h"
#include "core/logging/LogCategory.h"
#include "core/logging/SensitiveDataFilter.h"
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
        int httpStatusCode = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        core::logging::LoggingService::instance().debug(core::logging::Category::NetworkHttp, QStringLiteral("HTTP finished"), QMap<QString, QString>{
            {QStringLiteral("httpStatus"), QString::number(httpStatusCode)},
            {QStringLiteral("error"), QString::number(static_cast<int>(m_reply->error()))}
        });
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
        core::logging::LoggingService::instance().warning(core::logging::Category::NetworkHttp, QStringLiteral("HTTP error occurred"), QMap<QString, QString>{
            {QStringLiteral("httpStatus"), QString::number(httpStatusCode)},
            {QStringLiteral("code"), QString::number(static_cast<int>(code))},
            {QStringLiteral("error"), errorMsg},
            {QStringLiteral("body"), QString::fromUtf8(responseBody.left(512))}
        });
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

        QString methodStr = QStringLiteral("GET");
        QNetworkReply *reply = nullptr;
        switch (request.method) {
            case HttpMethod::Get:
                methodStr = QStringLiteral("GET");
                reply = m_nam->get(netReq);
                break;
            case HttpMethod::Post:
                methodStr = QStringLiteral("POST");
                reply = m_nam->post(netReq, request.body);
                break;
            case HttpMethod::Put:
                methodStr = QStringLiteral("PUT");
                reply = m_nam->put(netReq, request.body);
                break;
            case HttpMethod::Delete:
                methodStr = QStringLiteral("DELETE");
                reply = m_nam->deleteResource(netReq);
                break;
        }

        QString cleanUrl = core::logging::SensitiveDataFilter::sanitizeUrl(request.url);
        core::logging::LoggingService::instance().debug(core::logging::Category::NetworkHttp, QStringLiteral("HTTP dispatch"), {
            {QStringLiteral("method"), methodStr},
            {QStringLiteral("url"), cleanUrl},
            {QStringLiteral("bytes"), QString::number(request.body.size())}
        });

        return new QtHttpOperation(reply);
    }

} // namespace network
