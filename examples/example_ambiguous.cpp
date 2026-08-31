// Example: ambiguous completion resolved by idempotent retry (no double apply).
// Copyright 2026 Summon Software Labs.
#include "failure_fabric/ff.hpp"
#include <cstdio>
using namespace ff;
int main() {
  std::mt19937_64 rng(2);
  OperationId op = OperationId::generate(rng);
  IdempotencyKey key = IdempotencyKey::generate(rng);
  IdempotencyStore idem;
  AuthorityEnvelope a; a.coordinator_epoch=CoordinatorEpoch(1); a.attempt=AttemptId::generate(rng); a.attempt_generation=AttemptGeneration(1); a.worker_boot=WorkerBootId::generate(rng);
  auto v0 = idem.begin(op, key, 0xA, a);
  std::printf("submit (may have side effect) -> %s\n", to_string(v0));
  // worker dies before ack -> completion ambiguous; re-submit same key+hash
  auto v1 = idem.begin(op, key, 0xA, a);
  std::printf("replay ambiguous completion under same key+hash -> %s (deduplicated, no double apply)\n", to_string(v1));
  idem.complete(op, key, "result=42");
  std::string prior;
  if (idem.prior_result(op, key, prior)) std::printf("completed idempotent op returns prior result metadata: %s\n", prior.c_str());
  auto vconf = idem.begin(op, key, 0xBAD, a);
  std::printf("conflicting duplicate payload under same key -> %s (rejected)\n", to_string(vconf));
  return (v1 == IdempotencyVerdict::IN_PROGRESS_REPLAY && vconf == IdempotencyVerdict::CONFLICT) ? 0 : 1;
}