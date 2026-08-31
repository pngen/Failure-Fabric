#include "failure_fabric/operation.hpp"

namespace ff {
namespace {
// Transition table encoded as a 20-bit mask per source state. bit i set means
// transition to index i is allowed.
uint32_t make_mask(const std::initializer_list<int>& idx) {
  uint32_t m = 0;
  for (int i : idx) m |= (1u << i);
  return m;
}
// state index order (must match enum.hpp):
// 0 CREATED,1 DISPATCHED,2 RUNNING,3 COMPLETION_REPORTED,4 COMPLETION_CONFIRMED,
// 5 FAILED_KNOWN,6 FAILED_AMBIGUOUS,7 PARTIALLY_SUCCEEDED,8 RETRY_PENDING,9 RETRYING,
// 10 ROLLBACK_PENDING,11 ROLLING_BACK,12 COMPENSATION_PENDING,13 COMPENSATING,
// 14 RECOVERY_PENDING,15 RECOVERING,16 RECOVERED,17 TERMINAL_FAILED,18 CANCELLED,19 ABANDONED
struct Table { uint32_t m[20]; };
const Table& table() {
  static const Table T{{
    // CREATED
    make_mask({1, 5, 6}),
    // DISPATCHED
    make_mask({2, 5, 6, 18}),
    // RUNNING
    make_mask({3, 5, 6, 7, 18}),
    // COMPLETION_REPORTED
    make_mask({4, 5, 6, 7}),
    // COMPLETION_CONFIRMED (terminal)
    make_mask({}),
    // FAILED_KNOWN
    make_mask({8, 10, 12, 14, 17, 18, 19}),
    // FAILED_AMBIGUOUS
    make_mask({8, 10, 12, 14, 17, 19}),
    // PARTIALLY_SUCCEEDED
    make_mask({8, 10, 12, 14, 17}),
    // RETRY_PENDING
    make_mask({9, 18, 19}),
    // RETRYING
    make_mask({2, 5, 6, 18}),
    // ROLLBACK_PENDING
    make_mask({11, 18, 19}),
    // ROLLING_BACK
    make_mask({8, 12, 14, 16, 17}),
    // COMPENSATION_PENDING
    make_mask({13, 18, 19}),
    // COMPENSATING
    make_mask({14, 16, 17}),
    // RECOVERY_PENDING
    make_mask({15, 17, 19}),
    // RECOVERING
    make_mask({16, 17, 18}),
    // RECOVERED (terminal)
    make_mask({}),
    // TERMINAL_FAILED (terminal)
    make_mask({}),
    // CANCELLED (terminal)
    make_mask({}),
    // ABANDONED (terminal)
    make_mask({}),
  }};
  return T;
}
}

bool OperationStateMachine::is_allowed(OperationState from, OperationState to) noexcept {
  int f = static_cast<int>(from);
  int t = static_cast<int>(to);
  if (f < 0 || f >= 20 || t < 0 || t >= 20) return false;
  return (table().m[f] & (1u << t)) != 0;
}

bool OperationStateMachine::is_terminal(OperationState s) noexcept {
  return s == OperationState::COMPLETION_CONFIRMED || s == OperationState::RECOVERED ||
         s == OperationState::TERMINAL_FAILED || s == OperationState::CANCELLED ||
         s == OperationState::ABANDONED;
}

bool OperationStateMachine::is_failed(OperationState s) noexcept {
  return s == OperationState::FAILED_KNOWN || s == OperationState::FAILED_AMBIGUOUS ||
         s == OperationState::PARTIALLY_SUCCEEDED || s == OperationState::TERMINAL_FAILED;
}

OperationState OperationStateMachine::index_to_state(int i) {
  static const OperationState states[20] = {
    OperationState::CREATED, OperationState::DISPATCHED, OperationState::RUNNING,
    OperationState::COMPLETION_REPORTED, OperationState::COMPLETION_CONFIRMED,
    OperationState::FAILED_KNOWN, OperationState::FAILED_AMBIGUOUS,
    OperationState::PARTIALLY_SUCCEEDED, OperationState::RETRY_PENDING,
    OperationState::RETRYING, OperationState::ROLLBACK_PENDING,
    OperationState::ROLLING_BACK, OperationState::COMPENSATION_PENDING,
    OperationState::COMPENSATING, OperationState::RECOVERY_PENDING,
    OperationState::RECOVERING, OperationState::RECOVERED,
    OperationState::TERMINAL_FAILED, OperationState::CANCELLED, OperationState::ABANDONED
  };
  if (i < 0 || i >= 20) return OperationState::CREATED; // unreachable fallback
  return states[i];
}
} // namespace ff