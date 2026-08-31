// Failure Fabric benchmark suite. Reports actual measurements only.
// Copyright 2026 Summon Software Labs. Apache-2.0.
#include "failure_fabric/ff.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <thread>
#include <atomic>
#include <vector>

using namespace ff;

static double now_ms() {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
  int n = argc > 1 ? std::atoi(argv[1]) : 100000;
  std::printf("Failure Fabric benchmark suite  (n=%d)\n", n);
  std::mt19937_64 rng(2026);
  RetryClassifier rc;
  RecoveryPlanner rp;

  // 1. failure-record append
  {
    FailureStore store;
    double t0 = now_ms();
    for (int i = 0; i < n; ++i) {
      FailureRecordBuilder b;
      b.failure_class(FailureClass::TRANSIENT).operation(OperationId::generate(rng)).attempt(AttemptId::generate(rng)).authority([&]{ AuthorityEnvelope a; a.coordinator_epoch=CoordinatorEpoch(1); a.attempt=AttemptId::generate(rng); a.attempt_generation=AttemptGeneration(1); a.worker_boot=WorkerBootId::generate(rng); return a; }());
      store.append_failure(b.build(rng));
    }
    double ms = now_ms() - t0;
    std::printf("  failure-record append       : %8.1f ops/s  (%d in %.1f ms)\n", n / (ms / 1000.0), n, ms);
  }

  // 2. classification
  {
    FailureRecord rec;
    rec.failure_class = FailureClass::TRANSIENT; rec.completion = Ambiguity::KNOWN; rec.side_effect_state = SideEffectState::NONE_POSSIBLE;
    RetryInput in; in.idempotent_retry_possible = true; in.attempt_count = 1; in.worker_healthy = true; in.dependency_ready = true;
    double t0 = now_ms();
    volatile int sink = 0;
    for (int i = 0; i < n; ++i) { auto d = rc.classify(rec, in, RetryPolicy{}); sink += static_cast<int>(d.verdict); }
    double ms = now_ms() - t0;
    std::printf("  classification              : %8.1f ops/s  (%d in %.1f ms)\n", n / (ms / 1000.0), n, ms);
    (void)sink;
  }

  // 3. recovery-plan generation
  {
    FailureRecord rec;
    rec.failure_class = FailureClass::AMBIGUOUS; rec.completion = Ambiguity::AMBIGUOUS; rec.side_effect_state = SideEffectState::MAY_OCCURRED;
    rec.authority.coordinator_epoch = CoordinatorEpoch(1);
    PlanInput pi; pi.idempotent_retry_possible = true; pi.rollback_possible = true;
    double t0 = now_ms();
    volatile int sink = 0;
    for (int i = 0; i < n; ++i) { auto p = rp.plan(rec, pi); sink += static_cast<int>(p.action); }
    double ms = now_ms() - t0;
    std::printf("  recovery-plan generation    : %8.1f ops/s  (%d in %.1f ms)\n", n / (ms / 1000.0), n, ms);
    (void)sink;
  }

  // 4. idempotency lookup + duplicate rejection
  {
    IdempotencyStore idem;
    OperationId op = OperationId::generate(rng);
    IdempotencyKey key = IdempotencyKey::generate(rng);
    AuthorityEnvelope a; a.coordinator_epoch = CoordinatorEpoch(1); a.attempt = AttemptId::generate(rng); a.attempt_generation = AttemptGeneration(1); a.worker_boot = WorkerBootId::generate(rng);
    idem.begin(op, key, 42, a);
    double t0 = now_ms();
    volatile int sink = 0;
    for (int i = 0; i < n; ++i) { auto v = idem.begin(op, key, 42, a); sink += static_cast<int>(v); }
    double ms = now_ms() - t0;
    std::printf("  idempotency lookup (dup rej) : %8.1f ops/s  (%d in %.1f ms)\n", n / (ms / 1000.0), n, ms);
    (void)sink;
  }

  // 5. state reconstruction (create+dispatch+run+complete+confirm)
  {
    double t0 = now_ms();
    for (int i = 0; i < n; ++i) {
      OperationRegistry reg;
      OperationId op = OperationId::generate(rng);
      AuthorityEnvelope a; a.coordinator_epoch = CoordinatorEpoch(1); a.attempt = AttemptId::generate(rng); a.attempt_generation = AttemptGeneration(1); a.worker_boot = WorkerBootId::generate(rng);
      reg.create(op, a); reg.dispatch(op, a); reg.run(op, a); reg.report_completion(op, a, false); reg.confirm_completion(op, a);
    }
    double ms = now_ms() - t0;
    std::printf("  state reconstruction         : %8.1f ops/s  (%d in %.1f ms)\n", n / (ms / 1000.0), n, ms);
  }

  // 6. persistence save / recovery
  {
    FailureStore store;
    for (int i = 0; i < n; ++i) {
      FailureRecordBuilder b; b.failure_class(FailureClass::TRANSIENT).operation(OperationId::generate(rng)).attempt(AttemptId::generate(rng));
      AuthorityEnvelope a; a.coordinator_epoch = CoordinatorEpoch(1); a.attempt = AttemptId::generate(rng); a.attempt_generation = AttemptGeneration(1); a.worker_boot = WorkerBootId::generate(rng); b.authority(a);
      store.append_failure(b.build(rng));
    }
    std::string path = "ff_bench_snapshot.bin";
    double t0 = now_ms();
    bool ok = store.save(path);
    double s_ms = now_ms() - t0;
    FailureStore loaded;
    double t1 = now_ms();
    bool ok2 = loaded.load(path);
    double r_ms = now_ms() - t1;
    std::remove(path.c_str());
    std::printf("  persistence save/recover     : save %.1f ms (%.1f MB/s), load %.1f ms, records %d  [%s]\n",
                s_ms, (double)(n * 200) / (s_ms / 1000.0) / 1e6, r_ms, n, (ok && ok2) ? "ok" : "FAIL");
  }

  // 7. deterministic replay
  {
    FailureStore store;
    for (int i = 0; i < n; ++i) {
      FailureRecordBuilder b; b.failure_class(FailureClass::PARTIAL_SUCCESS).operation(OperationId::generate(rng)).attempt(AttemptId::generate(rng));
      AuthorityEnvelope a; a.coordinator_epoch = CoordinatorEpoch(1); a.attempt = AttemptId::generate(rng); a.attempt_generation = AttemptGeneration(1); a.worker_boot = WorkerBootId::generate(rng); b.authority(a);
      store.append_failure(b.build(rng));
    }
    std::string path = "ff_bench_replay.bin";
    store.save(path);
    double t0 = now_ms();
    FailureStore loaded;
    loaded.load(path);
    double ms = now_ms() - t0;
    std::remove(path.c_str());
    std::printf("  deterministic replay          : %8.1f ops/s  (%d in %.1f ms)\n", n / (ms / 1000.0), n, ms);
  }

  // 8. concurrent failure ingestion (threads)
  {
    FailureStore store;
    std::atomic<bool> stop{false};
    auto worker = [&store, &rng, &stop](unsigned seed) {
      std::mt19937_64 r(seed);
      while (!stop.load()) {
        FailureRecordBuilder b; b.failure_class(FailureClass::TRANSIENT).operation(OperationId::generate(r)).attempt(AttemptId::generate(r));
        AuthorityEnvelope a; a.coordinator_epoch = CoordinatorEpoch(1); a.attempt = AttemptId::generate(r); a.attempt_generation = AttemptGeneration(1); a.worker_boot = WorkerBootId::generate(r); b.authority(a);
        store.append_failure(b.build(r));
      }
    };
    unsigned threads = 8;
    double t0 = now_ms();
    std::vector<std::thread> ts;
    for (unsigned i = 0; i < threads; ++i) ts.emplace_back(worker, 100 + i);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    stop.store(true);
    for (auto& t : ts) t.join();
    double ms = now_ms() - t0;
    std::printf("  concurrent failure ingestion : %8.1f ops/s  (%zu records in %.1f ms, %u threads)\n",
                (double)store.failure_count() / (ms / 1000.0), store.failure_count(), ms, threads);
  }

  // 9. explanation generation
  {
    FailureRecord rec;
    rec.failure_class = FailureClass::AMBIGUOUS; rec.completion = Ambiguity::AMBIGUOUS; rec.side_effect_state = SideEffectState::MAY_OCCURRED;
    rec.authority.coordinator_epoch = CoordinatorEpoch(1); rec.attempt = AttemptId::generate(rng);
    RetryDecision d; d.verdict = RetryVerdict::RETRY; d.reason = "r";
    PlanInput pi; pi.idempotent_retry_possible = true;
    auto plan = rp.plan(rec, pi);
    double t0 = now_ms();
    volatile int sink = 0;
    for (int i = 0; i < n; ++i) { auto s = explain_failure(rec); sink += (int)s.size(); }
    double ms = now_ms() - t0;
    std::printf("  explanation generation       : %8.1f ops/s  (%d in %.1f ms)\n", n / (ms / 1000.0), n, ms);
    (void)sink; (void)d;
  }
  std::printf("benchmark complete\n");
  return 0;
}
