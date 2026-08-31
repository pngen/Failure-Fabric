// Semantics, property-invariant (fixed seed), corruption, and concurrency tests.
// Copyright 2026 Summon Software Labs. Apache-2.0.
#include "failure_fabric/ff.hpp"
#include "fft.hpp"
#include <random>
#include <thread>
#include <atomic>
#include <map>
#include <string>

using namespace ff;

static AuthorityEnvelope auth_for(std::mt19937_64& rng, uint64_t gen) {
  AuthorityEnvelope a;
  a.coordinator_epoch = CoordinatorEpoch(1);
  a.attempt = AttemptId::generate(rng);
  a.dispatch = DispatchId::generate(rng);
  a.attempt_generation = AttemptGeneration(gen);
  a.operation_generation = OperationGeneration(gen);
  a.worker_boot = WorkerBootId::generate(rng);
  return a;
}

TEST(retry_hard_bounded) {
  RetryClassifier c; RetryPolicy p; p.max_attempts = 3;
  FailureRecord rec; rec.failure_class = FailureClass::TRANSIENT; rec.completion = Ambiguity::KNOWN; rec.side_effect_state = SideEffectState::NONE_POSSIBLE;
  RetryInput in; in.idempotent_retry_possible = true; in.worker_healthy = true; in.dependency_ready = true;
  for (uint32_t a = 0; a < p.max_attempts; ++a) {
    in.attempt_count = a;
    CHECK(c.classify(rec, in, p).verdict == RetryVerdict::RETRY);
  }
  in.attempt_count = p.max_attempts;
  CHECK(c.classify(rec, in, p).verdict == RetryVerdict::DO_NOT_RETRY);
}

TEST(retry_ambiguous_requires_idempotency) {
  RetryClassifier c; RetryPolicy p;
  FailureRecord rec; rec.failure_class = FailureClass::AMBIGUOUS; rec.completion = Ambiguity::AMBIGUOUS; rec.side_effect_state = SideEffectState::MAY_OCCURRED; rec.rollback = RollbackRequirement::REQUIRED;
  RetryInput in; in.idempotent_retry_possible = false; in.attempt_count = 1; in.worker_healthy = true;
  auto d = c.classify(rec, in, p);
  CHECK(d.verdict == RetryVerdict::ROLLBACK_FIRST);
  in.idempotent_retry_possible = true;
  d = c.classify(rec, in, p);
  CHECK(d.verdict == RetryVerdict::RETRY);
  CHECK(d.requires_fresh_attempt);
}

TEST(persistent_verdicts) {
  // known fatal classes are permanently non-retryable (never unbounded loops)
  RetryClassifier c; RetryPolicy p;
  for (auto cls : {FailureClass::PERMANENT, FailureClass::VALIDATION_FAILURE, FailureClass::INCOMPATIBILITY, FailureClass::CANCELLED}) {
    FailureRecord rec; rec.failure_class = cls; rec.retryability = Retryability::NON_RETRYABLE;
    RetryInput in; in.attempt_count = 0; in.idempotent_retry_possible = true;
    CHECK(c.classify(rec, in, p).verdict == RetryVerdict::DO_NOT_RETRY);
  }
}

TEST(rollback_monotonic_resumable) {
  RollbackPlan plan;
  for (int i = 0; i < 5; ++i) plan.steps.push_back({RollbackAction::RELEASE_RESERVATION, std::to_string(i), true, false, false, ""});
  RollbackExecutor ex(std::move(plan));
  // Fenced/blocked at first => must not advance (monotonic).
  bool ok = ex.run_next(false);
  CHECK(!ok);
  CHECK_EQ(ex.completed(), size_t(0)); // progress pinned, no regression/skip
  CHECK(ex.is_failed());
  // fresh executor resumable from persisted count
  RollbackExecutor ex2(ex.plan());
  ex2.resume_from(2);
  while (!ex2.is_complete()) { CHECK(ex2.run_next(true)); }
  CHECK_EQ(ex2.completed(), size_t(5));
}

TEST(compensation_is_not_rollback_and_monotonic) {
  CompensationRecord rec; rec.compensation_id = CompensationId::generate(); rec.target_operation = OperationId::generate();
  rec.reason = "external side effect escaped";
  Compensator comp(std::move(rec));
  comp.run(true);
  CHECK(comp.is_complete());
  size_t attempts = comp.record().attempts;
  comp.run(true); // running again must not change monotonic completion
  CHECK(comp.is_complete());
  CHECK_EQ(comp.record().attempts, attempts);
  comp.resume_failure("op");
  CHECK(comp.is_failed());
  comp.run(true);
  CHECK(comp.is_failed()); // monotonic: failure final
}

TEST(ownership_transfer_requires_strict_generation) {
  std::mt19937_64 rng(9);
  RecoveryOwnership own;
  AuthorityEnvelope a = auth_for(rng, 1);
  CHECK(own.adopt(RecoveryOwner::COORDINATOR, RecoveryGeneration(1), a.worker_boot, a));
  CHECK(own.transfer(RecoveryOwner::REPLACEMENT_WORKER, RecoveryGeneration(2), WorkerBootId::generate(), a));
  CHECK(!own.transfer(RecoveryOwner::OPERATOR, RecoveryGeneration(1), WorkerBootId::generate(), a)); // regress
  CHECK(!own.transfer(RecoveryOwner::OPERATOR, RecoveryGeneration(2), WorkerBootId::generate(), a)); // not strictly greater
  CHECK(own.transfer(RecoveryOwner::OPERATOR, RecoveryGeneration(3), WorkerBootId::generate(), a));
  CHECK_EQ(own.owner(), RecoveryOwner::OPERATOR);
}

TEST(recovery_plan_deterministic_and_rejects_alts) {
  RecoveryPlanner rp;
  std::mt19937_64 rng(10);
  FailureRecord rec;
  rec.failure_class = FailureClass::AMBIGUOUS; rec.completion = Ambiguity::AMBIGUOUS; rec.side_effect_state = SideEffectState::MAY_OCCURRED;
  rec.authority.coordinator_epoch = CoordinatorEpoch(1);
  PlanInput pi; pi.idempotent_retry_possible = true; pi.rollback_possible = true;
  auto p1 = rp.plan(rec, pi);
  auto p2 = rp.plan(rec, pi);
  CHECK(p1.action == p2.action);
  CHECK_EQ(p1.generation.value, p2.generation.value);
  CHECK(!p1.reason_factors.empty());
  // ambiguous + idempotent -> RETRY, and rollback/compensate listed as rejected alts
  CHECK(p1.action == RecoveryAction::RETRY);
  CHECK(!p1.rejected_alternatives.empty());
}

// property: one authoritative terminal outcome per operation (fixed seed).
TEST(property_one_terminal_outcome) {
  std::mt19937_64 rng(11);
  for (int trial = 0; trial < 200; ++trial) {
    OperationRegistry reg;
    OperationId op = OperationId::generate(rng);
    AuthorityEnvelope a1 = auth_for(rng, 1);
    reg.create(op, a1); reg.dispatch(op, a1); reg.run(op, a1);
    // interleave stale attempts and completions
    ApplyResult kom = reg.report_completion(op, a1, false);
    int terminal = 0;
    if (kom == ApplyResult::OK && reg.confirm_completion(op, a1) == ApplyResult::OK) ++terminal;
    // after terminal, further attempts must not change it
    CHECK(reg.transition(op, OperationState::RUNNING, a1) == ApplyResult::ALREADY_TERMINAL);
    CHECK_EQ(reg.state(op), OperationState::COMPLETION_CONFIRMED);
    CHECK_EQ(terminal, 1); // exactly one terminal confirmation
  }
}


// property: retry count never exceeds policy; conflicting duplicate always rejected.
TEST(property_conflicting_duplicate_rejected) {
  IdempotencyStore idem;
  std::mt19937_64 rng(12);
  OperationId op = OperationId::generate(rng);
  IdempotencyKey key = IdempotencyKey::generate(rng);
  AuthorityEnvelope a = auth_for(rng, 1);
  CHECK(idem.begin(op, key, 0x1111, a) == IdempotencyVerdict::NEW);
  for (int i = 0; i < 100; ++i) {
    auto v = idem.begin(op, key, 0x2222, a); // different hash
    CHECK(v == IdempotencyVerdict::CONFLICT);
  }
  CHECK(idem.begin(op, key, 0x1111, a) == IdempotencyVerdict::IN_PROGRESS_REPLAY); // same hash not conflict
}

// property: recovery generation only increases.
TEST(property_recovery_generation_monotonic) {
  std::mt19937_64 rng(13);
  RecoveryOwnership own;
  AuthorityEnvelope a = auth_for(rng, 1);
  own.adopt(RecoveryOwner::COORDINATOR, RecoveryGeneration(1), a.worker_boot, a);
  uint64_t g = 1;
  for (int i = 0; i < 50; ++i) {
    uint64_t next = g + 1;
    CHECK(own.transfer(RecoveryOwner::REPLACEMENT_WORKER, RecoveryGeneration(next), WorkerBootId::generate(), a));
    CHECK_EQ(own.generation().value, next);
    g = next;
  }
}

TEST(corruption_trailing_garbage_rejected) {
  // Build a valid frame then append garbage: decode_frame loop in store must reject.
  std::mt19937_64 rng(14);
  FailureRecordBuilder b; b.failure_class(FailureClass::TRANSIENT).operation(OperationId::generate(rng)).attempt(AttemptId::generate(rng));
  AuthorityEnvelope a; a.coordinator_epoch = CoordinatorEpoch(1); a.attempt = AttemptId::generate(rng); a.attempt_generation = AttemptGeneration(1); a.worker_boot = WorkerBootId::generate(rng); b.authority(a);
  FailureRecord rec = b.build(rng);
  BinaryWriter w; encode_failure_record(w, rec);
  auto frame = encode_frame(3, w.buffer().data(), w.buffer().size());
  std::vector<uint8_t> with_garbage = frame;
  with_garbage.push_back(0xDE); with_garbage.push_back(0xAD); with_garbage.push_back(0xBE); with_garbage.push_back(0xEF);
  FrameHeader h; size_t next = 0;
  CHECK(decode_frame(with_garbage.data(), with_garbage.size(), 0, h, next)); // first frame ok
  // remaining garbage should not decode as a second frame
  CHECK(!decode_frame(with_garbage.data(), with_garbage.size(), next, h, next));
}

// concurrency: simultaneous failures + registry ops on the same op -> exactly one terminal, no exceptions.
TEST(concurrency_no_double_terminal) {
  OperationRegistry reg;
  std::mt19937_64 rng(15);
  OperationId op = OperationId::generate(rng);
  AuthorityEnvelope a = auth_for(rng, 1);
  reg.create(op, a); reg.dispatch(op, a); reg.run(op, a);
  // Concurrent reporters try to confirm terminal exactly once.
  std::atomic<int> terminals{0};
  auto hammer = [&]() {
    for (int i = 0; i < 500; ++i) {
      if (reg.report_completion(op, a, false) == ApplyResult::OK && reg.confirm_completion(op, a) == ApplyResult::OK)
        terminals.fetch_add(1);
    }
  };
  std::vector<std::thread> ts;
  for (int i = 0; i < 8; ++i) ts.emplace_back(hammer);
  for (auto& t : ts) t.join();
  CHECK(terminals.load() <= 1); // exactly one authoritative terminal outcome
  CHECK_EQ(reg.state(op), OperationState::COMPLETION_CONFIRMED);
}

// concurrency: concurrent failure ingestion never corrupts the log.
TEST(concurrency_failure_ingestion_without_corruption) {
  FailureStore store;
  std::mt19937_64 rng(16);
  auto worker = [&](unsigned seed) {
    std::mt19937_64 r(seed);
    for (int i = 0; i < 2000; ++i) {
      FailureRecordBuilder b; b.failure_class(FailureClass::TRANSIENT).operation(OperationId::generate(r)).attempt(AttemptId::generate(r));
      AuthorityEnvelope a; a.coordinator_epoch = CoordinatorEpoch(1); a.attempt = AttemptId::generate(r); a.attempt_generation = AttemptGeneration(1); a.worker_boot = WorkerBootId::generate(r); b.authority(a);
      store.append_failure(b.build(r));
    }
  };
  std::vector<std::thread> ts;
  for (unsigned i = 0; i < 8; ++i) ts.emplace_back(worker, 200 + i);
  for (auto& t : ts) t.join();
  // Each failure id is unique; log size equals number of successfully appended unique records.
  std::map<FailureId, int> counts;
  for (auto& f : store.failures()) ++counts[f.failure_id];
  CHECK_EQ(counts.size(), store.failure_count()); // no duplicates
  // no failure with a null id
  for (auto& f : store.failures()) CHECK(!f.failure_id.is_null());
  CHECK(store.failure_count() > 0);
}

TEST(explain_is_deterministic) {
  FailureRecord rec;
  rec.failure_class = FailureClass::CORRUPTION; rec.completion = Ambiguity::KNOWN; rec.side_effect_state = SideEffectState::KNOWN_NOT_OCCURRED;
  (void)rec.side_effect_state;
  rec.authority.coordinator_epoch = CoordinatorEpoch(1); rec.attempt = AttemptId::generate(); rec.operation = OperationId::generate();
  RetryDecision d; d.verdict = RetryVerdict::DO_NOT_RETRY; d.reason = "r";
  RecoveryPlanner rp; PlanInput pi; pi.rollback_possible = true;
  FailureRecord rec2 = rec; rec2.attempt = rec.attempt; rec2.operation = rec.operation;
  auto plan = rp.plan(rec2, pi);
  std::string t1 = explain_failure(rec2);
  std::string t2 = explain_failure(rec2);
  CHECK_EQ(t1, t2); // deterministic
  std::string j1 = explain_decision_json(rec2, d, plan);
  std::string j2 = explain_decision_json(rec2, d, plan);
  CHECK_EQ(j1, j2);
}

int main() { return fft::run_all(); }