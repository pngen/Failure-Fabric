#include "failure_fabric/rollback.hpp"

namespace ff {
bool RollbackExecutor::run_next(bool authority_current) {
  if (is_complete()) return true;
  if (!authority_current) {
    failed_ = true;
    failure_reason_ = "rollback blocked: recovery authority no longer current";
    return false;
  }
  RollbackStep& step = plan_.steps[completed_];
  step.executed = true;
  ++completed_;
  return true;
}
} // namespace ff
