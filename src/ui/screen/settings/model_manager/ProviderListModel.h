#pragma once

#include <QAbstractListModel>
#include <QList>
#include <optional>
#include "domain/model/ModelProvider.h"

namespace ui::screen::settings::model_manager {

    enum ProviderRoles {
        ProviderIdRole = Qt::UserRole + 1,
        ProviderNameRole,
        ProviderProtocolRole,
        ProviderEnabledRole,
        ProviderIconPathRole
    };

    /**
     * @brief 服务商列表数据模型
     */
    class ProviderListModel : public QAbstractListModel {
        Q_OBJECT

    public:
        explicit ProviderListModel(QObject *parent = nullptr);
        ~ProviderListModel() override = default;

        int rowCount(const QModelIndex &parent = QModelIndex()) const override;
        QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

        void setProviders(const QList<domain::model::ModelProvider> &providers);
        const QList<domain::model::ModelProvider> &providers() const { return m_providers; }

        std::optional<domain::model::ModelProvider> providerAt(int row) const;
        std::optional<domain::model::ModelProvider> findProvider(const QString &providerId) const;
        int rowForProvider(const QString &providerId) const;

    private:
        QList<domain::model::ModelProvider> m_providers;
    };

} // namespace ui::screen::settings::model_manager
