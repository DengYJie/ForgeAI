#include "ProviderListModel.h"

#include <algorithm>

#include <QFile>
#include <QIcon>

namespace ui::screen::settings::model_manager {

    ProviderListModel::ProviderListModel(QObject* parent)
        : QAbstractListModel(parent) {}

    int ProviderListModel::rowCount(const QModelIndex& parent) const {
        if (parent.isValid()) return 0;
        return static_cast<int>(m_providers.size());
    }

    QVariant ProviderListModel::data(const QModelIndex& index, int role) const {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_providers.size()) {
            return {};
        }

        const auto& provider = m_providers.at(index.row());
        switch (role) {
        case Qt::DisplayRole:
        case ProviderNameRole:
            return provider.name;
        case ProviderIdRole:
            return provider.id;
        case ProviderProtocolRole:
            return static_cast<int>(provider.protocol);
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

    void ProviderListModel::setProviders(const QList<domain::model::ModelProvider>& providers) {
        if (m_providers == providers) return;

        // Editing provider settings must not reset the view: a reset invalidates
        // ListView's current index, which can make it select the first row.
        // The model structure is unchanged as long as the provider IDs and their
        // order are unchanged, so update those rows in place instead.
        const bool sameStructure = m_providers.size() == providers.size()
            && std::equal(m_providers.cbegin(), m_providers.cend(), providers.cbegin(),
                [](const domain::model::ModelProvider& current, const domain::model::ModelProvider& next) {
                    return current.id == next.id;
                });
        if (sameStructure) {
            for (int row = 0; row < providers.size(); ++row) {
                if (m_providers.at(row) == providers.at(row)) continue;

                m_providers[row] = providers.at(row);
                const QModelIndex changedIndex = index(row, 0);
                Q_EMIT dataChanged(changedIndex, changedIndex, {
                    Qt::DisplayRole,
                    Qt::DecorationRole,
                    ProviderNameRole,
                    ProviderProtocolRole,
                    ProviderEnabledRole,
                    ProviderIconPathRole,
                    });
            }
            return;
        }

        beginResetModel();
        m_providers = providers;
        endResetModel();
    }

    std::optional<domain::model::ModelProvider> ProviderListModel::providerAt(int row) const {
        if (row < 0 || row >= m_providers.size()) return std::nullopt;
        return m_providers.at(row);
    }

    std::optional<domain::model::ModelProvider> ProviderListModel::findProvider(const QString& providerId) const {
        for (const auto& p : m_providers) {
            if (p.id == providerId) return p;
        }
        return std::nullopt;
    }

    int ProviderListModel::rowForProvider(const QString& providerId) const {
        for (int i = 0; i < m_providers.size(); ++i) {
            if (m_providers.at(i).id == providerId) return i;
        }
        return -1;
    }

} // namespace ui::screen::settings::model_manager
