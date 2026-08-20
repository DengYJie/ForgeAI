#include "SettingsRegistry.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>

namespace core::settings {
    SettingsRegistry &SettingsRegistry::instance() {
        static SettingsRegistry registry;
        return registry;
    }

    SettingsRegistry::SettingsRegistry(QObject *parent) : QObject(parent) {
        m_saveTimer = new QTimer(this);
        m_saveTimer->setSingleShot(true);
        connect(m_saveTimer, &QTimer::timeout, this, &SettingsRegistry::flushDirtyProviders);
    }

    SettingsRegistry::~SettingsRegistry() {
        if (m_saveTimer &&m_saveTimer->isActive())
        {
            m_saveTimer->stop();
            flushDirtyProviders();
        }
    }

    QString SettingsRegistry::getConfigDirPath() const {
#ifdef QT_DEBUG
        return QCoreApplication::applicationDirPath();
#else
        return QDir::homePath() + "/.config/forgeai";
#endif
    }

    QString SettingsRegistry::getMainConfigPath() const {
        return getConfigDirPath() + "/config.json";
    }

    void SettingsRegistry::registerProvider(std::shared_ptr<ISettingsProvider> provider) {
        if (!provider) return;
        m_providers.insert(provider->id(), provider);
        connect(provider.get(), &ISettingsProvider::dataChanged, this, &SettingsRegistry::onProviderDataChanged);
    }

    std::shared_ptr<ISettingsProvider> SettingsRegistry::getProvider(const QString &id) const {
        return m_providers.value(id);
    }

    void SettingsRegistry::loadAll() {
        QString mainConfigPath = getMainConfigPath();
        QFile file(mainConfigPath);
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            m_cachedMainConfig = doc.object();
            file.close();
        } else {
            m_cachedMainConfig = QJsonObject();
        }

        for (const auto &provider: m_providers) {
            if (provider->useSeparateFile()) {
                QString path = getConfigDirPath() + "/" + provider->configFileName();
                QFile sepFile(path);
                if (sepFile.open(QIODevice::ReadOnly)) {
                    QJsonDocument doc = QJsonDocument::fromJson(sepFile.readAll());
                    provider->fromJson(doc.object());
                } else {
                    provider->fromJson(QJsonObject());
                }
            } else {
                QJsonObject obj = m_cachedMainConfig.value(provider->id()).toObject();
                provider->fromJson(obj);
            }
        }
    }

    void SettingsRegistry::onProviderDataChanged() {
        auto *provider = qobject_cast<ISettingsProvider *>(sender());
        if (provider) {
            m_dirtyProviders.insert(provider);
            m_saveTimer->start(500); // 500ms debounce
        }
    }

    void SettingsRegistry::flushDirtyProviders() {
        if (m_dirtyProviders.isEmpty()) return;

        QDir dir;
        dir.mkpath(getConfigDirPath());

        bool mainConfigDirty = false;
        QMap<QString, QJsonObject> separateConfigs;

        for (auto *provider: m_dirtyProviders) {
            if (provider->useSeparateFile()) {
                QJsonObject obj;
                QString path = getConfigDirPath() + "/" + provider->configFileName();
                QFile sepFile(path);
                if (sepFile.open(QIODevice::ReadOnly)) {
                    obj = QJsonDocument::fromJson(sepFile.readAll()).object();
                    sepFile.close();
                }
                provider->saveToJson(obj);
                separateConfigs[path] = obj;
            } else {
                QJsonObject obj = m_cachedMainConfig.value(provider->id()).toObject();
                provider->saveToJson(obj);
                m_cachedMainConfig.insert(provider->id(), obj);
                mainConfigDirty = true;
            }
        }

        if (mainConfigDirty) {
            QSaveFile saveFile(getMainConfigPath());
            if (saveFile.open(QIODevice::WriteOnly)) {
                QJsonDocument doc(m_cachedMainConfig);
                saveFile.write(doc.toJson());
                saveFile.commit();
            }
        }

        for (auto it = separateConfigs.begin(); it != separateConfigs.end(); ++it) {
            QSaveFile saveFile(it.key());
            if (saveFile.open(QIODevice::WriteOnly)) {
                QJsonDocument doc(it.value());
                saveFile.write(doc.toJson());
                saveFile.commit();
            }
        }

        m_dirtyProviders.clear();
    }

    void SettingsRegistry::saveAllSync() {
        if (m_saveTimer &&m_saveTimer->isActive())
        {
            m_saveTimer->stop();
        }
        for (const auto &provider: m_providers) {
            m_dirtyProviders.insert(provider.get());
        }
        flushDirtyProviders();
    }
} // namespace core::settings
