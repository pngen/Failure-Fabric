#include "failure_fabric/explain.hpp"
#include "failure_fabric/authority.hpp"
#include <sstream>

namespace ff {
std::string quote_json(const std::string& s) {
  std::string o = "\"";
  for (char ch : s) {
    switch (ch) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      default: o += ch;
    }
  }
  o += "\"";
  return o;
}

static const char* boolstr(bool b) { return b ? "true" : "false"; }

std::string explain_failure(const FailureRecord& rec) {
  std::ostringstream o;
  o << "Failure " << rec.failure_id.to_hex() << " on operation " << rec.operation.to_hex()
    << " attempt " << rec.attempt.to_hex() << " classified " << to_string(rec.failure_class)
    << " (origin " << to_string(rec.origin) << ", code " << rec.failure_code << ", phase "
    << to_string(rec.phase) << ").\n";
  o << "  completion status: " << to_string(rec.completion) << "\n";
  o << "  side-effect state: " << to_string(rec.side_effect_state)
    << " (unknown side effects: " << rec.unknown_side_effects << ")\n";
  o << "  retryability: " << to_string(rec.retryability)
    << "; rollback requirement: " << to_string(rec.rollback)
    << "; compensation requirement: " << to_string(rec.compensation)
    << "; terminality: " << to_string(rec.terminality) << "\n";
  o << "  recovery owner: " << to_string(rec.recovery_owner) << "\n";
  o << "  authority: epoch=" << rec.authority.coordinator_epoch.value
    << " attempt_gen=" << rec.authority.attempt_generation.value
    << " op_gen=" << rec.authority.operation_generation.value
    << " fail_gen=" << rec.authority.failure_generation.value
    << " recovery_gen=" << rec.authority.recovery_generation.value << "\n";
  o << "  summary: " << rec.summary;
  if (!rec.reason.empty()) o << "  reason: " << rec.reason;
  return o.str();
}

std::string explain_failure_json(const FailureRecord& rec) {
  std::ostringstream o;
  o << "{";
  o << "\"failure_id\":" << quote_json(rec.failure_id.to_hex()) << ",";
  o << "\"operation\":" << quote_json(rec.operation.to_hex()) << ",";
  o << "\"attempt\":" << quote_json(rec.attempt.to_hex()) << ",";
  o << "\"failure_class\":" << quote_json(to_string(rec.failure_class)) << ",";
  o << "\"origin\":" << quote_json(to_string(rec.origin)) << ",";
  o << "\"completion\":" << quote_json(to_string(rec.completion)) << ",";
  o << "\"side_effect_state\":" << quote_json(to_string(rec.side_effect_state)) << ",";
  o << "\"retryability\":" << quote_json(to_string(rec.retryability)) << ",";
  o << "\"rollback\":" << quote_json(to_string(rec.rollback)) << ",";
  o << "\"compensation\":" << quote_json(to_string(rec.compensation)) << ",";
  o << "\"terminality\":" << quote_json(to_string(rec.terminality)) << ",";
  o << "\"recovery_owner\":" << quote_json(to_string(rec.recovery_owner)) << ",";
  o << "\"unknown_side_effects\":" << rec.unknown_side_effects << ",";
  o << "\"summary\":" << quote_json(rec.summary);
  o << "}";
  return o.str();
}

std::string explain_decision(const FailureRecord& rec, const RetryDecision& retry, const RecoveryPlan& plan) {
  (void)rec;
  std::ostringstream o;
  o << "Retry decision: " << to_string(retry.verdict)
    << (retry.same_idempotency_key ? " (same idempotency key)" : "")
    << (retry.requires_fresh_attempt ? " (fresh attempt required)" : "")
    << ". Reason: " << retry.reason << "\n";
  o << "Recovery plan id " << plan.plan_id.to_hex() << " selected action "
    << to_string(plan.action) << " (generation " << plan.generation.value << ").\n";
  o << "  expected preserved: " << plan.expected_preserved << "\n";
  o << "  expected discarded: " << plan.expected_discarded << "\n";
  o << "  rejected alternatives:";
  for (size_t i = 0; i < plan.rejected_alternatives.size(); ++i) {
    o << " " << to_string(plan.rejected_alternatives[i]);
    if (i < plan.rejection_reasons.size()) o << " (" << plan.rejection_reasons[i] << ")";
  }
  o << "\n  reason factors:";
  for (auto& f : plan.reason_factors) o << " [" << f << "]";
  o << "\n";
  return o.str();
}

std::string explain_decision_json(const FailureRecord& rec, const RetryDecision& retry, const RecoveryPlan& plan) {
  (void)rec;
  std::ostringstream o;
  o << "{";
  o << "\"verdict\":" << quote_json(to_string(retry.verdict)) << ",";
  o << "\"fresh_attempt\":" << boolstr(retry.requires_fresh_attempt) << ",";
  o << "\"same_key\":" << boolstr(retry.same_idempotency_key) << ",";
  o << "\"rollback_first\":" << boolstr(retry.rollback_first) << ",";
  o << "\"recovery_action\":" << quote_json(to_string(plan.action)) << ",";
  o << "\"recovery_generation\":" << plan.generation.value << ",";
  o << "\"reason\":" << quote_json(retry.reason) << ",";
  o << "\"expected_preserved\":" << quote_json(plan.expected_preserved) << ",";
  o << "\"expected_discarded\":" << quote_json(plan.expected_discarded);
  o << "}";
  return o.str();
}
} // namespace ff