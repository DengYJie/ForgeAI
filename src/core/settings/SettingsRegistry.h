#pragma once
#include <QObject>
#include <QMap>
#include <QSet>
#include <QTimer>
#include <QJsonObject>
#include <memory>
#include "ISettingsProvider.h"

class QFileSystemWatcher;

namespace core::settings {
    class SettingsRegistry : public QObject {
        Q_OBJECT

    public:
        static SettingsRegistry &instance();

        void registerProvider(std::shared_ptr<ISettingsProvider> provider);

        std::shared_ptr<ISettingsProvider> getProvider(const QString &id) const;

        void loadAll();

        void saveAllSync();

    private slots:
        void onProviderDataChanged();

        void flushDirtyProviders();

        void onFileSystemChanged();

        void doReload();

    private:
        SettingsRegistry(QObject *parent = nullptr);

        ~SettingsRegistry() override;

        QString getMainConfigPath() const;

        QString getConfigDirPath() const;

        QMap<QString, std::shared_ptr<ISettingsProvider> > m_providers;
        QSet<ISettingsProvider *> m_dirtyProviders;

        QJsonObject m_cachedMainConfig;
        QTimer *m_saveTimer = nullptr;
        QTimer *m_reloadTimer = nullptr;
        QFileSystemWatcher *m_watcher = nullptr;
        bool m_isSaving = false;
    };
} // namespace core::settings
