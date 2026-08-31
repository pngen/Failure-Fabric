#pragma once
// Compensation semantics, separate from rollback. Copyright 2026 Summon Software Labs.
#include "failure_fabric/enum.hpp"
#include "failure_fabric/id.hpp"
#include "failure_fabric/authority.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace ff {

// Compensation is NOT rollback: it restores *logical* correctness when a side
// effect cannot be undone physically (external side effect already escaped, an
// accounting adjustment, or a replacement action that restores equilibrium).
// Completion of a compensation action itself.
enum class CompletionState : uint32_t { PENDING, COMPLETE, FAILURE };

struct CompensationRecord {
  CompensationId        compensation_id{};
  OperationId           target_operation{};
  std::string           reason;              // why compensation, not rollback
  std::string           preconditions;       // machine-readable precondition summary
  AuthorityEnvelope     authority{};
  SideEffectState       side_effects = SideEffectState::UNKNOWN;
  CompletionState       completion = CompletionState::PENDING;
  bool                  failed = false;
  std::string           failure_reason;
  uint32_t              attempts = 0;
};


class Compensator {
public:
  explicit Compensator(CompensationRecord rec) : rec_(std::move(rec)) {}
  const CompensationRecord& record() const { return rec_; }
  bool is_complete() const { return rec_.completion == CompletionState::COMPLETE; }
  bool is_failed() const { return rec_.completion == CompletionState::FAILURE; }
  const std::string& failure_reason() const { return rec_.failure_reason; }

  // Run compensation. Monotonic: once COMPLETE or FAILURE, no further runs change it.
  // authority_current gates mutation under fencing.
  void run(bool authority_current);
  void resume_failure(const std::string& reason);

private:
  CompensationRecord rec_;
};

} // namespace ff