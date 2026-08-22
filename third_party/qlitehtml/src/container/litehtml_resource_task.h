#pragma once

#include "qlitehtml_types.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QMetaObject>
#include <QNetworkReply>
#include <QPointer>
#include <QRunnable>
#include <QUrl>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

namespace qlitehtml::internal {

/**
 * @brief Thread-safe cancellation token and network handle context for an in-flight resource task.
 */
struct ResourceTaskContext
{
    std::shared_ptr<std::atomic<bool>> cancelToken = std::make_shared<std::atomic<bool>>(false);
    QPointer<QNetworkReply> reply;
};

/**
 * @brief Unified resource request parameters.
 */
struct ResourceRequest
{
    QUrl url;
    ResourceType type = ResourceType::Image;
    int priority = 0;             ///< Higher integer = higher priority in QThreadPool.
    bool allowNetwork = false;    ///< Explicit opt-in flag for external HTTP(S) access.
};

/**
 * @brief Unified stateless resource fetcher supporting custom handlers, data:, qrc:, file:, and http(s): protocols.
 */
class GenericResourceFetcher
{
public:
    using ChunkCallback = std::function<void(const QByteArray &cumulativeData)>;

    /**
     * @brief Fetches raw bytes for the given request, routing to the appropriate protocol handler.
     * @param req The resource request parameters.
     * @param handler Optional user-provided resource handler (checked first).
     * @param ctx Task cancellation context and reply tracking.
     * @param onChunk Optional callback invoked when streaming chunks are received (e.g. for header sniffing).
     * @return The raw byte payload, or an empty QByteArray if fetching failed, timed out, or was cancelled.
     */
    static QByteArray fetchRawData(
        const ResourceRequest &req,
        const ResourceHandler &handler,
        const std::shared_ptr<ResourceTaskContext> &ctx,
        const ChunkCallback &onChunk = nullptr);
};

/**
 * @brief Generic asynchronous resource pipeline task executed on a QThreadPool worker.
 * @tparam ResultType The type produced by the post-processor (e.g. QImage, QString, etc.).
 */
template <typename ResultType>
class GenericResourceTask : public QRunnable
{
public:
    using PostProcessor = std::function<ResultType(const QByteArray &data, const std::shared_ptr<ResourceTaskContext> &ctx)>;
    using CompletionCallback = std::function<void(const QByteArray &rawData, ResultType &&result)>;

    GenericResourceTask(ResourceRequest req,
                        ResourceHandler handler,
                        std::shared_ptr<ResourceTaskContext> ctx,
                        GenericResourceFetcher::ChunkCallback onChunk,
                        PostProcessor processor,
                        CompletionCallback onComplete)
        : m_req(std::move(req))
        , m_handler(std::move(handler))
        , m_ctx(std::move(ctx))
        , m_onChunk(std::move(onChunk))
        , m_processor(std::move(processor))
        , m_onComplete(std::move(onComplete))
    {}

    void run() override
    {
        if (!m_ctx || m_ctx->cancelToken->load())
            return;

        // Step 1: Fetch raw payload across all supported protocols
        QByteArray data = GenericResourceFetcher::fetchRawData(m_req, m_handler, m_ctx, m_onChunk);
        if (m_ctx->cancelToken->load())
            return;

        // Step 2: Execute type-specific post-processing / decoding on worker thread
        ResultType result = m_processor ? m_processor(data, m_ctx) : ResultType();
        if (m_ctx->cancelToken->load())
            return;

        // Step 3: Schedule completion callback safely back to the GUI main thread
        auto ctx = m_ctx;
        auto cb = m_onComplete;
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [ctx, cb, data, res = std::move(result)]() mutable {
                if (ctx && !ctx->cancelToken->load() && cb) {
                    cb(data, std::move(res));
                }
            },
            Qt::QueuedConnection);
    }

private:
    ResourceRequest m_req;
    ResourceHandler m_handler;
    std::shared_ptr<ResourceTaskContext> m_ctx;
    GenericResourceFetcher::ChunkCallback m_onChunk;
    PostProcessor m_processor;
    CompletionCallback m_onComplete;
};

} // namespace qlitehtml::internal
