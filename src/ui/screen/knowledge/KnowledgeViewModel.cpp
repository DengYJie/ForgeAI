#include "KnowledgeViewModel.h"

namespace ui::screen::knowledge {
    KnowledgeViewModel::KnowledgeViewModel(QObject *parent)
        : BaseViewModel<KnowledgeViewModel, KnowledgeState>(parent) {
    }

    KnowledgeViewModel::~KnowledgeViewModel() = default;

    void KnowledgeViewModel::setSearchQuery(const QString &query) {
        updateState([query](KnowledgeState &s) { s.searchQuery = query; });
    }

    void KnowledgeViewModel::addDocument(const QString &docPath) {
        if (docPath.isEmpty()) return;

        updateState([](KnowledgeState &s) {
            s.documentCount += 1;
            s.statusMessage = QStringLiteral("文档已索引");
        });
    }

    void KnowledgeViewModel::emitStateChanged() {
        Q_EMIT stateChanged(m_state);
    }
} // namespace ui::screen::knowledge
