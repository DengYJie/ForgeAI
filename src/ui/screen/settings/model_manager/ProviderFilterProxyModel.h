#pragma once

#include <QSortFilterProxyModel>

namespace ui::screen::settings::model_manager {

    /**
     * @brief 服务商实时搜索过滤代理模型
     */
    class ProviderFilterProxyModel : public QSortFilterProxyModel {
        Q_OBJECT

    public:
        explicit ProviderFilterProxyModel(QObject *parent = nullptr);
        ~ProviderFilterProxyModel() override = default;

        void setSearchKeyword(const QString &keyword);

    protected:
        bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

    private:
        QString m_keyword;
    };

} // namespace ui::screen::settings::model_manager
