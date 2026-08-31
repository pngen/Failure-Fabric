#pragma once
// Retry classification with bounded retries and explicit policy.
// Copyright 2026 Summon Software Labs.
#include "failure_fabric/enum.hpp"
#include "failure_fabric/failure_record.hpp"
#include "failure_fabric/id.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace ff {

enum class RetryVerdict : uint32_t { RETRY, DO_NOT_RETRY, RETRY_ELSEWHERE, ROLLBACK_FIRST };

struct RetryDecision {
  RetryVerdict verdict = RetryVerdict::DO_NOT_RETRY;
  bool requires_fresh_attempt = true;   // a retry must be a fresh AttemptId/Generation
  bool same_idempotency_key = true;     // retry under the same key (safe only when idempotent)
  bool rollback_first = false;
  bool compensation_required = false;
  bool fenced_permanently = false;
  std::string reason;
  std::vector<std::string> factors;
};

struct RetryPolicy {
  uint32_t max_attempts = 3;                  // hard bound; never unbounded
  bool allow_retry_unknown_side_effects = false;
  bool require_same_key = true;
  bool allow_retry_elsewhere = true;
  bool allow_retry_after_ambiguous = true;
  uint32_t backoff_ms_base = 10;
  uint32_t backoff_ms_max = 5000;
};

// Inputs the retry classifier needs. Supplied by the runtime/integration.
struct RetryInput {
  uint32_t attempt_count = 0;
  bool worker_healthy = true;
  bool device_healthy = true;
  bool dependency_ready = true;
  bool reservation_active = true;
  bool deadline_exceeded = false;
  bool idempotent_retry_possible = false;   // operation has a valid idempotency key & policy
};

class RetryClassifier {
public:
  RetryDecision classify(const FailureRecord& fail, const RetryInput& in, const RetryPolicy& policy) const;
  // deterministic backoff for attempt n (0-based). Monotonic; bounded by max.
  uint32_t backoff_ms(uint32_t attempt) const;
};

} // namespace ff
