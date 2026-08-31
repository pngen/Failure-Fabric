#include "failure_fabric/retry.hpp"
#include "failure_fabric/authority.hpp"

namespace ff {
RetryDecision RetryClassifier::classify(const FailureRecord& fail, const RetryInput& in, const RetryPolicy& policy) const {
  RetryDecision d;
  d.reason = "DO_NOT_RETRY by default";
  d.factors.push_back(std::string("failure_class=") + to_string(fail.failure_class));
  d.factors.push_back(std::string("completion=") + to_string(fail.completion));
  d.factors.push_back(std::string("side_effects=") + to_string(fail.side_effect_state));
  d.factors.push_back(std::string("attempts=") + std::to_string(in.attempt_count) + "/" + std::to_string(policy.max_attempts));

  // Hard bound: never retry past the policy limit (no unbounded loops).
  if (in.attempt_count >= policy.max_attempts) {
    d.verdict = RetryVerdict::DO_NOT_RETRY;
    d.requires_fresh_attempt = false;
    d.reason = "attempt limit reached; bounded retries exhausted";
    return d;
  }

  const bool ambiguous = fail.completion == Ambiguity::AMBIGUOUS || fail.completion == Ambiguity::UNKNOWN_OUTCOME;
  const bool may_effect = fail.side_effect_state == SideEffectState::MAY_OCCURRED ||
                          fail.side_effect_state == SideEffectState::UNKNOWN;

  // Non-retryable classes are terminal; we never retry them.
  switch (fail.failure_class) {
    case FailureClass::PERMANENT:
    case FailureClass::NON_RETRYABLE:
    case FailureClass::VALIDATION_FAILURE:
    case FailureClass::INCOMPATIBILITY:
    case FailureClass::CANCELLED:
    case FailureClass::RECOVERY_FAILURE:
    case FailureClass::STALE_AUTHORITY:
      d.verdict = RetryVerdict::DO_NOT_RETRY;
      d.requires_fresh_attempt = false;
      d.fenced_permanently = true;
      d.reason = std::string("non-retryable failure class: ") + to_string(fail.failure_class);
      return d;
    default: break;
  }

  // Ambiguous completion: retry only if idempotent-safe, else rollback first.
  if (ambiguous) {
    if (!in.idempotent_retry_possible) {
      if (fail.rollback == RollbackRequirement::REQUIRED || may_effect) {
        d.verdict = RetryVerdict::ROLLBACK_FIRST;
        d.rollback_first = true;
        d.requires_fresh_attempt = true;
        d.reason = "ambiguous completion with possible side effects; rollback before retry";
      } else {
        d.verdict = RetryVerdict::DO_NOT_RETRY;
        d.reason = "ambiguous completion and not idempotent-safe; manual resolution";
      }
      return d;
    }
    // idempotent-safe -> retry under the same key, fresh attempt generation.
    d.verdict = RetryVerdict::RETRY;
    d.same_idempotency_key = policy.require_same_key;
    d.requires_fresh_attempt = true;
    d.reason = "ambiguous but idempotent; retry same key with fresh attempt";
    return d;
  }

  // Known side effects occurred and not idempotent -> rollback first, no retry.
  if (fail.side_effect_state == SideEffectState::KNOWN_OCCURRED && !in.idempotent_retry_possible) {
    d.verdict = RetryVerdict::ROLLBACK_FIRST;
    d.rollback_first = true;
    d.requires_fresh_attempt = false;
    d.reason = "known side effect occurred; rollback required before any retry";
    return d;
  }

  // Unknown/may-have side effects: only retry if policy allows unknown side effects.
  if (may_effect && !policy.allow_retry_unknown_side_effects) {
    d.verdict = RetryVerdict::ROLLBACK_FIRST;
    d.rollback_first = true;
    d.requires_fresh_attempt = true;
    d.reason = "unknown side effects; policy requires rollback before retry";
    return d;
  }

  // Retryable class, idempotency not required for known-no-side-effect failures.
  const bool retryable_class = fail.retryability == Retryability::RETRYABLE ||
                              fail.retryability == Retryability::CONDITIONALLY_RETRYABLE;
  if (retryable_class) {
    if (in.deadline_exceeded) {
      d.verdict = RetryVerdict::DO_NOT_RETRY;
      d.reason = "deadline/SLO exceeded";
    } else if (!in.worker_healthy && policy.allow_retry_elsewhere) {
      d.verdict = RetryVerdict::RETRY_ELSEWHERE;
      d.requires_fresh_attempt = true;
      d.reason = "worker unhealthy; retry elsewhere";
    } else if (!in.dependency_ready) {
      d.verdict = RetryVerdict::DO_NOT_RETRY;
      d.reason = "dependency not ready";
    } else {
      d.verdict = RetryVerdict::RETRY;
      d.requires_fresh_attempt = true;
      d.reason = "transient/retryable; retry with fresh attempt";
    }
    return d;
  }

  d.verdict = RetryVerdict::DO_NOT_RETRY;
  d.reason = "unclassified; not retryable";
  return d;
}

uint32_t RetryClassifier::backoff_ms(uint32_t attempt) const {
  // exponential backoff: base * 2^attempt, capped. Deterministic.
  uint64_t v = 10ULL << (attempt < 12 ? attempt : 12);
  if (v > 5000ULL) v = 5000ULL;
  return static_cast<uint32_t>(v);
}
} // namespace ff
