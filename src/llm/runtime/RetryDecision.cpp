#include "RetryDecision.h"
#include <cmath>
#include <QRandomGenerator>

namespace llm::runtime {

    RetryDecision RetryDecision::evaluate(
        const domain::llm::ChatError &error,
        const RetryPolicy &policy,
        int currentAttempt,
        bool hasEmittedVisibleTokens) {
        
        RetryDecision decision;

        if (!policy.enabled) {
            decision.action = RetryAction::DoNotRetry;
            decision.reason = QStringLiteral("RetryPolicy disabled");
            return decision;
        }

        if (currentAttempt >= policy.maxAttempts) {
            decision.action = RetryAction::DoNotRetry;
            decision.reason = QStringLiteral("Max attempts limit reached (%1/%2)").arg(currentAttempt).arg(policy.maxAttempts);
            return decision;
        }

        if (hasEmittedVisibleTokens) {
            decision.action = RetryAction::DoNotRetry;
            decision.reason = QStringLiteral("Visible tokens already emitted, automatic retry disallowed to prevent duplicated output");
            return decision;
        }

        // 不可重试的错误分类
        switch (error.category) {
            case domain::llm::ChatErrorCategory::Cancelled:
            case domain::llm::ChatErrorCategory::Authentication:
            case domain::llm::ChatErrorCategory::Authorization:
            case domain::llm::ChatErrorCategory::Request:
            case domain::llm::ChatErrorCategory::Context:
            case domain::llm::ChatErrorCategory::Quota:
            case domain::llm::ChatErrorCategory::Model:
            case domain::llm::ChatErrorCategory::Configuration:
                decision.action = RetryAction::DoNotRetry;
                decision.reason = QStringLiteral("Error category is non-retryable");
                return decision;
            default:
                break;
        }

        // 优先采纳服务端 Retry-After
        if (error.retryAfterSeconds > 0) {
            decision.action = RetryAction::RetryAfter;
            decision.delayMs = error.retryAfterSeconds * 1000;
            decision.reason = QStringLiteral("Provider specified retry-after delay");
            return decision;
        }

        // 判定是否属于可安全重试的错误分类
        bool isRetryable = error.retryable ||
                           error.category == domain::llm::ChatErrorCategory::Network ||
                           error.category == domain::llm::ChatErrorCategory::Timeout ||
                           error.category == domain::llm::ChatErrorCategory::Provider ||
                           error.category == domain::llm::ChatErrorCategory::RateLimit;

        if (isRetryable) {
            // 计算指数退避: baseDelay * (backoffFactor ^ (attempt - 1))
            double exponent = std::max(0, currentAttempt - 1);
            double calculatedDelay = policy.baseDelayMs * std::pow(policy.backoffFactor, exponent);

            // 加入随机抖动 Jitter (+/- jitterFactor)
            if (policy.jitterFactor > 0.0) {
                double jitterRange = calculatedDelay * policy.jitterFactor;
                double randomOffset = QRandomGenerator::global()->generateDouble() * 2.0 * jitterRange - jitterRange;
                calculatedDelay += randomOffset;
            }

            // 限制在 [baseDelayMs, maxDelayMs]
            int finalDelay = static_cast<int>(std::clamp(calculatedDelay, static_cast<double>(policy.baseDelayMs), static_cast<double>(policy.maxDelayMs)));

            decision.action = RetryAction::RetryAfter;
            decision.delayMs = finalDelay;
            decision.reason = QStringLiteral("Calculated exponential backoff retry");
            return decision;
        }

        decision.action = RetryAction::DoNotRetry;
        decision.reason = QStringLiteral("Error is not retryable");
        return decision;
    }

} // namespace llm::runtime
