#pragma once
#include "domain/llm/ChatError.h"
#include "RetryPolicy.h"
#include <QString>

namespace llm::runtime {

    enum class RetryAction {
        DoNotRetry,
        RetryAfter
    };

    struct RetryDecision {
        RetryAction action = RetryAction::DoNotRetry;
        int delayMs = 0;
        QString reason;

        static RetryDecision evaluate(
            const domain::llm::ChatError& error,
            const RetryPolicy& policy,
            int currentAttempt,
            bool hasEmittedVisibleTokens);
    };

} // namespace llm::runtime
