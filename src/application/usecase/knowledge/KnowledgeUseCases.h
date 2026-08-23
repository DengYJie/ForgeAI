#pragma once

#include "application/usecase/knowledge/SearchDocumentsUseCase.h"
#include "application/usecase/knowledge/AddDocumentUseCase.h"

namespace application::usecase::knowledge {
    /**
     * @brief 知识库界面业务用例聚合容器
     */
    struct KnowledgeUseCases {
        SearchDocumentsUseCase *searchDocuments = nullptr;
        AddDocumentUseCase *addDocument = nullptr;
    };
} // namespace application::usecase::knowledge
