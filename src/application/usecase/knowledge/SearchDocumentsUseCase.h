#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace application::usecase::knowledge {
    /**
     * @brief 检索知识库文档用例
     */
    class SearchDocumentsUseCase : public QObject {
        Q_OBJECT

    public:
        explicit SearchDocumentsUseCase(QObject *parent = nullptr);
        ~SearchDocumentsUseCase() override = default;

        /**
         * @brief 执行文档检索
         * @param query 检索关键词
         * @return 匹配的文档列表
         */
        QStringList execute(const QString &query);

    Q_SIGNALS:
        void searchCompleted(const QString &query, const QStringList &results);
    };
} // namespace application::usecase::knowledge
