#pragma once

#include "ui/base/BaseViewModel.h"
#include <QString>

namespace ui::screen::knowledge {
    struct KnowledgeState {
        int documentCount = 0;
        QString searchQuery;
        QString statusMessage;

        bool operator==(const KnowledgeState &other) const = default;
    };

    class KnowledgeViewModel : public BaseViewModel<KnowledgeViewModel, KnowledgeState> {
        Q_OBJECT

    public:
        explicit KnowledgeViewModel(QObject *parent = nullptr);

        ~KnowledgeViewModel() override;

        void setSearchQuery(const QString &query);

        void addDocument(const QString &docPath);

    Q_SIGNALS:
        void stateChanged(const ui::screen::knowledge::KnowledgeState &state);

    protected:
        void emitStateChanged() override;
    };
} // namespace ui::screen::knowledge
