#pragma once

#include "ui/base/BaseViewModel.h"
#include "application/usecase/knowledge/KnowledgeUseCases.h"
#include <QString>

namespace ui::screen::knowledge {
    struct KnowledgeState {
        int documentCount = 0;
        QString searchQuery;
        QString statusMessage;

        bool operator==(const KnowledgeState &other) const = default;
    };

    /**
     * @brief 知识库界面的 ViewModel，负责文档索引与检索状态响应
     */
    class KnowledgeViewModel : public BaseViewModel<KnowledgeViewModel, KnowledgeState> {
        Q_OBJECT

    public:
        explicit KnowledgeViewModel(
            const application::usecase::knowledge::KnowledgeUseCases &useCases = {},
            QObject *parent = nullptr
        );

        ~KnowledgeViewModel() override;

        /**
         * @brief 设置检索词并执行搜索
         */
        void setSearchQuery(const QString &query);

        /**
         * @brief 导入并索引文档
         */
        void addDocument(const QString &docPath);

    Q_SIGNALS:
        void stateChanged(const ui::screen::knowledge::KnowledgeState &state);

    protected:
        void emitStateChanged() override;

    private:
        void setupUseCaseConnections();

        application::usecase::knowledge::KnowledgeUseCases m_useCases;
    };
} // namespace ui::screen::knowledge
