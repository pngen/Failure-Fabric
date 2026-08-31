#pragma once
// Operation registry: ties the guarded state machine to authority fencing and
// exactly-once terminality. Copyright 2026 Summon Software Labs.
#include "failure_fabric/operation.hpp"
#include "failure_fabric/authority.hpp"
#include "failure_fabric/id.hpp"
#include "failure_fabric/idempotency.hpp"
#include <string>
#include <unordered_map>
#include <mutex>

namespace ff {

enum class ApplyResult : uint32_t {
  OK, STALE_REJECTED, ILLEGAL_TRANSITION, ALREADY_TERMINAL,
  CONFLICT, UNKNOWN_OPERATION, FENCED, ALREADY_REPORTED
};

const char* to_string(ApplyResult v) noexcept;

struct OperationRuntime {
  OperationStateMachine machine;
  AuthorityEnvelope     authority;     // current authoritative attempt envelope
  AuthorityEnvelope     terminal;      // set once a terminal outcome is recorded
  bool terminal_recorded = false;
  uint32_t attempt_count = 0;
  std::string terminal_outcome;
  bool idempotent = false;
  IdempotencyKey idem_key{};
};

class OperationRegistry {
public:
  // create a new operation (CREATED) with initial authority.
  ApplyResult create(const OperationId& op, const AuthorityEnvelope& initial);
  // dispatch onto an attempt: CREATED/DISPATCHED -> DISPATCHED; bind attempt authority.
  // A fresh attempt (new AttemptId/Generation/Boot) advances the current authority,
  // which fences any late completion from the prior attempt.
  ApplyResult dispatch(const OperationId& op, const AuthorityEnvelope& attempt);
  // Worker RUNNING signal: DISPATCHED -> RUNNING.
  ApplyResult run(const OperationId& op, const AuthorityEnvelope& attempt);
  // Completion report: fenced; COMPLETION_REPORTED (or PARTIALLY_SUCCEEDED).
  ApplyResult report_completion(const OperationId& op, const AuthorityEnvelope& attempt, bool partial);
  // Ack/conclusion: COMPLETION_REPORTED -> COMPLETION_CONFIRMED (terminal, once).
  ApplyResult confirm_completion(const OperationId& op, const AuthorityEnvelope& attempt);
  // Failure report: fenced; -> FAILED_KNOWN / FAILED_AMBIGUOUS.
  ApplyResult report_failure(const OperationId& op, const AuthorityEnvelope& attempt, bool ambiguous);
  // General authority-guarded transition.
  ApplyResult transition(const OperationId& op, OperationState to, const AuthorityEnvelope& attempt);
  // Record a terminal outcome exactly once. Idempotent only if already terminal with same outcome.
  ApplyResult record_terminal(const OperationId& op, const AuthorityEnvelope& attempt, const std::string& outcome);

  // Restore a persisted runtime snapshot directly (used by store reload).
  bool restore(const OperationId& op, OperationRuntime&& rt);

  bool has(const OperationId& op) const;
  OperationState state(const OperationId& op) const;
  const AuthorityEnvelope& current_authority(const OperationId& op) const;
  const OperationRuntime* find(const OperationId& op) const;
  size_t size() const { return ops_.size(); }
  std::vector<OperationId> operations() const;
  std::mutex& mutex() { return mu_; }
  void clear() { std::lock_guard<std::mutex> lk(mu_); ops_.clear(); }
private:
  mutable std::mutex mu_;
  std::unordered_map<OperationId, OperationRuntime> ops_;
};

} // namespace ff