#include "failure_fabric/compensation.hpp"

namespace ff {
void Compensator::run(bool authority_current) {
  if (rec_.completion == CompletionState::COMPLETE) return;
  if (rec_.completion == CompletionState::FAILURE) return;
  if (!authority_current) {
    rec_.completion = CompletionState::FAILURE;
    rec_.failed = true;
    rec_.failure_reason = "compensation blocked: authority no longer current";
    return;
  }
  rec_.completion = CompletionState::COMPLETE;
  rec_.failed = false;
  rec_.side_effects = SideEffectState::KNOWN_NOT_OCCURRED;
  rec_.attempts += 1;
}
void Compensator::resume_failure(const std::string& reason) {
  rec_.completion = CompletionState::FAILURE;
  rec_.failed = true;
  rec_.failure_reason = reason;
}
} // namespace ff
