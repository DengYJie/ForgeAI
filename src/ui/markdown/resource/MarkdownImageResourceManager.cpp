#include "MarkdownImageResourceManager.h"

#include <QImageReader>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace ui::markdown {

MarkdownImageResourceManager::MarkdownImageResourceManager(QObject* parent)
    : QObject(parent)
{}

void MarkdownImageResourceManager::setNetworkAccessEnabled(bool enabled)
{
    m_networkAccessEnabled = enabled;
}

bool MarkdownImageResourceManager::networkAccessEnabled() const noexcept { return m_networkAccessEnabled; }
const QHash<QString, QImage>& MarkdownImageResourceManager::images() const noexcept { return m_images; }

void MarkdownImageResourceManager::clear()
{
    m_images.clear();
    m_aliases.clear();
    for (QNetworkReply* reply : m_pending.keys()) reply->abort();
    m_pending.clear();
}

QString MarkdownImageResourceManager::resolveKey(const QString& source, const QUrl& baseUrl) const
{
    if (source.startsWith(QStringLiteral(":"))) return source;
    QUrl url(source);
    if (url.isRelative() && baseUrl.isValid()) url = baseUrl.resolved(url);
    return url.isValid() ? url.toString() : source;
}

void MarkdownImageResourceManager::request(const QString& source, const QUrl& baseUrl)
{
    if (source.isEmpty()) return;
    const QString key = resolveKey(source, baseUrl);
    m_aliases.insert(key, source);
    if (m_images.contains(key) || m_pending.values().contains(key)) return;
    const QUrl url(key);
    if (key.startsWith(QStringLiteral(":")) || url.isLocalFile() || url.scheme().isEmpty()) loadLocal(key);
    else if ((url.scheme() == u"http" || url.scheme() == u"https") && m_networkAccessEnabled) loadRemote(key);
}

void MarkdownImageResourceManager::loadLocal(const QString& key)
{
    QString path = key;
    if (key.startsWith(QStringLiteral("file://"))) {
        path = QUrl(key).toLocalFile();
    }
    QImage image(path);
    if (!image.isNull()) {
        m_images.insert(key, image);
        for (const QString& alias : m_aliases.values(key)) m_images.insert(alias, image);
        emit imageUpdated(key);
    }
}

void MarkdownImageResourceManager::loadRemote(const QString& key)
{
    auto* reply = m_network.get(QNetworkRequest{QUrl(key)});
    m_pending.insert(reply, key);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const QString key = m_pending.take(reply);
        if (reply->error() == QNetworkReply::NoError) {
            QImage image;
            image.loadFromData(reply->readAll());
            if (!image.isNull()) {
                m_images.insert(key, image);
                for (const QString& alias : m_aliases.values(key)) m_images.insert(alias, image);
                emit imageUpdated(key);
            }
        }
        reply->deleteLater();
    });
}

} // namespace ui::markdown
