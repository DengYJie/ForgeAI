#pragma once

#include <QHash>
#include <QImage>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>

class QNetworkReply;

namespace ui::markdown {

class MarkdownImageResourceManager final : public QObject {
    Q_OBJECT
public:
    explicit MarkdownImageResourceManager(QObject* parent = nullptr);

    void setNetworkAccessEnabled(bool enabled);
    bool networkAccessEnabled() const noexcept;
    const QHash<QString, QImage>& images() const noexcept;
    void clear();
    void request(const QString& source, const QUrl& baseUrl);

signals:
    void imageUpdated(const QString& source);

private:
    QString resolveKey(const QString& source, const QUrl& baseUrl) const;
    void loadLocal(const QString& key);
    void loadRemote(const QString& key);

    QNetworkAccessManager m_network;
    QHash<QString, QImage> m_images;
    QMultiHash<QString, QString> m_aliases;
    QHash<QNetworkReply*, QString> m_pending;
    bool m_networkAccessEnabled = true;
};

} // namespace ui::markdown
