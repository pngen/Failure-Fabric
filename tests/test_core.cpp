#include "failure_fabric/ff.hpp"
#include "fft.hpp"
#include <random>
#include <set>

using namespace ff;

TEST(uuid_round_trip_exact) {
  std::mt19937_64 rng(42);
  for (int i = 0; i < 5000; ++i) {
    uuid128 u = uuid128::generate(rng);
    CHECK_EQ(uuid128::from_hex(u.to_hex()), u);
    CHECK(u.to_hex().size() == 32);
    // display form round-trips through strip of dashes
    std::string d = u.to_display();
    CHECK_EQ(d.size(), size_t(36));
  }
  // left-pad shorter hex
  uuid128 shorty = uuid128::from_hex("a1b2");
  CHECK_EQ(shorty.to_hex().substr(28), "a1b2");
}

TEST(identity_strong_typing) {
  std::mt19937_64 rng(7);
  FailureId f = FailureId::generate(rng);
  RequestId  r = RequestId::generate(rng);
  CHECK(f == f);
  CHECK(f.raw != r.raw);
  // distinct types serialize to distinct string forms
  CHECK(f.to_hex() != r.to_hex());
  CHECK(!f.is_null());
}

TEST(generation_monotonic_and_distinct) {
  Generation g;
  CHECK(g.is_unset());
  Generation n = g.next();
  CHECK(n.value == 1);
  CHECK(n < n.next());
}

TEST(state_machine_legal_and_illegal) {
  OperationStateMachine m{OperationState::CREATED};
  CHECK(m.transition(OperationState::DISPATCHED));
  CHECK(m.transition(OperationState::RUNNING));
  CHECK(m.transition(OperationState::COMPLETION_REPORTED));
  CHECK(m.transition(OperationState::COMPLETION_CONFIRMED));
  CHECK_EQ(m.state(), OperationState::COMPLETION_CONFIRMED);
  // terminal: cannot leave
  CHECK(!m.transition(OperationState::RUNNING));
  // illegal raw jump
  OperationStateMachine m2{OperationState::CREATED};
  CHECK(!m2.transition(OperationState::RETRYING));
  CHECK(!OperationStateMachine::is_allowed(OperationState::CREATED, OperationState::CREATED));
  CHECK(OperationStateMachine::is_terminal(OperationState::CANCELLED));
  CHECK(OperationStateMachine::is_failed(OperationState::FAILED_AMBIGUOUS));
}

TEST(fence_rejects_stale_worker_boot_completion) {
  AuthorityEnvelope current;
  current.coordinator_epoch = CoordinatorEpoch(3);
  current.attempt = AttemptId::from_hex("aabb");
  current.attempt_generation = AttemptGeneration(2);
  current.operation_generation = OperationGeneration(2);
  current.dispatch = DispatchId::from_hex("ccdd");
  current.worker_boot = WorkerBootId::from_hex("1111");

  // A completion from the OLD worker (different boot) at old generation must be rejected.
  AuthorityEnvelope stale = current;
  stale.worker_boot = WorkerBootId::from_hex("2222"); // restart -> fresh boot
  stale.attempt_generation = AttemptGeneration(2);
  stale.operation_generation = OperationGeneration(2);
  FenceResult r = fence_completion(stale, current, AuthorityEnvelope{});
  CHECK(!r.accepted);
  CHECK_EQ(r.reason, FenceReason::BOOT_STALE);
}

TEST(fence_accepts_current_authority_completion) {
  AuthorityEnvelope current;
  current.coordinator_epoch = CoordinatorEpoch(3);
  current.attempt = AttemptId::from_hex("aabb");
  current.attempt_generation = AttemptGeneration(2);
  current.operation_generation = OperationGeneration(2);
  current.worker_boot = WorkerBootId::from_hex("1111");
  current.dispatch = DispatchId::from_hex("ccdd");

  AuthorityEnvelope ok = current;
  FenceResult r = fence_completion(ok, current, AuthorityEnvelope{});
  CHECK(r.accepted);
  CHECK_EQ(r.status, AuthorityStatus::AUTHORITATIVE);
}

TEST(fence_rejects_old_epoch_and_terminal_jump) {
  AuthorityEnvelope current;
  current.coordinator_epoch = CoordinatorEpoch(5);
  current.attempt = AttemptId::from_hex("aabb");
  current.attempt_generation = AttemptGeneration(1);
  current.operation_generation = OperationGeneration(1);
  current.worker_boot = WorkerBootId::from_hex("1111");
  current.dispatch = DispatchId::from_hex("ccdd");

  AuthorityEnvelope old_epoch = current;
  old_epoch.coordinator_epoch = CoordinatorEpoch(4);
  CHECK(!fence_completion(old_epoch, current, AuthorityEnvelope{}).accepted);

  // terminal authority recorded at gen 2 -> a completion at gen 1 is stale
  AuthorityEnvelope terminal = current;
  terminal.attempt_generation = AttemptGeneration(2);
  terminal.operation_generation = OperationGeneration(2);
  AuthorityEnvelope late = current; // gen 1
  CHECK(!fence_completion(late, current, terminal).accepted);
}

TEST(fence_recovery_requires_strict_generation_increase) {
  AuthorityEnvelope current;
  current.coordinator_epoch = CoordinatorEpoch(1);
  current.attempt = AttemptId::from_hex("aabb");
  RecoveryGeneration curGen(2);
  AuthorityEnvelope cand = current;
  cand.recovery_generation = RecoveryGeneration(2); // not greater
  CHECK(!fence_recovery(cand, current, curGen).accepted);
  cand.recovery_generation = RecoveryGeneration(3);
  CHECK(fence_recovery(cand, current, curGen).accepted);
}

TEST(failure_record_derives_dimensions) {
  FailureRecordBuilder b;
  FailureId f = FailureId::generate();
  b.id(f).failure_class(FailureClass::AMBIGUOUS).operation(OperationId::generate())
    .attempt(AttemptId::generate()).timestamp(1234)
    .authority(AuthorityEnvelope{});
  b.derive_defaults();
  FailureRecord rec = b.build();
  CHECK_EQ(rec.failure_id, f);
  CHECK_EQ(rec.retryability, Retryability::CONDITIONALLY_RETRYABLE);
  CHECK_EQ(rec.terminality, Terminality::PENDING);
  CHECK(rec.side_effect_state == SideEffectState::UNKNOWN || rec.side_effect_state == SideEffectState::MAY_OCCURRED);
  CHECK(to_json(rec).find("failure_class") != std::string::npos);
}

int main() { return fft::run_all(); }