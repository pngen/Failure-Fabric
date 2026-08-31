#include "failure_fabric/operation_registry.hpp"
#include "failure_fabric/enum.hpp"
#include <vector>

namespace ff {
const char* to_string(ApplyResult v) noexcept {
  switch (v) {
    case ApplyResult::OK: return "OK";
    case ApplyResult::STALE_REJECTED: return "STALE_REJECTED";
    case ApplyResult::ILLEGAL_TRANSITION: return "ILLEGAL_TRANSITION";
    case ApplyResult::ALREADY_TERMINAL: return "ALREADY_TERMINAL";
    case ApplyResult::CONFLICT: return "CONFLICT";
    case ApplyResult::UNKNOWN_OPERATION: return "UNKNOWN_OPERATION";
    case ApplyResult::FENCED: return "FENCED";
    case ApplyResult::ALREADY_REPORTED: return "ALREADY_REPORTED";
  }
  return "UNKNOWN";
}

ApplyResult OperationRegistry::create(const OperationId& op, const AuthorityEnvelope& initial) {
  std::lock_guard<std::mutex> lk(mu_);
  if (ops_.count(op)) return ApplyResult::ALREADY_TERMINAL;
  OperationRuntime r;
  r.authority = initial;
  r.machine.reset(OperationState::CREATED);
  ops_.emplace(op, std::move(r));
  return ApplyResult::OK;
}

ApplyResult OperationRegistry::dispatch(const OperationId& op, const AuthorityEnvelope& attempt) {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = ops_.find(op);
  if (it == ops_.end()) return ApplyResult::UNKNOWN_OPERATION;
  OperationRuntime& r = it->second;
  if (r.terminal_recorded) return ApplyResult::ALREADY_TERMINAL;
  r.authority = attempt;
  OperationState s = r.machine.state();
  // A fresh attempt may re-dispatch any non-terminal operation (worker restart,
  // recovery, retry). Terminal operations can never leave their terminal state.
  if (OperationStateMachine::is_terminal(s)) return ApplyResult::ILLEGAL_TRANSITION;
  if (s != OperationState::DISPATCHED) {
    r.machine.reset(OperationState::DISPATCHED);
  }
  ++r.attempt_count;
  return ApplyResult::OK;
}

ApplyResult OperationRegistry::run(const OperationId& op, const AuthorityEnvelope& attempt) {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = ops_.find(op);
  if (it == ops_.end()) return ApplyResult::UNKNOWN_OPERATION;
  OperationRuntime& r = it->second;
  if (r.terminal_recorded) return ApplyResult::ALREADY_TERMINAL;
  if (!fence_failure(attempt, r.authority).accepted) return ApplyResult::STALE_REJECTED;
  if (!r.machine.transition(OperationState::RUNNING)) return ApplyResult::ILLEGAL_TRANSITION;
  return ApplyResult::OK;
}

ApplyResult OperationRegistry::report_completion(const OperationId& op, const AuthorityEnvelope& attempt, bool partial) {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = ops_.find(op);
  if (it == ops_.end()) return ApplyResult::UNKNOWN_OPERATION;
  OperationRuntime& r = it->second;
  if (r.terminal_recorded) return ApplyResult::ALREADY_TERMINAL;
  if (!fence_completion(attempt, r.authority, r.terminal).accepted) return ApplyResult::STALE_REJECTED;
  if (r.machine.state() == OperationState::COMPLETION_REPORTED || r.machine.state() == OperationState::COMPLETION_CONFIRMED)
    return ApplyResult::ALREADY_REPORTED;
  if (!r.machine.transition(partial ? OperationState::PARTIALLY_SUCCEEDED : OperationState::COMPLETION_REPORTED))
    return ApplyResult::ILLEGAL_TRANSITION;
  return ApplyResult::OK;
}

ApplyResult OperationRegistry::confirm_completion(const OperationId& op, const AuthorityEnvelope& attempt) {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = ops_.find(op);
  if (it == ops_.end()) return ApplyResult::UNKNOWN_OPERATION;
  OperationRuntime& r = it->second;
  if (r.terminal_recorded) return ApplyResult::ALREADY_TERMINAL;
  if (!fence_completion(attempt, r.authority, r.terminal).accepted) return ApplyResult::STALE_REJECTED;
  if (!r.machine.transition(OperationState::COMPLETION_CONFIRMED)) return ApplyResult::ILLEGAL_TRANSITION;
  r.terminal_recorded = true;
  r.terminal = attempt;
  r.terminal_outcome = "COMPLETED";
  return ApplyResult::OK;
}

ApplyResult OperationRegistry::report_failure(const OperationId& op, const AuthorityEnvelope& attempt, bool ambiguous) {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = ops_.find(op);
  if (it == ops_.end()) return ApplyResult::UNKNOWN_OPERATION;
  OperationRuntime& r = it->second;
  if (r.terminal_recorded) return ApplyResult::ALREADY_TERMINAL;
  if (!fence_failure(attempt, r.authority).accepted) return ApplyResult::STALE_REJECTED;
  if (!r.machine.transition(ambiguous ? OperationState::FAILED_AMBIGUOUS : OperationState::FAILED_KNOWN))
    return ApplyResult::ILLEGAL_TRANSITION;
  return ApplyResult::OK;
}

ApplyResult OperationRegistry::transition(const OperationId& op, OperationState to, const AuthorityEnvelope& attempt) {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = ops_.find(op);
  if (it == ops_.end()) return ApplyResult::UNKNOWN_OPERATION;
  OperationRuntime& r = it->second;
  if (r.terminal_recorded) return ApplyResult::ALREADY_TERMINAL;
  if (!fence_failure(attempt, r.authority).accepted) return ApplyResult::STALE_REJECTED;
  if (!r.machine.transition(to)) return ApplyResult::ILLEGAL_TRANSITION;
  return ApplyResult::OK;
}

ApplyResult OperationRegistry::record_terminal(const OperationId& op, const AuthorityEnvelope& attempt, const std::string& outcome) {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = ops_.find(op);
  if (it == ops_.end()) return ApplyResult::UNKNOWN_OPERATION;
  OperationRuntime& r = it->second;
  if (r.terminal_recorded) {
    if (r.terminal_outcome == outcome) return ApplyResult::OK;
    return ApplyResult::ALREADY_TERMINAL;
  }
  if (!fence_failure(attempt, r.authority).accepted) return ApplyResult::STALE_REJECTED;
  bool ok = r.machine.transition(OperationState::TERMINAL_FAILED) ||
            r.machine.transition(OperationState::CANCELLED) ||
            r.machine.transition(OperationState::RECOVERED) ||
            r.machine.transition(OperationState::ABANDONED) ||
            r.machine.transition(OperationState::COMPLETION_CONFIRMED);
  if (!ok) return ApplyResult::ILLEGAL_TRANSITION;
  r.terminal_recorded = true;
  r.terminal = attempt;
  r.terminal_outcome = outcome;
  return ApplyResult::OK;
}

bool OperationRegistry::has(const OperationId& op) const { std::lock_guard<std::mutex> lk(mu_); return ops_.count(op) != 0; }
OperationState OperationRegistry::state(const OperationId& op) const {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = ops_.find(op);
  return it == ops_.end() ? OperationState::CREATED : it->second.machine.state();
}
const AuthorityEnvelope& OperationRegistry::current_authority(const OperationId& op) const {
  std::lock_guard<std::mutex> lk(mu_);
  static const AuthorityEnvelope empty{};
  auto it = ops_.find(op);
  return it == ops_.end() ? empty : it->second.authority;
}
const OperationRuntime* OperationRegistry::find(const OperationId& op) const {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = ops_.find(op);
  return it == ops_.end() ? nullptr : &it->second;
}
std::vector<OperationId> OperationRegistry::operations() const {
  std::lock_guard<std::mutex> lk(mu_);
  std::vector<OperationId> v; v.reserve(ops_.size());
  for (auto& p : ops_) v.push_back(p.first);
  return v;
}

bool OperationRegistry::restore(const OperationId& op, OperationRuntime&& rt) {
  std::lock_guard<std::mutex> lk(mu_);
  if (ops_.count(op)) return false;
  ops_.emplace(op, std::move(rt));
  return true;
}
} // namespace ff