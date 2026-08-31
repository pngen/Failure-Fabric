// Example: rollback vs compensation. Copyright 2026 Summon Software Labs.
#include "failure_fabric/ff.hpp"
#include <cstdio>
using namespace ff;
int main() {
  // Rollback: ordered, authority-fenced, monotonic, resumable.
  RollbackPlan plan; plan.operation = OperationId::from_hex("cafe0001"); plan.generation = RecoveryGeneration(1);
  plan.steps.push_back({RollbackAction::RELEASE_RESERVATION, "res-1", true, false, false, ""});
  plan.steps.push_back({RollbackAction::FREE_ALLOCATION, "alloc-1", true, false, false, ""});
  plan.steps.push_back({RollbackAction::REVERT_METADATA, "meta-1", true, false, false, ""});
  RollbackExecutor ex(std::move(plan));
  while (!ex.is_complete()) { if (!ex.run_next(true)) { std::printf("rollback blocked/failed: %s\n", ex.failure_reason().c_str()); break; } }
  std::printf("rollback complete=%d (monotonic, resumable)\n", ex.is_complete() ? 1 : 0);
  // Compensation: distinct from rollback (logical restore when physical undo impossible).
  CompensationRecord cr; cr.compensation_id = CompensationId::generate(); cr.target_operation = OperationId::from_hex("cafe0002");
  cr.reason = "external side effect escaped; accounting adjustment required";
  Compensator comp(std::move(cr)); comp.run(true);
  std::printf("compensation complete=%d (distinct from rollback)\n", comp.is_complete() ? 1 : 0);
  return ex.is_complete() && comp.is_complete() ? 0 : 1;
}