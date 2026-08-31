#pragma once
// Guarded operation state machine. Copyright 2026 Summon Software Labs.
#include "failure_fabric/enum.hpp"
#include <cstdint>

namespace ff {

// Guards all transitions. Illegal transitions are rejected; a late completion
// that references older attempt authority is rejected at the registry layer
// (see operation_registry) before this machine can accept it, so state can
// never jump over newer authority.
class OperationStateMachine {
public:
  explicit OperationStateMachine(OperationState s = OperationState::CREATED) : state_(s) {}
  OperationState state() const noexcept { return state_; }

  bool can_transition(OperationState to) const noexcept { return is_allowed(state_, to); }
  bool transition(OperationState to) noexcept {
    if (!can_transition(to)) return false;
    state_ = to;
    return true;
  }
  bool reset(OperationState s) noexcept { state_ = s; return true; }

  static bool is_allowed(OperationState from, OperationState to) noexcept;
  static bool is_terminal(OperationState s) noexcept;
  static bool is_failed(OperationState s) noexcept;
  static int count() noexcept { return 20; }
  static OperationState index_to_state(int i);
private:
  OperationState state_;
};

} // namespace ff
