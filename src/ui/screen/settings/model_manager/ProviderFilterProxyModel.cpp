#include "ProviderFilterProxyModel.h"
#include "ProviderListModel.h"

namespace ui::screen::settings::model_manager {

    ProviderFilterProxyModel::ProviderFilterProxyModel(QObject *parent)
        : QSortFilterProxyModel(parent) {
        setFilterCaseSensitivity(Qt::CaseInsensitive);
    }

    void ProviderFilterProxyModel::setSearchKeyword(const QString &keyword) {
        const QString trimmed = keyword.trimmed();
        if (m_keyword == trimmed) return;

        m_keyword = trimmed;
        invalidateFilter();
    }

    bool ProviderFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
        if (m_keyword.isEmpty()) return true;

        const auto *srcModel = sourceModel();
        if (!srcModel) return true;

        const QModelIndex idx = srcModel->index(sourceRow, 0, sourceParent);
        const QString name = idx.data(ProviderNameRole).toString();
        return name.contains(m_keyword, Qt::CaseInsensitive);
    }

} // namespace ui::screen::settings::model_manager
