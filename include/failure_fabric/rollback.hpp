#pragma once
// Typed rollback plans. Copyright 2026 Summon Software Labs.
#include "failure_fabric/enum.hpp"
#include "failure_fabric/id.hpp"
#include "failure_fabric/authority.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace ff {

struct RollbackStep {
  RollbackAction action = RollbackAction::RELEASE_RESERVATION;
  std::string    target;
  bool           idempotent = true;
  bool           executed = false;
  bool           failed = false;
  std::string    detail;

  std::string describe() const {
    std::string s = to_string(action);
    if (!target.empty()) s += ":" + target;
    return s;
  }
};

struct RollbackPlan {
  OperationId         operation{};
  RecoveryGeneration  generation{};
  RecoveryOwner       owner = RecoveryOwner::COORDINATOR;
  AuthorityEnvelope   authority{};
  std::vector<RollbackStep> steps;
};

class RollbackExecutor {
public:
  explicit RollbackExecutor(RollbackPlan plan) : plan_(std::move(plan)) {}
  const RollbackPlan& plan() const { return plan_; }
  size_t completed() const { return completed_; }
  bool is_complete() const { return completed_ >= plan_.steps.size(); }
  bool is_failed() const { return failed_; }
  const std::string& failure_reason() const { return failure_reason_; }
  bool run_next(bool authority_current);
  void resume_from(size_t completed) { completed_ = completed; }
private:
  RollbackPlan plan_;
  size_t completed_ = 0;
  bool failed_ = false;
  std::string failure_reason_;
};

} // namespace ff
