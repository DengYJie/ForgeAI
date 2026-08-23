#include "KnowledgeViewModel.h"

namespace ui::screen::knowledge {
    KnowledgeViewModel::KnowledgeViewModel(
        const application::usecase::knowledge::KnowledgeUseCases &useCases,
        QObject *parent
    ) : BaseViewModel<KnowledgeViewModel, KnowledgeState>(parent),
        m_useCases(useCases) {
        setupUseCaseConnections();
    }

    KnowledgeViewModel::~KnowledgeViewModel() = default;

    void KnowledgeViewModel::setupUseCaseConnections() {
        if (m_useCases.searchDocuments) {
            connect(m_useCases.searchDocuments, &application::usecase::knowledge::SearchDocumentsUseCase::searchCompleted,
                    this, [this](const QString &query, const QStringList &results) {
                updateState([query, results](KnowledgeState &s) {
                    s.searchQuery = query;
                    s.statusMessage = QStringLiteral("找到 ") + QString::number(results.size()) + QStringLiteral(" 条匹配结果");
                });
            });
        }

        if (m_useCases.addDocument) {
            connect(m_useCases.addDocument, &application::usecase::knowledge::AddDocumentUseCase::documentAdded,
                    this, [this](const QString &docPath) {
                updateState([](KnowledgeState &s) {
                    s.documentCount += 1;
                    s.statusMessage = QStringLiteral("文档已成功索引");
                });
            });

            connect(m_useCases.addDocument, &application::usecase::knowledge::AddDocumentUseCase::documentAddFailed,
                    this, [this](const QString &docPath, const QString &reason) {
                updateState([reason](KnowledgeState &s) {
                    s.statusMessage = QStringLiteral("文档索引失败: ") + reason;
                });
            });
        }
    }

    void KnowledgeViewModel::setSearchQuery(const QString &query) {
        updateState([query](KnowledgeState &s) { s.searchQuery = query; });
        if (m_useCases.searchDocuments) {
            m_useCases.searchDocuments->execute(query);
        }
    }

    void KnowledgeViewModel::addDocument(const QString &docPath) {
        if (docPath.isEmpty()) return;

        if (m_useCases.addDocument) {
            m_useCases.addDocument->execute(docPath);
        } else {
            updateState([](KnowledgeState &s) {
                s.documentCount += 1;
                s.statusMessage = QStringLiteral("文档已索引");
            });
        }
    }

    void KnowledgeViewModel::emitStateChanged() {
        Q_EMIT stateChanged(m_state);
    }
} // namespace ui::screen::knowledge
