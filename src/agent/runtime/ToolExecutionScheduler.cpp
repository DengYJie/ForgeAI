#include "ToolExecutionScheduler.h"

namespace agent::runtime {

    QList<QList<domain::agent::ToolCall>> ToolExecutionScheduler::scheduleBatches(
        const QList<domain::agent::ToolCall>& calls,
        const agent::tool::ToolRegistry* registry,
        bool allowParallel
    ) {
        QList<QList<domain::agent::ToolCall>> batches;
        if (calls.isEmpty()) return batches;

        if (!allowParallel) {
            for (const auto& call : calls) {
                batches.append({call});
            }
            return batches;
        }

        struct BatchMeta {
            QList<domain::agent::ToolCall> calls;
            QSet<QString> usedConcurrencyKeys;
            bool hasExclusiveTool = false;
        };

        QList<BatchMeta> metaBatches;

        for (const auto& call : calls) {
            application::ports::ToolExecutionTraits traits;
            if (registry) {
                auto tool = registry->findTool(call.name);
                if (tool) {
                    traits = tool->traits();
                }
            }

            const bool isParallelizable = traits.parallelizable;
            const QString& key = traits.concurrencyKey;

            int targetBatchIdx = -1;

            if (isParallelizable) {
                for (int i = 0; i < metaBatches.size(); ++i) {
                    auto& bm = metaBatches[i];
                    if (bm.hasExclusiveTool) continue;
                    if (!key.isEmpty() && bm.usedConcurrencyKeys.contains(key)) continue;

                    targetBatchIdx = i;
                    break;
                }
            }

            if (targetBatchIdx == -1) {
                BatchMeta newBatch;
                newBatch.calls.append(call);
                if (!key.isEmpty()) {
                    newBatch.usedConcurrencyKeys.insert(key);
                }
                newBatch.hasExclusiveTool = !isParallelizable;
                metaBatches.append(newBatch);
            } else {
                auto& bm = metaBatches[targetBatchIdx];
                bm.calls.append(call);
                if (!key.isEmpty()) {
                    bm.usedConcurrencyKeys.insert(key);
                }
            }
        }

        for (const auto& bm : metaBatches) {
            batches.append(bm.calls);
        }

        return batches;
    }

} // namespace agent::runtime
