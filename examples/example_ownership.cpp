// Example: recovery ownership transfer under generation fencing + stale authority.
// Copyright 2026 Summon Software Labs.
#include "failure_fabric/ff.hpp"
#include <cstdio>
using namespace ff;
int main() {
  std::mt19937_64 rng(3);
  RecoveryOwnership own;
  AuthorityEnvelope a; a.coordinator_epoch=CoordinatorEpoch(1); a.attempt=AttemptId::generate(rng); a.attempt_generation=AttemptGeneration(1); a.worker_boot=WorkerBootId::generate(rng);
  own.adopt(RecoveryOwner::COORDINATOR, RecoveryGeneration(1), a.worker_boot, a);
  bool t = own.transfer(RecoveryOwner::REPLACEMENT_WORKER, RecoveryGeneration(2), WorkerBootId::generate(), a);
  std::printf("ownership transfer to replacement (gen2): %d, owner=%s\n", t, to_string(own.owner()));
  bool regress = own.transfer(RecoveryOwner::OPERATOR, RecoveryGeneration(1), WorkerBootId::generate(), a);
  std::printf("stale transfer (gen1 <= gen2) rejected: %d\n", !regress);
  // stale authority fencing: a worker restart must not inherit prior authority.
  AuthorityEnvelope oldBoot = a;
  AuthorityEnvelope newBoot = oldBoot.next_boot(WorkerBootId::generate());
  FenceResult fr = fence_completion(oldBoot, newBoot, AuthorityEnvelope{});
  std::printf("stale completion from old boot rejected: %d\n", !fr.accepted);
  return (t && !regress && !fr.accepted) ? 0 : 1;
}