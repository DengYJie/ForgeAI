#include "SearchDocumentsUseCase.h"

namespace application::usecase::knowledge {
    SearchDocumentsUseCase::SearchDocumentsUseCase(QObject *parent)
        : QObject(parent) {
    }

    QStringList SearchDocumentsUseCase::execute(const QString &query) {
        QStringList results;
        // 占位检索逻辑
        emit searchCompleted(query, results);
        return results;
    }
} // namespace application::usecase::knowledge
