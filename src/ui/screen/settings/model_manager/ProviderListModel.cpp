#include "ProviderListModel.h"

#include <QIcon>
#include <QFile>

namespace ui::screen::settings::model_manager {

    ProviderListModel::ProviderListModel(QObject *parent)
        : QAbstractListModel(parent) {}

    int ProviderListModel::rowCount(const QModelIndex &parent) const {
        if (parent.isValid()) return 0;
        return static_cast<int>(m_providers.size());
    }

    QVariant ProviderListModel::data(const QModelIndex &index, int role) const {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_providers.size()) {
            return {};
        }

        const auto &provider = m_providers.at(index.row());
        switch (role) {
        case Qt::DisplayRole:
        case ProviderNameRole:
            return provider.name;
        case ProviderIdRole:
            return provider.id;
        case ProviderTypeRole:
            return static_cast<int>(provider.type);
        case ProviderEnabledRole:
            return provider.isEnabled;
        case ProviderIconPathRole:
            return provider.icon;
        case Qt::DecorationRole:
            if (!provider.icon.isEmpty()) {
                if (QFile::exists(provider.icon)) {
                    return QIcon(provider.icon);
                }
            }
            return {};
        default:
            return {};
        }
    }

    void ProviderListModel::setProviders(const QList<domain::model::ModelProvider> &providers) {
        beginResetModel();
        m_providers = providers;
        endResetModel();
    }

    std::optional<domain::model::ModelProvider> ProviderListModel::providerAt(int row) const {
        if (row < 0 || row >= m_providers.size()) return std::nullopt;
        return m_providers.at(row);
    }

    std::optional<domain::model::ModelProvider> ProviderListModel::findProvider(const QString &providerId) const {
        for (const auto &p : m_providers) {
            if (p.id == providerId) return p;
        }
        return std::nullopt;
    }

    int ProviderListModel::rowForProvider(const QString &providerId) const {
        for (int i = 0; i < m_providers.size(); ++i) {
            if (m_providers.at(i).id == providerId) return i;
        }
        return -1;
    }

} // namespace ui::screen::settings::model_manager
