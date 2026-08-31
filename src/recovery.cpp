#include "failure_fabric/recovery.hpp"

namespace ff {
bool RecoveryOwnership::adopt(RecoveryOwner owner, RecoveryGeneration gen, const WorkerBootId& boot,
                              const AuthorityEnvelope& authority) {
  if (owner == RecoveryOwner::NONE) return false;
  if (gen.is_unset()) return false;
  if (owner_ != RecoveryOwner::NONE) return false; // already assigned
  if (!authority.coordinator_epoch.is_unset() &&
      !authority.coordinator_epoch.is_unset()) { /* epoch check done by caller */ }
  owner_ = owner;
  generation_ = gen;
  boot_ = boot;
  return true;
}

bool RecoveryOwnership::transfer(RecoveryOwner owner, RecoveryGeneration gen, const WorkerBootId& boot,
                                 const AuthorityEnvelope& authority) {
  if (owner == RecoveryOwner::NONE) return false;
  if (gen.is_unset()) return false;
  if (owner_ == RecoveryOwner::NONE) return adopt(owner, gen, boot, authority);
  // Strict generation increase is the fencing rule for ownership transfer.
  if (!(generation_ < gen)) return false;
  owner_ = owner;
  generation_ = gen;
  boot_ = boot;
  return true;
}

namespace {
void reject(RecoveryPlan& p, RecoveryAction a, const char* why) {
  p.rejected_alternatives.push_back(a);
  p.rejection_reasons.push_back(why);
}
void factor(RecoveryPlan& p, const std::string& f) { p.reason_factors.push_back(f); }
}

RecoveryPlan RecoveryPlanner::plan(const FailureRecord& fail, const PlanInput& in) const {
  RecoveryPlan p;
  p.operation = fail.operation;
  p.authority = fail.authority;
  p.generation = fail.authority.recovery_generation.next();
  p.plan_id = RecoveryPlanId::generate();
  p.owner = fail.recovery_owner == RecoveryOwner::NONE ? RecoveryOwner::COORDINATOR : fail.recovery_owner;

  factor(p, std::string("failure_class=") + to_string(fail.failure_class));
  factor(p, std::string("completion=") + to_string(fail.completion));
  factor(p, std::string("side_effects=") + to_string(fail.side_effect_state));
  factor(p, std::string("attempts=") + std::to_string(in.attempt_count) + "/" + std::to_string(in.max_attempts));
  factor(p, std::string("idempotent=") + (in.idempotent_retry_possible ? "true" : "false"));

  const bool exhausted = in.attempt_count >= in.max_attempts;
  const bool ambiguous = fail.completion == Ambiguity::AMBIGUOUS || fail.completion == Ambiguity::UNKNOWN_OUTCOME;
  const bool may_effect = fail.side_effect_state == SideEffectState::MAY_OCCURRED ||
                          fail.side_effect_state == SideEffectState::UNKNOWN ||
                          fail.side_effect_state == SideEffectState::KNOWN_OCCURRED;

  // 1. Device / worker hardware losses drive restart. 
  if (fail.failure_class == FailureClass::LOST_DEVICE ||
      fail.failure_class == FailureClass::LOST_WORKER) {
    if (fail.failure_class == FailureClass::LOST_DEVICE && in.device_healthy) {
      p.action = RecoveryAction::RESTART_DEVICE_CONTEXT;
      p.preconditions.push_back("device healthy; reinit context");
      reject(p, RecoveryAction::RETRY, "device identity lost, retry in-place unsafe");
    } else if (in.worker_healthy) {
      p.action = RecoveryAction::RESTART_WORKER;
      p.preconditions.push_back("worker restart with fresh WorkerBootId");
      reject(p, RecoveryAction::COMPENSATE, "restart recovers state without compensation");
    } else {
      p.action = RecoveryAction::FAILOVER_REPLICA;
      p.preconditions.push_back("replica available");
    }
    p.expected_preserved = "authority fenced to fresh boot; original worker state discarded";
    p.expected_discarded = "stale worker boot authority";
    return p;
  }

  // 2. Ambiguous completion: side effects may already have happened.
  if (ambiguous) {
    if (in.idempotent_retry_possible && !exhausted) {
      p.action = RecoveryAction::RETRY;
      p.preconditions.push_back("idempotency key valid; retry same key is deduplicated");
      reject(p, RecoveryAction::ROLLBACK, "idempotent retry cannot double-apply; rollback unnecessary");
      reject(p, RecoveryAction::COMPENSATE, "idempotent retry restores correctness directly");
      p.expected_preserved = "prior result metadata preserved; no duplicate side effect";
      p.expected_discarded = "unknown-side-effect uncertainty";
    } else if (in.rollback_possible) {
      p.action = RecoveryAction::ROLLBACK;
      p.preconditions.push_back("side effects may have occurred; rollback first");
      reject(p, RecoveryAction::RETRY, "not idempotent-safe; retry would double-apply");
      p.expected_preserved = "state reverted to pre-operation";
    } else if (in.compensation_possible) {
      p.action = RecoveryAction::COMPENSATE;
      p.preconditions.push_back("side effect escaped; compensate logically");
      p.expected_preserved = "logical equilibrium restored";
    } else {
      p.action = RecoveryAction::MANUAL_RESOLUTION;
      p.preconditions.push_back("operator/external resolution required");
      p.expected_discarded = "uncertain";
    }
    return p;
  }

  // 3. Known side effects that already occurred.
  if (may_effect) {
    if (fail.side_effect_state == SideEffectState::KNOWN_OCCURRED && !in.idempotent_retry_possible) {
      if (in.rollback_possible) {
        p.action = RecoveryAction::ROLLBACK;
        p.preconditions.push_back("known side effect; rollback then recover");
        reject(p, RecoveryAction::RETRY, "would re-apply side effect");
      } else if (in.compensation_possible) {
        p.action = RecoveryAction::COMPENSATE;
      } else {
        p.action = RecoveryAction::ESCALATE;
      }
      return p;
    }
  }

  // 4. Retryable classes with bounded retry.
  const bool retryable_class = fail.retryability == Retryability::RETRYABLE ||
                              fail.retryability == Retryability::CONDITIONALLY_RETRYABLE;
  if (retryable_class && !exhausted && in.idempotent_retry_possible) {
    if (!in.dependency_ready) {
      p.action = RecoveryAction::RETRY;
      p.preconditions.push_back("dependency not ready; retry after dependency");
    } else if (!in.worker_healthy) {
      p.action = RecoveryAction::RETRY_ELSEWHERE;
      p.preconditions.push_back("worker unhealthy; retry on another worker");
    } else if (in.deadline_exceeded) {
      p.action = RecoveryAction::ESCALATE;
      p.preconditions.push_back("deadline exceeded; escalate");
    } else {
      p.action = RecoveryAction::RETRY;
      p.preconditions.push_back("transient; retry under fresh attempt");
    }
    p.expected_preserved = "previous partial state if any";
    p.expected_discarded = "failed attempt authority";
    return p;
  }

  // 5. Non-retryable / terminal after side effects.
  if (!retryable_class || exhausted) {
    if (fail.side_effect_state != SideEffectState::NONE_POSSIBLE &&
        fail.side_effect_state != SideEffectState::KNOWN_NOT_OCCURRED) {
      if (in.rollback_possible) {
        p.action = RecoveryAction::ROLLBACK;
        p.preconditions.push_back("terminal failure with side effects; rollback required");
        return p;
      }
    }
    if (in.externally_committed) {
      p.action = RecoveryAction::RECOMPUTE;
      p.preconditions.push_back("externally committed; recompute canonical result");
    } else if (fail.failure_class == FailureClass::RESOURCE_EXHAUSTION && !in.worker_healthy) {
      p.action = RecoveryAction::FAILOVER_REPLICA;
    } else if (fail.failure_class == FailureClass::RESOURCE_EXHAUSTION && in.worker_healthy) {
      p.action = RecoveryAction::RETRY_ELSEWHERE;
      p.preconditions.push_back("resource exhaustion; retry on healthy worker");
    } else {
      p.action = RecoveryAction::ABANDON;
      p.preconditions.push_back("terminal/non-retryable; abandon after cleanup");
      reject(p, RecoveryAction::RETRY, "policy exhausted or non-retryable");
    }
    p.expected_discarded = "operation marked terminal";
    return p;
  }

  p.action = RecoveryAction::ESCALATE;
  p.preconditions.push_back("unclassified failure; escalate");
  return p;
}

} // namespace ff