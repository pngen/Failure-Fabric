// Example: persistence + deterministic replay. Copyright 2026 Summon Software Labs.
#include "failure_fabric/ff.hpp"
#include <cstdio>
#include <cstdlib>
using namespace ff;
int main() {
  std::mt19937_64 rng(4);
  FailureStore store;
  OperationId op = OperationId::generate(rng);
  AuthorityEnvelope a; a.coordinator_epoch=CoordinatorEpoch(1); a.attempt=AttemptId::generate(rng); a.attempt_generation=AttemptGeneration(1); a.worker_boot=WorkerBootId::generate(rng);
  store.ops().create(op, a); store.ops().dispatch(op, a); store.ops().run(op, a); store.ops().report_completion(op, a, false); store.ops().confirm_completion(op, a);
  FailureRecordBuilder b; b.failure_class(FailureClass::PARTIAL_SUCCESS).operation(op).attempt(a.attempt).authority(a); store.append_failure(b.build(rng));
  const char* path = "ff_example_state.bin";
  if (!store.save(path)) return 1;
  FailureStore loaded;
  if (!loaded.load(path)) return 1;
  auto r = loaded.ops().find(op);
  std::printf("reloaded op state=%s terminal_recorded=%d\n", to_string(loaded.ops().state(op)), r && r->terminal_recorded ? 1 : 0);
  std::printf("reloaded failure records=%zu\n", loaded.failure_count());
  std::remove(path);
  return (loaded.ops().state(op) == OperationState::COMPLETION_CONFIRMED) ? 0 : 1;
}