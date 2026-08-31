#include "failure_fabric/ff.hpp"
#include "fft.hpp"
#include <random>
#include <string>

using namespace ff;

static AuthorityEnvelope make_auth(std::mt19937_64& rng, uint64_t gen) {
  AuthorityEnvelope a;
  a.coordinator_epoch = CoordinatorEpoch(1);
  a.attempt = AttemptId::generate(rng);
  a.dispatch = DispatchId::generate(rng);
  a.attempt_generation = AttemptGeneration(gen);
  a.operation_generation = OperationGeneration(gen);
  a.worker_boot = WorkerBootId::generate(rng);
  return a;
}

TEST(registry_one_terminal_outcome) {
  std::mt19937_64 rng(1);
  OperationRegistry reg;
  OperationId op = OperationId::generate(rng);
  AuthorityEnvelope a1 = make_auth(rng, 1);
  CHECK_EQ(reg.create(op, a1), ApplyResult::OK);
  CHECK_EQ(reg.dispatch(op, a1), ApplyResult::OK);
  CHECK_EQ(reg.run(op, a1), ApplyResult::OK);
  CHECK_EQ(reg.report_completion(op, a1, false), ApplyResult::OK);
  CHECK_EQ(reg.confirm_completion(op, a1), ApplyResult::OK);
  CHECK_EQ(reg.state(op), OperationState::COMPLETION_CONFIRMED);
  CHECK_EQ(reg.record_terminal(op, a1, "COMPLETED"), ApplyResult::OK); // idempotent same outcome
  CHECK_EQ(reg.record_terminal(op, a1, "TERMINAL_FAILED"), ApplyResult::ALREADY_TERMINAL); // different outcome rejected
  // cannot mutate terminal
  CHECK_EQ(reg.transition(op, OperationState::RUNNING, a1), ApplyResult::ALREADY_TERMINAL);
  CHECK_EQ(reg.state(op), OperationState::COMPLETION_CONFIRMED);
}

TEST(registry_rejects_stale_boot_completion) {
  std::mt19937_64 rng(2);
  OperationRegistry reg;
  OperationId op = OperationId::generate(rng);
  AuthorityEnvelope a1 = make_auth(rng, 1);
  CHECK_EQ(reg.create(op, a1), ApplyResult::OK);
  CHECK_EQ(reg.dispatch(op, a1), ApplyResult::OK);
  CHECK_EQ(reg.run(op, a1), ApplyResult::OK);
  // a fresh worker restart takes over with a fresh boot at gen 2
  AuthorityEnvelope a2 = a1;
  a2.worker_boot = WorkerBootId::generate(rng);
  a2.attempt = AttemptId::generate(rng);
  a2.attempt_generation = AttemptGeneration(2);
  a2.operation_generation = OperationGeneration(2);
  CHECK_EQ(reg.dispatch(op, a2), ApplyResult::OK); // advance authority to a2
  // the OLD worker (a1) completion must be rejected as stale
  CHECK_EQ(reg.report_completion(op, a1, false), ApplyResult::STALE_REJECTED);
  // the NEW worker succeds
  CHECK_EQ(reg.run(op, a2), ApplyResult::OK);
  CHECK_EQ(reg.report_completion(op, a2, false), ApplyResult::OK);
  CHECK_EQ(reg.confirm_completion(op, a2), ApplyResult::OK);
  CHECK_EQ(reg.state(op), OperationState::COMPLETION_CONFIRMED);
}

TEST(registry_rejects_old_epoch_and_duplicate_report) {
  std::mt19937_64 rng(3);
  OperationRegistry reg;
  OperationId op = OperationId::generate(rng);
  AuthorityEnvelope a1 = make_auth(rng, 1);
  CHECK_EQ(reg.create(op, a1), ApplyResult::OK);
  CHECK_EQ(reg.dispatch(op, a1), ApplyResult::OK);
  CHECK_EQ(reg.run(op, a1), ApplyResult::OK);
  CHECK_EQ(reg.report_completion(op, a1, false), ApplyResult::OK);
  CHECK_EQ(reg.report_completion(op, a1, false), ApplyResult::ALREADY_REPORTED);
  CHECK_EQ(reg.confirm_completion(op, a1), ApplyResult::OK);
  // old epoch is rejected
  AuthorityEnvelope old = a1;
  old.coordinator_epoch = CoordinatorEpoch(0);
  CHECK_EQ(reg.record_terminal(op, old, "TERMINAL_FAILED"), ApplyResult::ALREADY_TERMINAL);
}

TEST(idempotency_new_then_replay_and_conflict) {
  IdempotencyStore s;
  std::mt19937_64 rng(4);
  OperationId op = OperationId::generate(rng);
  IdempotencyKey key = IdempotencyKey::generate(rng);
  AuthorityEnvelope a = make_auth(rng, 1);
  CHECK_EQ(s.begin(op, key, 0x1234, a), IdempotencyVerdict::NEW);
  // same key + hash in flight -> dedupe
  CHECK_EQ(s.begin(op, key, 0x1234, a), IdempotencyVerdict::IN_PROGRESS_REPLAY);
  // conflicting hash -> reject
  CHECK_EQ(s.begin(op, key, 0x9999, a), IdempotencyVerdict::CONFLICT);
  s.complete(op, key, "result=42");
  // completed replay returns prior result without re-execution
  CHECK_EQ(s.begin(op, key, 0x1234, a), IdempotencyVerdict::COMPLETED_REPLAY);
  std::string prior;
  CHECK(s.prior_result(op, key, prior));
  CHECK_EQ(prior, "result=42");
}

TEST(store_append_rejects_duplicate_failure_id) {
  std::mt19937_64 rng(5);
  FailureStore store;
  FailureRecordBuilder b;
  b.failure_class(FailureClass::EXECUTION_FAILURE).operation(OperationId::generate(rng))
    .attempt(AttemptId::generate(rng)).origin(FailureOrigin::WORKER);
  FailureRecord rec = b.build(rng);
  CHECK(store.append_failure(rec));
  CHECK(!store.append_failure(rec)); // duplicate id rejected
  // missing operation/attempt rejected
  FailureRecord bad;
  bad.failure_id = FailureId::generate(rng);
  CHECK(!store.append_failure(bad));
}

TEST(persistence_failure_record_roundtrip) {
  std::mt19937_64 rng(6);
  FailureRecordBuilder b;
  b.failure_class(FailureClass::AMBIGUOUS).operation(OperationId::generate(rng))
    .attempt(AttemptId::generate(rng)).timestamp(1234567890).code(77)
    .summary("worker died after side effect").reason("side_effect=unknown; worker=dead")
    .phase(FailurePhase::EXECUTE).completion(Ambiguity::AMBIGUOUS)
    .side_effect_state(SideEffectState::MAY_OCCURRED).unknown_side_effects(1);
  AuthorityEnvelope a; a.coordinator_epoch = CoordinatorEpoch(2);
  a.attempt = AttemptId::generate(rng); a.attempt_generation = AttemptGeneration(1);
  a.worker_boot = WorkerBootId::generate(rng); a.operation_generation = OperationGeneration(1);
  b.authority(a);
  FailureRecord rec = b.build(rng);
  BinaryWriter w;
  encode_failure_record(w, rec);
  BinaryReader rr(w.buffer().data(), w.buffer().size());
  FailureRecord rec2;
  CHECK(decode_failure_record(rr, rec2));
  CHECK_EQ(rec2.summary, rec.summary);
  CHECK_EQ(rec2.failure_class, rec.failure_class);
  CHECK_EQ(rec2.attempt, rec.attempt);
  CHECK_EQ(rec2.timestamp_ms, rec.timestamp_ms);
  CHECK_EQ(rec2.device, rec.device);
}

TEST(persistence_rejects_corruption_truncation) {
  std::mt19937_64 rng(7);
  FailureRecordBuilder b;
  b.failure_class(FailureClass::TRANSIENT).operation(OperationId::generate(rng))
    .attempt(AttemptId::generate(rng));
  FailureRecord rec = b.build(rng);
  BinaryWriter w;
  encode_failure_record(w, rec);
  std::vector<uint8_t> data = w.buffer();
  // frame then corrupt a byte
  auto frame = encode_frame(7, data.data(), data.size());
  // flip a byte in the payload -> checksum mismatch
  std::vector<uint8_t> bad = frame;
  bad[15] ^= 0xFF;
  FrameHeader h; size_t next = 0;
  CHECK(!decode_frame(bad.data(), bad.size(), 0, h, next));
  // truncation
  std::vector<uint8_t> trunc(frame.begin(), frame.end() - 3);
  CHECK(!decode_frame(trunc.data(), trunc.size(), 0, h, next));
  // unsupported magic
  std::vector<uint8_t> badver = frame;
  badver[0] = 0; // corrupt magic first byte
  CHECK(!decode_frame(badver.data(), badver.size(), 0, h, next));
}

TEST(store_save_load_roundtrip_terminal) {
  std::mt19937_64 rng(8);
  FailureStore store;
  OperationId op = OperationId::generate(rng);
  AuthorityEnvelope a = make_auth(rng, 1);
  CHECK_EQ(store.ops().create(op, a), ApplyResult::OK);
  CHECK_EQ(store.ops().dispatch(op, a), ApplyResult::OK);
  CHECK_EQ(store.ops().run(op, a), ApplyResult::OK);
  CHECK_EQ(store.ops().report_completion(op, a, false), ApplyResult::OK);
  CHECK_EQ(store.ops().confirm_completion(op, a), ApplyResult::OK);
  FailureRecordBuilder b;
  b.failure_class(FailureClass::PARTIAL_SUCCESS).operation(op).attempt(a.attempt).authority(a);
  store.append_failure(b.build(rng));

  const std::string path = "ff_test_store.bin";
  CHECK(store.save(path));
  FailureStore loaded;
  CHECK(loaded.load(path));
  CHECK_EQ(loaded.ops().state(op), OperationState::COMPLETION_CONFIRMED);
  CHECK(loaded.has_failure(store.failures()[0].failure_id));
  std::remove(path.c_str());
}

int main() { return fft::run_all(); }