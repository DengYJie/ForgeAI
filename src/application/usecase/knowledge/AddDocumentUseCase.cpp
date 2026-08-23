#include "AddDocumentUseCase.h"

namespace application::usecase::knowledge {
    AddDocumentUseCase::AddDocumentUseCase(QObject *parent)
        : QObject(parent) {
    }

    bool AddDocumentUseCase::execute(const QString &docPath) {
        if (docPath.trimmed().isEmpty()) {
            emit documentAddFailed(docPath, QStringLiteral("文件路径为空"));
            return false;
        }
        // 占位导入逻辑
        emit documentAdded(docPath);
        return true;
    }
} // namespace application::usecase::knowledge
