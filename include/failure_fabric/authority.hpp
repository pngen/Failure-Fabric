#pragma once
// Authority envelope + fencing. Copyright 2026 Summon Software Labs.
#include "failure_fabric/id.hpp"
#include "failure_fabric/enum.hpp"
#include <string>

namespace ff {

// Every failure and recovery action is fenced by the relevant authority
// envelope. The envelope is a conjunction of independent generation/boot
// dimensions; because it is not a single total order, staleness is evaluated
// per dimension by the fence_* helpers below.
struct AuthorityEnvelope {
  CoordinatorEpoch    coordinator_epoch{};
  WorkerBootId        worker_boot{};      // logical worker incarnation
  AttemptId           attempt{};          // logical attempt
  AttemptGeneration   attempt_generation{};
  DispatchId          dispatch{};
  OperationGeneration operation_generation{};
  FailureGeneration   failure_generation{};
  RecoveryGeneration  recovery_generation{};

  bool is_unset() const noexcept {
    return coordinator_epoch.is_unset() && worker_boot.is_null() &&
           attempt.is_null() && attempt_generation.is_unset() &&
           dispatch.is_null() && operation_generation.is_unset() &&
           failure_generation.is_unset() && recovery_generation.is_unset();
  }

  // A future-facing envelope derived from this one with a fresh worker boot
  // and incremented generations. Logical identity (attempt, dispatch) is kept
  // only where it remains valid; callers set the fields they need.
  AuthorityEnvelope next_boot(const WorkerBootId& fresh_boot) const {
    AuthorityEnvelope e = *this;
    e.worker_boot = fresh_boot;
    e.attempt_generation = e.attempt_generation.next();
    e.operation_generation = e.operation_generation.next();
    e.failure_generation = e.failure_generation.next();
    e.recovery_generation = e.recovery_generation.next();
    return e;
  }
};

// Per-dimension reason a proposed authority event was rejected or accepted.
enum class FenceReason : uint32_t {
  ACCEPTED, EPOCH_STALE, BOOT_STALE, ATTEMPT_STALE, ATTEMPT_MISMATCH,
  DISPATCH_MISMATCH, ATTEMPT_GENERATION_STALE, OPERATION_GENERATION_STALE,
  FAILURE_GENERATION_STALE, RECOVERY_GENERATION_STALE, RECOVERY_OWNER_STALE,
  TERMINAL, UNKNOWN
};

struct FenceResult {
  bool accepted = false;
  AuthorityStatus status = AuthorityStatus::UNKNOWN;
  FenceReason reason = FenceReason::UNKNOWN;
  std::string detail;
};

// Fence a COMPLETION event (a worker's final report) against current state.
// A completion that carries older attempt/boot/generation authority than the
// current authoritative state is rejected as stale and must not resurrect dead
// authority (e.g. a late completion from a pre-kill worker).
FenceResult fence_completion(const AuthorityEnvelope& candidate,
                             const AuthorityEnvelope& current,
                             const AuthorityEnvelope& terminal_authority);

// Fence a FAILURE event.
FenceResult fence_failure(const AuthorityEnvelope& candidate,
                          const AuthorityEnvelope& current);

// Fence a RECOVERY command. Recovery commands only mutate state when their
// recovery generation strictly increases against the current owner and the
// listed boot/epoch authority is still live.
FenceResult fence_recovery(const AuthorityEnvelope& candidate,
                           const AuthorityEnvelope& current,
                           RecoveryGeneration current_recovery_generation);

} // namespace ff
