#include "SettingsRegistry.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QFileSystemWatcher>

namespace core::settings {
    SettingsRegistry &SettingsRegistry::instance() {
        static SettingsRegistry registry;
        return registry;
    }

    SettingsRegistry::SettingsRegistry(QObject *parent) : QObject(parent) {
        m_saveTimer = new QTimer(this);
        m_saveTimer->setSingleShot(true);
        connect(m_saveTimer, &QTimer::timeout, this, &SettingsRegistry::flushDirtyProviders);

        m_reloadTimer = new QTimer(this);
        m_reloadTimer->setSingleShot(true);
        connect(m_reloadTimer, &QTimer::timeout, this, &SettingsRegistry::doReload);

        m_watcher = new QFileSystemWatcher(this);
        connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &SettingsRegistry::onFileSystemChanged);
        connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &SettingsRegistry::onFileSystemChanged);
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
        QDir dir;
        dir.mkpath(getConfigDirPath());
        if (!m_watcher->directories().contains(getConfigDirPath())) {
            m_watcher->addPath(getConfigDirPath());
        }

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

        m_isSaving = true;

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

        QTimer::singleShot(200, this, [this]() {
            m_isSaving = false;
        });
    }

    void SettingsRegistry::onFileSystemChanged() {
        if (m_isSaving) return;
        m_reloadTimer->start(100);
    }

    void SettingsRegistry::doReload() {
        if (m_isSaving) return;

        QFile file(getMainConfigPath());
        QJsonObject newMainConfig;

        if (file.exists() && file.open(QIODevice::ReadOnly)) {
            newMainConfig = QJsonDocument::fromJson(file.readAll()).object();
            m_cachedMainConfig = newMainConfig;
            file.close();
        } else if (!file.exists()) {
            m_cachedMainConfig = QJsonObject();
            // newMainConfig is already empty
        }

        for (const auto &provider: m_providers) {
            if (m_dirtyProviders.contains(provider.get())) {
                continue;
            }

            if (provider->useSeparateFile()) {
                QString path = getConfigDirPath() + "/" + provider->configFileName();
                QFile sepFile(path);
                if (sepFile.exists() && sepFile.open(QIODevice::ReadOnly)) {
                    provider->fromJson(QJsonDocument::fromJson(sepFile.readAll()).object());
                }
            } else {
                provider->fromJson(newMainConfig.value(provider->id()).toObject());
            }
        }
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
