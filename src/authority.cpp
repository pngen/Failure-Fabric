#include "failure_fabric/authority.hpp"

namespace ff {
namespace {
FenceResult accepted(const char* detail) {
  return FenceResult{true, AuthorityStatus::AUTHORITATIVE, FenceReason::ACCEPTED, detail};
}
FenceResult rejected(FenceReason r, const char* detail) {
  return FenceResult{false, AuthorityStatus::STALE, r, detail};
}
}

FenceResult fence_completion(const AuthorityEnvelope& c,
                             const AuthorityEnvelope& cur,
                             const AuthorityEnvelope& terminal) {
  // Guard against unset candidate.
  if (c.is_unset()) return rejected(FenceReason::UNKNOWN, "completion carries no authority envelope");
  // Coordinator epoch must not be older than the authoritative epoch.
  if (!cur.coordinator_epoch.is_unset() && c.coordinator_epoch < cur.coordinator_epoch)
    return rejected(FenceReason::EPOCH_STALE, "completion epoch older than authoritative epoch");
  // Logical attempt and dispatch must match the attempt being completed.
  if (!cur.attempt.is_null() && c.attempt != cur.attempt)
    return rejected(FenceReason::ATTEMPT_MISMATCH, "completion references a different attempt id");
  if (!cur.dispatch.is_null() && c.dispatch != cur.dispatch)
    return rejected(FenceReason::DISPATCH_MISMATCH, "completion references a different dispatch id");
  // A completion that does not come from the worker boot that currently owns
  // the attempt is a stale/late completion and must not resurrect authority.
  if (!cur.worker_boot.is_null() && c.worker_boot != cur.worker_boot)
    return rejected(FenceReason::BOOT_STALE, "completion from a dead/stale worker boot");
  // Generations must not regress below the current authoritative generation.
  if (!cur.attempt_generation.is_unset() && c.attempt_generation < cur.attempt_generation)
    return rejected(FenceReason::ATTEMPT_GENERATION_STALE, "completion attempt generation regressed");
  if (!cur.operation_generation.is_unset() && c.operation_generation < cur.operation_generation)
    return rejected(FenceReason::OPERATION_GENERATION_STALE, "completion operation generation regressed");
  // A terminal outcome already recorded at equal or higher generations means
  // this completion is not strictly newer and is stale.
  if (!terminal.is_unset()) {
    bool not_newer = c.attempt_generation <= terminal.attempt_generation &&
                     c.operation_generation <= terminal.operation_generation;
    if (not_newer)
      return rejected(FenceReason::ATTEMPT_GENERATION_STALE, "terminal outcome already recorded at this/newer generation");
  }
  return accepted("completion authority is current");
}

FenceResult fence_failure(const AuthorityEnvelope& c, const AuthorityEnvelope& cur) {
  if (c.is_unset()) return rejected(FenceReason::UNKNOWN, "failure carries no authority envelope");
  if (!cur.coordinator_epoch.is_unset() && c.coordinator_epoch < cur.coordinator_epoch)
    return rejected(FenceReason::EPOCH_STALE, "failure epoch older than authoritative epoch");
  if (!cur.attempt.is_null() && c.attempt != cur.attempt)
    return rejected(FenceReason::ATTEMPT_MISMATCH, "failure references a different attempt id");
  if (!cur.attempt_generation.is_unset() && c.attempt_generation < cur.attempt_generation)
    return rejected(FenceReason::ATTEMPT_GENERATION_STALE, "failure attempt generation regressed");
  // A failure from an old boot that has already been superseded is stale; a
  // failure from a *new* boot describing current work is accepted.
  if (!cur.worker_boot.is_null() && !c.worker_boot.is_null() && c.worker_boot != cur.worker_boot) {
    if (c.attempt_generation <= cur.attempt_generation)
      return rejected(FenceReason::BOOT_STALE, "failure from a stale worker boot");
  }
  return accepted("failure authority is current");
}

FenceResult fence_recovery(const AuthorityEnvelope& c,
                           const AuthorityEnvelope& cur,
                           RecoveryGeneration current_recovery_generation) {
  if (c.is_unset()) return rejected(FenceReason::UNKNOWN, "recovery command carries no authority envelope");
  if (!cur.coordinator_epoch.is_unset() && c.coordinator_epoch < cur.coordinator_epoch)
    return rejected(FenceReason::EPOCH_STALE, "recovery epoch older than authoritative epoch");
  // Recovery mutation requires a strictly increasing recovery generation.
  if (c.recovery_generation <= current_recovery_generation)
    return rejected(FenceReason::RECOVERY_GENERATION_STALE, "recovery generation did not increase");
  if (!cur.attempt.is_null() && c.attempt != cur.attempt)
    return rejected(FenceReason::ATTEMPT_MISMATCH, "recovery references a different attempt id");
  return accepted("recovery authority is current");
}
} // namespace ff
