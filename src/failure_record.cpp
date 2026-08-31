#include "failure_fabric/failure_record.hpp"
#include "failure_fabric/id.hpp"
#include <sstream>

namespace ff {
namespace {
struct Dims { Retryability retry; RollbackRequirement rollback; CompensationRequirement comp; Terminality term; };
Dims dims_for(FailureClass c) {
  using FC = FailureClass;
  switch (c) {
    case FC::TRANSIENT:            return {Retryability::RETRYABLE, RollbackRequirement::NONE, CompensationRequirement::NONE, Terminality::TRANSIENT};
    case FC::PERMANENT:            return {Retryability::NON_RETRYABLE, RollbackRequirement::OPTIONAL, CompensationRequirement::OPTIONAL, Terminality::TERMINAL};
    case FC::RETRYABLE:            return {Retryability::RETRYABLE, RollbackRequirement::NONE, CompensationRequirement::NONE, Terminality::TRANSIENT};
    case FC::NON_RETRYABLE:        return {Retryability::NON_RETRYABLE, RollbackRequirement::OPTIONAL, CompensationRequirement::OPTIONAL, Terminality::TERMINAL};
    case FC::AMBIGUOUS:            return {Retryability::CONDITIONALLY_RETRYABLE, RollbackRequirement::OPTIONAL, CompensationRequirement::OPTIONAL, Terminality::PENDING};
    case FC::PARTIAL_SUCCESS:      return {Retryability::CONDITIONALLY_RETRYABLE, RollbackRequirement::REQUIRED, CompensationRequirement::REQUIRED, Terminality::PENDING};
    case FC::STALE_AUTHORITY:      return {Retryability::NON_RETRYABLE, RollbackRequirement::NONE, CompensationRequirement::NONE, Terminality::TERMINAL};
    case FC::LOST_WORKER:          return {Retryability::CONDITIONALLY_RETRYABLE, RollbackRequirement::OPTIONAL, CompensationRequirement::OPTIONAL, Terminality::PENDING};
    case FC::LOST_DEVICE:          return {Retryability::CONDITIONALLY_RETRYABLE, RollbackRequirement::OPTIONAL, CompensationRequirement::OPTIONAL, Terminality::PENDING};
    case FC::TRANSPORT_FAILURE:    return {Retryability::CONDITIONALLY_RETRYABLE, RollbackRequirement::NONE, CompensationRequirement::NONE, Terminality::TRANSIENT};
    case FC::TIMEOUT_OBSERVED:     return {Retryability::CONDITIONALLY_RETRYABLE, RollbackRequirement::NONE, CompensationRequirement::NONE, Terminality::TRANSIENT};
    case FC::PROTOCOL_FAILURE:     return {Retryability::NON_RETRYABLE, RollbackRequirement::NONE, CompensationRequirement::NONE, Terminality::TRANSIENT};
    case FC::RESOURCE_EXHAUSTION:  return {Retryability::RETRYABLE, RollbackRequirement::NONE, CompensationRequirement::NONE, Terminality::TRANSIENT};
    case FC::CORRUPTION:           return {Retryability::NON_RETRYABLE, RollbackRequirement::REQUIRED, CompensationRequirement::REQUIRED, Terminality::TERMINAL};
    case FC::VALIDATION_FAILURE:   return {Retryability::NON_RETRYABLE, RollbackRequirement::NONE, CompensationRequirement::NONE, Terminality::TERMINAL};
    case FC::INCOMPATIBILITY:      return {Retryability::NON_RETRYABLE, RollbackRequirement::OPTIONAL, CompensationRequirement::OPTIONAL, Terminality::TERMINAL};
    case FC::CANCELLED:            return {Retryability::NON_RETRYABLE, RollbackRequirement::OPTIONAL, CompensationRequirement::OPTIONAL, Terminality::TERMINAL};
    case FC::PREEMPTED:            return {Retryability::RETRYABLE, RollbackRequirement::NONE, CompensationRequirement::NONE, Terminality::TRANSIENT};
    case FC::DEPENDENCY_FAILURE:   return {Retryability::CONDITIONALLY_RETRYABLE, RollbackRequirement::NONE, CompensationRequirement::NONE, Terminality::TRANSIENT};
    case FC::STORAGE_FAILURE:      return {Retryability::CONDITIONALLY_RETRYABLE, RollbackRequirement::NONE, CompensationRequirement::NONE, Terminality::TRANSIENT};
    case FC::EXECUTION_FAILURE:    return {Retryability::RETRYABLE, RollbackRequirement::NONE, CompensationRequirement::NONE, Terminality::TRANSIENT};
    case FC::RECOVERY_FAILURE:     return {Retryability::NON_RETRYABLE, RollbackRequirement::OPTIONAL, CompensationRequirement::OPTIONAL, Terminality::PENDING};
    case FC::UNKNOWN:              return {Retryability::UNKNOWN, RollbackRequirement::NONE, CompensationRequirement::NONE, Terminality::UNKNOWN};
  }
  return {Retryability::UNKNOWN, RollbackRequirement::NONE, CompensationRequirement::NONE, Terminality::UNKNOWN};
}
}

void derive_dimensions(FailureRecord& rec) {
  Dims d = dims_for(rec.failure_class);
  if (rec.retryability == Retryability::UNKNOWN) rec.retryability = d.retry;
  // rollback/comp: only move NONE->derived default, never downgrade an explicit value
  if (rec.rollback == RollbackRequirement::NONE) rec.rollback = d.rollback;
  if (rec.compensation == CompensationRequirement::NONE) rec.compensation = d.comp;
  if (rec.terminality == Terminality::UNKNOWN) rec.terminality = d.term;
}

void FailureRecordBuilder::derive_defaults() { derive_dimensions(r); }

FailureRecord FailureRecordBuilder::build() const { std::mt19937_64 rng(std::random_device{}()); return build(rng); }

FailureRecord FailureRecordBuilder::build(std::mt19937_64& rng) const {
  FailureRecord out = r;
  // apply derived defaults only for dimensions the caller did not set
  Dims d = dims_for(out.failure_class);
  if (!retry_set_ && out.retryability == Retryability::UNKNOWN) out.retryability = d.retry;
  if (!rollback_set_ && out.rollback == RollbackRequirement::NONE) out.rollback = d.rollback;
  if (!comp_set_ && out.compensation == CompensationRequirement::NONE) out.compensation = d.comp;
  if (!term_set_ && out.terminality == Terminality::UNKNOWN) out.terminality = d.term;
  if (out.failure_id.is_null()) out.failure_id = FailureId::generate(rng);
  return out;
}

std::string to_json(const FailureRecord& r) {
  std::ostringstream o;
  o << '{' << '\n';

  // helper: emits "key":value,  (with comma)
  const char* K = nullptr;
  (void)K;
  o << '"' << "failure_id" << '"' << ':' << '"' << r.failure_id.to_hex() << '"' << ',' << '\n';
  o << '"' << "operation" << '"' << ':' << '"' << r.operation.to_hex() << '"' << ',' << '\n';
  o << '"' << "request" << '"' << ':' << '"' << r.request.to_hex() << '"' << ',' << '\n';
  o << '"' << "workload" << '"' << ':' << '"' << r.workload.to_hex() << '"' << ',' << '\n';
  o << '"' << "attempt" << '"' << ':' << '"' << r.attempt.to_hex() << '"' << ',' << '\n';
  o << '"' << "dispatch" << '"' << ':' << '"' << r.dispatch.to_hex() << '"' << ',' << '\n';
  o << '"' << "worker" << '"' << ':' << '"' << r.worker.to_hex() << '"' << ',' << '\n';
  o << '"' << "node" << '"' << ':' << '"' << r.node.to_hex() << '"' << ',' << '\n';
  o << '"' << "device" << '"' << ':' << '"' << r.device.to_hex() << '"' << ',' << '\n';
  o << '"' << "failure_class" << '"' << ':' << '"' << to_string(r.failure_class) << '"' << ',' << '\n';
  o << '"' << "origin" << '"' << ':' << '"' << to_string(r.origin) << '"' << ',' << '\n';
  o << '"' << "failure_code" << '"' << ':' << r.failure_code << ',' << '\n';
  o << '"' << "phase" << '"' << ':' << '"' << to_string(r.phase) << '"' << ',' << '\n';
  o << '"' << "timestamp_ms" << '"' << ':' << r.timestamp_ms << ',' << '\n';
  o << '"' << "completion" << '"' << ':' << '"' << to_string(r.completion) << '"' << ',' << '\n';
  o << '"' << "side_effect_state" << '"' << ':' << '"' << to_string(r.side_effect_state) << '"' << ',' << '\n';
  o << '"' << "unknown_side_effects" << '"' << ':' << r.unknown_side_effects << ',' << '\n';
  o << '"' << "retryability" << '"' << ':' << '"' << to_string(r.retryability) << '"' << ',' << '\n';
  o << '"' << "rollback" << '"' << ':' << '"' << to_string(r.rollback) << '"' << ',' << '\n';
  o << '"' << "compensation" << '"' << ':' << '"' << to_string(r.compensation) << '"' << ',' << '\n';
  o << '"' << "terminality" << '"' << ':' << '"' << to_string(r.terminality) << '"' << ',' << '\n';
  o << '"' << "recovery_owner" << '"' << ':' << '"' << to_string(r.recovery_owner) << '"' << ',' << '\n';
  o << '"' << "provenance" << '"' << ':' << '"' << to_string(r.provenance) << '"' << '\n';
  o << '}' << '\n';
  return o.str();
}
} // namespace ff