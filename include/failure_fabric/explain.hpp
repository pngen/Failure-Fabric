#pragma once
// Deterministic explainability: text and JSON explanations.
// Copyright 2026 Summon Software Labs.
#include "failure_fabric/failure_record.hpp"
#include "failure_fabric/retry.hpp"
#include "failure_fabric/recovery.hpp"
#include <string>

namespace ff {

// Deterministic human-readable explanation of why a failure was classified this
// way, whether completion is known/ambiguous, whether retry/rollback/compensation
// is required, who owns recovery, and which authority permits the action.
std::string explain_failure(const FailureRecord& rec);

// Deterministic explanation of a retry + recovery-plan decision (selected plan,
// rejected alternatives, reason factors, preserved/discarded state).
std::string explain_decision(const FailureRecord& rec, const RetryDecision& retry,
                            const RecoveryPlan& plan);

// Deterministic JSON encoding of the same explanation.
std::string explain_failure_json(const FailureRecord& rec);
std::string explain_decision_json(const FailureRecord& rec, const RetryDecision& retry,
                                  const RecoveryPlan& plan);

std::string quote_json(const std::string& s);
} // namespace ff
