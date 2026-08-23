#pragma once

#include <QObject>
#include <QString>

namespace application::usecase::knowledge {
    /**
     * @brief 导入知识库文档用例
     */
    class AddDocumentUseCase : public QObject {
        Q_OBJECT

    public:
        explicit AddDocumentUseCase(QObject *parent = nullptr);
        ~AddDocumentUseCase() override = default;

        /**
         * @brief 执行添加/导入文档
         * @param docPath 文档文件路径
         * @return 是否导入成功
         */
        bool execute(const QString &docPath);

    Q_SIGNALS:
        void documentAdded(const QString &docPath);
        void documentAddFailed(const QString &docPath, const QString &reason);
    };
} // namespace application::usecase::knowledge
