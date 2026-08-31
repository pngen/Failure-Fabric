#include "failure_fabric/ff.hpp"
#include "failure_fabric/protocol.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <chrono>

// Coordinator OS process: real framed-TCP runtime for the distributed failure
// proof. It accepts worker registrations, dispatches operations, fenced by
// authority, classifies worker loss as ambiguity, transfers recovery ownership
// under a fresh generation, and confirms exactly one terminal outcome.
//
// Usage: ff_coordinator --port N --state FILE
using namespace ff;

struct Conn {
  TcpSocket sock;
  bool worker = false;
  WorkerId wid{};
  WorkerBootId boot{};
  bool running_op = false;
};

static std::mutex g_mu;
static FailureStore g_store;
static std::unordered_map<WorkerId, std::shared_ptr<Conn>> g_workers;
static std::vector<std::shared_ptr<Conn>> g_reg_order;   // registration order (first = A)
static std::shared_ptr<Conn> g_running_conn;   // conn currently running the dispatched op
static OperationId g_current_op;
static bool g_finished = false;
static std::string g_statefile;
static RecoveryOwner g_owner = RecoveryOwner::COORDINATOR;

static void save_state() { g_store.save(g_statefile); }

static void new_attempt(AuthorityEnvelope& fresh) {
  // fresh attempt for re-dispatch: new attempt id + generation + boot
  fresh.attempt = AttemptId::generate();
  fresh.attempt_generation = fresh.attempt_generation.next();
  fresh.operation_generation = fresh.operation_generation.next();
  fresh.failure_generation = fresh.failure_generation.next();
  fresh.recovery_generation = fresh.recovery_generation.next();

}

static void dispatch_to(const std::shared_ptr<Conn>& conn, const OperationId& op,
                        const AuthorityEnvelope& authority) {
  WireMessage run;
  run.type = ProtocolMessage::DISPATCH;
  run.op = op; run.attempt = authority.attempt;
  run.authority = authority;
  run.worker = conn->wid;
  conn->sock.send(run);
  std::lock_guard<std::mutex> lk(g_mu);
  conn->running_op = true;
  g_running_conn = conn;
  g_current_op = op;
}

static void handle_message(const std::shared_ptr<Conn>& conn, const WireMessage& m) {
  switch (m.type) {
    case ProtocolMessage::REGISTER: {
      std::lock_guard<std::mutex> lk(g_mu);
      conn->worker = true;
      conn->wid = m.worker.is_null() ? WorkerId::generate() : m.worker;
      conn->boot = m.boot;
      g_workers[conn->wid] = conn;
      g_reg_order.push_back(conn);
      WireMessage ack;
      ack.type = ProtocolMessage::REGISTER_ACK;
      ack.worker = conn->wid;
      conn->sock.send(ack);
      std::printf("[coord] registered worker %s (boot %s)\n", conn->wid.to_display().c_str(), conn->boot.to_display().c_str());
      break;
    }
    case ProtocolMessage::DISPATCH: {
      // control client requests dispatch of m.op to the first registered worker.
      OperationId op = m.op;
      AuthorityEnvelope auth;
      auth.coordinator_epoch = CoordinatorEpoch(1);
      auth.attempt = AttemptId::generate();
      auth.attempt_generation = AttemptGeneration(1);
      auth.operation_generation = OperationGeneration(1);
      auth.worker_boot = g_workers.empty() ? WorkerBootId{} : g_workers.begin()->second->boot;
      std::shared_ptr<Conn> target;
      {
        std::lock_guard<std::mutex> lk(g_mu);
        if (g_workers.empty()) { std::printf("[coord] no workers\n"); return; }
        target = g_workers.begin()->second;
        g_store.ops().create(op, auth);
        g_store.ops().dispatch(op, auth);
      }
      dispatch_to(target, op, auth);
      g_store.ops().run(op, auth);
      std::printf("[coord] dispatched %s to worker %s attempt_gen %llu\n",
                  op.to_display().c_str(), target->wid.to_display().c_str(),
                  static_cast<unsigned long long>(auth.attempt_generation.value));
      break;
    }
    case ProtocolMessage::RUNNING: {
      std::lock_guard<std::mutex> lk(g_mu);
      g_store.ops().run(m.op, m.authority);
      break;
    }
    case ProtocolMessage::COMPLETE: {
      ApplyResult fr = g_store.ops().report_completion(m.op, m.authority, false);
      if (fr == ApplyResult::OK) {
        g_store.ops().confirm_completion(m.op, m.authority);
        WireMessage ack; ack.type = ProtocolMessage::ACK; ack.flag = true; ack.op = m.op;
        conn->sock.send(ack);
        std::lock_guard<std::mutex> lk(g_mu);
        g_finished = true;
        save_state();
        std::printf("[coord] COMPLETE accepted -> terminal COMPLETED for %s\n", m.op.to_display().c_str());
      } else {
        WireMessage ack; ack.type = ProtocolMessage::FENCE; ack.flag = false;
        ack.op = m.op; ack.text = "stale completion rejected";
        conn->sock.send(ack);
        std::printf("[coord] COMPLETE REJECTED (%s) -> stale authority fenced\n", to_string(fr));
      }
      break;
    }
    case ProtocolMessage::FAILURE: {
      std::lock_guard<std::mutex> lk(g_mu);
      g_store.ops().report_failure(m.op, m.authority, m.flag);
      std::printf("[coord] FAILURE recorded for %s (ambiguous=%d)\n", m.op.to_display().c_str(), m.flag ? 1 : 0);
      break;
    }
    default: break;
  }
}

static void worker_thread(std::shared_ptr<Conn> conn) {
  // Read loop until the worker disconnects (recv returns false on close).
  while (true) {
    WireMessage m;
    if (!conn->sock.recv(m)) break; // disconnect: worker died or closed
    handle_message(conn, m);
  }
  // Worker lost. If it was running the current op, classify as AMBIGUOUS and
  // recover: fence its boot, transfer ownership, re-dispatch under a fresh
  // attempt to another worker.
  bool was_running = false;
  {
    std::lock_guard<std::mutex> lk(g_mu);
    was_running = (conn == g_running_conn);
    if (was_running) {
      g_workers.erase(conn->wid);
      g_running_conn.reset();
    }
  }
  if (was_running && !g_finished) {
    std::printf("[coord] worker %s lost while running %s -> AMBIGUOUS + recovery\n",
                conn->wid.to_display().c_str(), g_current_op.to_display().c_str());
    // Classify ambiguous (side effects unknown), fence stale boot.
    AuthorityEnvelope cur;
    {
      std::lock_guard<std::mutex> lk(g_mu);
      cur = g_store.ops().current_authority(g_current_op);
      // fence the lost boot; transfer ownership to a fresh replacement generation.
      g_store.ownership().transfer(RecoveryOwner::REPLACEMENT_WORKER, RecoveryGeneration(cur.recovery_generation.next()), WorkerBootId::generate(), cur);
    }
    // record an ambiguous failure record, immutable
    FailureRecordBuilder b;
    b.failure_class(FailureClass::AMBIGUOUS)
      .operation(g_current_op).attempt(cur.attempt)
      .origin(FailureOrigin::WORKER).phase(FailurePhase::ACK)
      .completion(Ambiguity::AMBIGUOUS).side_effect_state(SideEffectState::MAY_OCCURRED)
      .unknown_side_effects(1).authority(cur).timestamp(static_cast<uint64_t>(::std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()));
    {
      std::lock_guard<std::mutex> lk(g_mu);
      g_store.append_failure(b.build());
      g_store.ops().report_failure(g_current_op, cur, true);
    }
    // Re-dispatch with a fresh attempt to the next registered worker.
    std::shared_ptr<Conn> replacement;
    AuthorityEnvelope fresh = cur;
    new_attempt(fresh);
    {
      std::lock_guard<std::mutex> lk(g_mu);
      replacement = g_workers.empty() ? nullptr : (g_reg_order.empty() ? nullptr : g_reg_order[0]);
        if (replacement && !g_workers.count(replacement->wid)) replacement = g_workers.empty() ? nullptr : g_workers.begin()->second;
      if (replacement) g_store.ops().dispatch(g_current_op, fresh);
    }
    if (replacement && !OperationStateMachine::is_terminal(g_store.ops().state(g_current_op))) {
      dispatch_to(replacement, g_current_op, fresh);
      std::printf("[coord] recovery: re-dispatched %s to replacement worker, attempt_gen %llu\n",
                  g_current_op.to_display().c_str(), static_cast<unsigned long long>(fresh.attempt_generation.value));
    } else {
      std::printf("[coord] recovery: no replacement worker available\n");
    }
  }
}

int main(int argc, char** argv) {
  uint16_t port = 5000;
  g_statefile = "ff_coord_state.bin";
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--port" && i + 1 < argc) port = static_cast<uint16_t>(std::atoi(argv[++i]));
    else if (a == "--state" && i + 1 < argc) g_statefile = argv[++i];
  }
  if (!winsock_init()) { std::fprintf(stderr, "coord: winsock init failed\n"); return 2; }
  // best-effort reload of a persisted state (recovery from crash)
  g_store.load(g_statefile);

  TcpListener listener;
  if (!listener.listen(port)) { std::fprintf(stderr, "coord: listen failed on port %u\n", port); return 2; }
  std::printf("[coord] listening on port %u, state=%s\n", port, g_statefile.c_str());

  for (;;) {
    TcpSocket cli = listener.accept();
    if (!cli.valid()) { std::this_thread::sleep_for(std::chrono::milliseconds(20)); continue; }
    auto conn = std::make_shared<Conn>();
    conn->sock = std::move(cli);
    std::thread(worker_thread, conn).detach();
  }
  return 0;
}