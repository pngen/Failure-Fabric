// Downstream find_package(FailureFabric) consumer. Copyright 2026 Summon Software Labs.
#include <failure_fabric/ff.hpp>
#include <cstdio>
using namespace ff;
int main() {
  std::mt19937_64 rng(1);
  FailureRecordBuilder b;
  b.failure_class(FailureClass::VALIDATION_FAILURE).operation(OperationId::generate(rng)).attempt(AttemptId::generate(rng));
  AuthorityEnvelope a; a.coordinator_epoch = CoordinatorEpoch(1); a.attempt = AttemptId::generate(rng); a.attempt_generation = AttemptGeneration(1); a.worker_boot = WorkerBootId::generate(rng);
  b.authority(a);
  FailureRecord rec = b.build(rng);
  RetryClassifier c; RetryInput in; in.idempotent_retry_possible = true;
  auto d = c.classify(rec, in, RetryPolicy{});
  std::printf("consumer: VALIDATION_FAILURE -> verdict %s\n", to_string(d.verdict));
  std::printf("consumer: %s\n", explain_failure(rec).c_str());
  return d.verdict == RetryVerdict::DO_NOT_RETRY ? 0 : 1;
}
