#pragma once
// Recovery ownership and deterministic recovery plan generation.
// Copyright 2026 Summon Software Labs.
#include "failure_fabric/enum.hpp"
#include "failure_fabric/id.hpp"
#include "failure_fabric/authority.hpp"
#include "failure_fabric/failure_record.hpp"
#include <vector>
#include <string>

namespace ff {

// Only one current authoritative recovery owner may mutate recovery state.
// Ownership transfers strictly increase the recovery generation; a dead owner
// loses authority on restart because the holder boot is part of the fence.
class RecoveryOwnership {
public:
  RecoveryOwner owner() const { return owner_; }
  RecoveryGeneration generation() const { return generation_; }
  WorkerBootId holder_boot() const { return boot_; }
  bool is_unassigned() const { return owner_ == RecoveryOwner::NONE; }

  // Adopt ownership for the first time (generation must be nonzero).
  bool adopt(RecoveryOwner owner, RecoveryGeneration gen, const WorkerBootId& boot,
             const AuthorityEnvelope& authority);
  // Transfer to a new owner; generation must strictly increase.
  bool transfer(RecoveryOwner owner, RecoveryGeneration gen, const WorkerBootId& boot,
                const AuthorityEnvelope& authority);
  // Verify the current holder (boot + generation) is still authoritative.
  bool is_current(const WorkerBootId& boot, RecoveryGeneration gen) const {
    return boot == boot_ && gen == generation_;
  }

private:
  RecoveryOwner owner_ = RecoveryOwner::NONE;
  RecoveryGeneration generation_{};
  WorkerBootId boot_{};
};

struct RecoveryPlan {
  RecoveryPlanId     plan_id{};
  OperationId        operation{};
  RecoveryGeneration generation{};
  RecoveryOwner      owner = RecoveryOwner::COORDINATOR;
  AuthorityEnvelope  authority{};
  RecoveryAction     action = RecoveryAction::RETRY;
  std::vector<std::string> preconditions;
  std::vector<RecoveryAction> rejected_alternatives;
  std::vector<std::string> rejection_reasons;
  std::vector<std::string> reason_factors;
  std::string expected_preserved;
  std::string expected_discarded;
};

// Inputs the planner considers. All deterministic; no opaque scoring.
struct PlanInput {
  uint32_t attempt_count = 0;
  uint32_t max_attempts = 3;
  bool worker_healthy = true;
  bool device_healthy = true;
  bool idempotent_retry_possible = false;
  bool rollback_possible = false;
  bool compensation_possible = false;
  bool deadline_exceeded = false;
  bool externally_committed = false;
  bool dependency_ready = true;
};

// Deterministically derives a recovery plan from a committed failure record.
class RecoveryPlanner {
public:
  RecoveryPlan plan(const FailureRecord& fail, const PlanInput& in) const;
};

} // namespace ff
