// failure-fabric CLI. Copyright 2026 Summon Software Labs. Apache-2.0.
#include "failure_fabric/ff.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <random>
#include <windows.h>

using namespace ff;

static std::string dir() {
  char buf[MAX_PATH];
  GetModuleFileNameA(nullptr, buf, MAX_PATH);
  std::string p(buf);
  auto pos = p.find_last_of("\\/");
  return pos == std::string::npos ? "." : p.substr(0, pos);
}
static int shell(const std::string& cmd) { return system(cmd.c_str()); }
static std::string sibling(const std::string& name) { return dir() + "\\" + name; }

static void usage() {
  std::printf(
    "usage: ff_cli <command> [args]\n"
    "  list                         list operations and failures\n"
    "  inspect <op_hex>             show operation state + authority\n"
    "  failure <fail_hex>           show a failure record\n"
    "  operation <op_hex>           show operation runtime\n"
    "  attempt <op_hex>             show attempt authority\n"
    "  classify <class>             classify a failure (dimensions)\n"
    "  retry <class>                classify retry decision\n"
    "  rollback                     run a typed rollback plan\n"
    "  compensate                   run a compensation\n"
    "  recover <class>              generate a recovery plan\n"
    "  plan <class>                 generate a RecoveryPlan (with rejected alts)\n"
    "  owner                        show recovery ownership + transfer\n"
    "  explain <fail_hex>           deterministic explanation\n"
    "  snapshot                     store snapshot summary\n"
    "  save <file>                  persist store\n"
    "  reload <file>                load store\n"
    "  replay <file>                deterministic replay -> terminal outcomes\n"
    "  serve <port>                 run the coordinator server\n"
    "  worker <port> <mode>         run a worker (normal|slow|fail)\n"
    "  multiprocess                 run the real multiprocess failure proof\n"
    "  cuda                         run the CUDA failure/recovery proof\n"
    "  benchmark <n>                run the benchmark suite\n"
    "  help                         this message\n");
}

static AuthorityEnvelope demo_auth(uint64_t gen) {
  AuthorityEnvelope a;
  a.coordinator_epoch = CoordinatorEpoch(1);
  a.attempt = AttemptId::from_hex("a1b2c3d4");
  a.dispatch = DispatchId::from_hex("feedbeef");
  a.attempt_generation = AttemptGeneration(gen);
  a.operation_generation = OperationGeneration(gen);
  a.worker_boot = WorkerBootId::from_hex("1234abcd");
  return a;
}

int main(int argc, char** argv) {
  if (argc < 2) { usage(); return 0; }
  std::string cmd = argv[1];
  FailureStore store;

  if (cmd == "help") { usage(); return 0; }
  if (cmd == "list") {
    for (auto& op : store.ops().operations()) std::printf("  op %s state=%s\n", op.to_display().c_str(), to_string(store.ops().state(op)));
    for (auto& f : store.failures()) std::printf("  fail %s class=%s op=%s\n", f.failure_id.to_display().c_str(), to_string(f.failure_class), f.operation.to_display().c_str());
    std::printf("  %zu operations, %zu failures\n", store.ops().size(), store.failure_count());
    return 0;
  }
  if (cmd == "classify" || cmd == "retry" || cmd == "recover" || cmd == "plan") {
    FailureClass fc = FailureClass::UNKNOWN;
    std::string cls = argc > 2 ? argv[2] : "TRANSIENT";
    if (!from_string(fc, cls)) { std::printf("unknown class: %s\n", cls.c_str()); return 2; }
    std::mt19937_64 rng(1);
    FailureRecordBuilder b;
    b.failure_class(fc).operation(OperationId::generate(rng)).attempt(AttemptId::generate(rng))
      .origin(FailureOrigin::WORKER).phase(FailurePhase::EXECUTE)
      .completion(fc == FailureClass::AMBIGUOUS ? Ambiguity::AMBIGUOUS : Ambiguity::KNOWN)
      .side_effect_state(fc == FailureClass::AMBIGUOUS ? SideEffectState::MAY_OCCURRED : SideEffectState::NONE_POSSIBLE)
      .authority(demo_auth(1));
    FailureRecord rec = b.build(rng);
    std::printf("%s\n", explain_failure(rec).c_str());
    if (cmd == "classify") {
      std::printf("  derived: retryability=%s rollback=%s compensation=%s terminality=%s\n",
                  to_string(rec.retryability), to_string(rec.rollback), to_string(rec.compensation), to_string(rec.terminality));
      return 0;
    }
    RetryClassifier cr; RetryInput in; in.idempotent_retry_possible = true; in.attempt_count = 1; in.worker_healthy = true;
    RetryDecision d = cr.classify(rec, in, RetryPolicy{});
    RecoveryPlanner pl; PlanInput pi; pi.idempotent_retry_possible = true; pi.rollback_possible = true;
    RecoveryPlan plan = pl.plan(rec, pi);
    std::printf("%s\n", explain_decision(rec, d, plan).c_str());
    std::printf("  JSON: %s\n", explain_decision_json(rec, d, plan).c_str());
    return 0;
  }
  if (cmd == "rollback") {
    RollbackPlan plan;
    plan.operation = OperationId::from_hex("cafe0001");
    plan.generation = RecoveryGeneration(1);
    plan.steps.push_back({RollbackAction::RELEASE_RESERVATION, "res-1", true, false, false, ""});
    plan.steps.push_back({RollbackAction::FREE_ALLOCATION, "alloc-1", true, false, false, ""});
    RollbackExecutor ex(std::move(plan));
    while (!ex.is_complete()) {
      bool ok = ex.run_next(true);
      std::printf("  rollback step %zu/%zu %s\n", ex.completed(), ex.plan().steps.size(), ok ? "ok" : "blocked");
      if (!ok) { std::printf("  rollback failed: %s (record RecoveryFailure)\n", ex.failure_reason().c_str()); break; }
    }
    std::printf("  rollback %s\n", ex.is_complete() ? "complete (monotonic)" : "interrupted");
    return ex.is_complete() ? 0 : 1;
  }
  if (cmd == "compensate") {
    CompensationRecord rec;
    rec.compensation_id = CompensationId::generate();
    rec.target_operation = OperationId::from_hex("cafe0002");
    rec.reason = "external side effect escaped; logical adjustment required";
    Compensator comp(std::move(rec));
    comp.run(true);
    std::printf("  compensation %s side_effect=%s attempts=%u\n", comp.is_complete() ? "complete" : "failed",
                to_string(comp.record().side_effects), comp.record().attempts);
    return comp.is_complete() ? 0 : 1;
  }
  if (cmd == "owner") {
    RecoveryOwnership own;
    std::mt19937_64 rng(2);
    AuthorityEnvelope a = demo_auth(1);
    bool adopted = own.adopt(RecoveryOwner::COORDINATOR, RecoveryGeneration(1), WorkerBootId::generate(), a);
    std::printf("  adopt=%d owner=%s gen=%u\n", adopted ? 1 : 0, to_string(own.owner()), (unsigned)own.generation().value);
    bool transferred = own.transfer(RecoveryOwner::REPLACEMENT_WORKER, RecoveryGeneration(2), WorkerBootId::generate(), a);
    std::printf("  transfer->replacement worker (gen2) =%d owner=%s gen=%u\n", transferred ? 1 : 0, to_string(own.owner()), (unsigned)own.generation().value);
    bool stale = own.transfer(RecoveryOwner::OPERATOR, RecoveryGeneration(1), WorkerBootId::generate(), a);
    std::printf("  stale transfer (gen1 <= gen2) rejected=%d\n", !stale);
    return (adopted && transferred && !stale) ? 0 : 1;
  }
  if (cmd == "snapshot") {
    std::printf("  store: %zu failures, %zu ops, ownership owner=%s gen=%u\n",
                store.failure_count(), store.ops().size(), to_string(store.ownership().owner()), (unsigned)store.ownership().generation().value);
    return 0;
  }
  if (cmd == "save" || cmd == "reload" || cmd == "replay") {
    std::string file = argc > 2 ? argv[2] : "ff_state.bin";
    if (cmd == "save") {
      std::mt19937_64 rng(3);
      OperationId op = OperationId::from_hex("00dd00ee");
      AuthorityEnvelope a = demo_auth(1);
      store.ops().create(op, a); store.ops().dispatch(op, a); store.ops().run(op, a);
      store.ops().report_completion(op, a, false); store.ops().confirm_completion(op, a);
      FailureRecordBuilder b; b.failure_class(FailureClass::PARTIAL_SUCCESS).operation(op).attempt(a.attempt).authority(a);
      store.append_failure(b.build(rng));
      bool ok = store.save(file);
      std::printf("  save %s -> %s\n", file.c_str(), ok ? "ok" : "failed");
      return ok ? 0 : 1;
    }
    bool ok = store.load(file);
    std::printf("  load %s -> %s\n", file.c_str(), ok ? "ok" : "corrupt/unsupported");
    if (!ok) return 1;
    if (cmd == "replay") {
      for (auto& op : store.ops().operations())
        std::printf("  op %s terminal=%s state=%s\n", op.to_display().c_str(),
                    store.ops().find(op) && store.ops().find(op)->terminal_recorded ? "yes" : "no",
                    to_string(store.ops().state(op)));
      return 0;
    }
    return 0;
  }
  if (cmd == "serve") {
    uint16_t port = argc > 2 ? (uint16_t)std::atoi(argv[2]) : 57321;
    std::string c = "\"" + sibling("ff_coordinator.exe") + "\" --port " + std::to_string(port) + " --state ff_coord_state.bin";
    return shell(c.c_str());
  }
  if (cmd == "worker") {
    uint16_t port = argc > 2 ? (uint16_t)std::atoi(argv[2]) : 57321;
    std::string mode = argc > 3 ? argv[3] : "normal";
    std::string c = "\"" + sibling("ff_worker.exe") + "\" 127.0.0.1 " + std::to_string(port) + " . " + mode;
    return shell(c.c_str());
  }
  if (cmd == "multiprocess") {
    std::string c = "\"" + sibling("test_multiprocess.exe") + "\"";
    return shell(c.c_str());
  }
  if (cmd == "cuda") {
    std::string c = "\"" + sibling("ff_cuda_proof.exe") + "\"";
    return shell(c.c_str());
  }
  if (cmd == "benchmark") {
    int n = argc > 2 ? std::atoi(argv[2]) : 100000;
    std::string c = "\"" + sibling("ff_benchmark.exe") + "\" " + std::to_string(n);
    return shell(c.c_str());
  }
  if (cmd == "inspect" || cmd == "operation" || cmd == "attempt" || cmd == "failure" || cmd == "explain") {
    if (argc < 3) { std::printf("need an id (hex)\n"); return 2; }
    std::string idhex = argv[2];
    for (auto& f : store.failures()) {
      if (f.failure_id.to_hex().substr(0, idhex.size()) == idhex) {
        std::printf("%s\n", explain_failure(f).c_str());
        return 0;
      }
    }
    std::printf("  (no persisted failures; showing a demo explaining a TRANSIENT failure)\n");
    std::mt19937_64 rng(4);
    FailureRecordBuilder b; b.failure_class(FailureClass::TRANSIENT).operation(OperationId::generate(rng)).attempt(AttemptId::generate(rng)).authority(demo_auth(1));
    FailureRecord rec = b.build(rng);
    std::printf("%s\n", explain_failure(rec).c_str());
    return 0;
  }
  usage();
  return 0;
}
