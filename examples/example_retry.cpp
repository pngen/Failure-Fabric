// Example: transient retry with bounded retries. Copyright 2026 Summon Software Labs.
#include "failure_fabric/ff.hpp"
#include <cstdio>
using namespace ff;
int main() {
  std::mt19937_64 rng(1);
  FailureRecordBuilder b;
  b.failure_class(FailureClass::TRANSIENT).operation(OperationId::generate(rng)).attempt(AttemptId::generate(rng)).authority([]{ AuthorityEnvelope a; a.coordinator_epoch=CoordinatorEpoch(1); a.attempt=AttemptId::generate(); a.attempt_generation=AttemptGeneration(1); a.worker_boot=WorkerBootId::generate(); return a; }());
  FailureRecord rec = b.build(rng);
  RetryClassifier c; RetryInput in; in.idempotent_retry_possible=true; in.attempt_count=1; in.worker_healthy=true;
  RetryDecision d = c.classify(rec, in, RetryPolicy{});
  std::printf("transient failure -> verdict %s (backoff %u ms)\n", to_string(d.verdict), c.backoff_ms(1));
  std::printf("bounded: attempts=1/3, %s\n", to_string(d.verdict));
  return d.verdict == RetryVerdict::RETRY ? 0 : 1;
}