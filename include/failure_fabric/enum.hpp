#pragma once
// orthogonal classification dimensions for failure semantics. Copyright 2026 Summon Software Labs.
#include <string_view>
#include <cstdint>

namespace ff {

// The set of failure classes. A class is one observable label; it is NOT the
// collapsed single classification of a failure. Retryability, ambiguity,
// side-effect state, authority status, terminality, and ownership are carried
// as independent orthogonal dimensions so a single failure cannot be
// mischaracterized by one enum value.
#define FF_ENUM_LIST_ITEM(e) e
enum class FailureClass : uint32_t {
  TRANSIENT, PERMANENT, RETRYABLE, NON_RETRYABLE, AMBIGUOUS, PARTIAL_SUCCESS,
  STALE_AUTHORITY, LOST_WORKER, LOST_DEVICE, TRANSPORT_FAILURE, TIMEOUT_OBSERVED,
  PROTOCOL_FAILURE, RESOURCE_EXHAUSTION, CORRUPTION, VALIDATION_FAILURE,
  INCOMPATIBILITY, CANCELLED, PREEMPTED, DEPENDENCY_FAILURE, STORAGE_FAILURE,
  EXECUTION_FAILURE, RECOVERY_FAILURE, UNKNOWN
};
#undef FF_ENUM_LIST_ITEM

enum class FailureOrigin : uint32_t {
  COORDINATOR, WORKER, STORAGE, TRANSPORT, DEVICE, PROTOCOL, RESOURCE,
  VALIDATION, SCHEDULER, EXTERNAL, UNKNOWN
};

enum class Severity : uint32_t { INFO, WARNING, ERROR, CRITICAL };

enum class Retryability : uint32_t {
  RETRYABLE, NON_RETRYABLE, CONDITIONALLY_RETRYABLE, UNKNOWN
};

// completion is known / unknown / partial / ambiguous
enum class Ambiguity : uint32_t { KNOWN, PARTIAL, AMBIGUOUS, UNKNOWN_OUTCOME };

enum class SideEffectState : uint32_t {
  NONE_POSSIBLE, KNOWN_NOT_OCCURRED, MAY_OCCURRED, KNOWN_OCCURRED, UNKNOWN
};

enum class AuthorityStatus : uint32_t {
  AUTHORITATIVE, STALE, FENCED, SUPERSEDED, UNKNOWN
};

enum class RollbackRequirement : uint32_t { NONE, OPTIONAL, REQUIRED, UNAVAILABLE };
enum class CompensationRequirement : uint32_t { NONE, OPTIONAL, REQUIRED };

enum class RecoveryOwner : uint32_t {
  COORDINATOR, ORIGINAL_WORKER, REPLACEMENT_WORKER, REPLICA_MANAGER,
  STORAGE_SUBSYSTEM, TRANSFER_SUBSYSTEM, OPERATOR, EXTERNAL_SYSTEM, NONE
};

enum class Terminality : uint32_t { TERMINAL, TRANSIENT, PENDING, UNKNOWN };

enum class Provenance : uint32_t {
  FAILURE_FABRIC, WORKER_REPORT, COORDINATOR_INFERENCE, OPERATOR,
  EXTERNAL_SYSTEM, SYSTEM_HEURISTIC, UNKNOWN
};

enum class FailurePhase : uint32_t {
  SUBMIT, DISPATCH, EXECUTE, TRANSFER, STORE, COMMIT, ACK, RECOVERY,
  POST_RECOVERY, UNKNOWN
};

// operation semantics are independent bits that can combine.
enum class OperationSemantics : uint32_t {
  NONE             = 0,
  IDEMPOTENT       = 1 << 0,
  DEDUPLICATED     = 1 << 1,
  TRANSACTIONAL    = 1 << 2,
  COMPENSATABLE    = 1 << 3,
  NON_REPEATABLE   = 1 << 4,
  EXTERNALLY_COMMITTED = 1 << 5,
  UNKNOWN_SIDE_EFFECTS = 1 << 6,
};
inline OperationSemantics operator|(OperationSemantics a, OperationSemantics b) {
  return static_cast<OperationSemantics>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline OperationSemantics operator&(OperationSemantics a, OperationSemantics b) {
  return static_cast<OperationSemantics>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline bool has_semantics(OperationSemantics value, OperationSemantics flag) {
  return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
}

enum class OperationState : uint32_t {
  CREATED, DISPATCHED, RUNNING, COMPLETION_REPORTED, COMPLETION_CONFIRMED,
  FAILED_KNOWN, FAILED_AMBIGUOUS, PARTIALLY_SUCCEEDED, RETRY_PENDING, RETRYING,
  ROLLBACK_PENDING, ROLLING_BACK, COMPENSATION_PENDING, COMPENSATING,
  RECOVERY_PENDING, RECOVERING, RECOVERED, TERMINAL_FAILED, CANCELLED, ABANDONED
};

enum class RecoveryAction : uint32_t {
  RETRY, RETRY_ELSEWHERE, RESTART_WORKER, RESTART_DEVICE_CONTEXT, RELOAD_STATE,
  REHYDRATE_STATE, FAILOVER_REPLICA, ROLLBACK, COMPENSATE, INVALIDATE, RECOMPUTE,
  ABANDON, ESCALATE, MANUAL_RESOLUTION
};

enum class RollbackAction : uint32_t {
  RELEASE_RESERVATION, FREE_ALLOCATION, INVALIDATE_GENERATION, REVERT_METADATA,
  REMOVE_PARTIAL_STATE, RESTORE_SNAPSHOT, CANCEL_DEPENDENT, RELEASE_SERVING_AUTHORITY,
  MARK_ARTIFACT_INVALID, UNWIND_TRANSACTION
};

enum class ProtocolMessage : uint32_t {
  REGISTER, REGISTER_ACK, DISPATCH, RUNNING, COMPLETE, FAILURE, ACK, RETRY,
  ROLLBACK, COMPENSATE, RECOVERY_ASSIGN, RECOVERY_COMPLETE, CANCEL, FENCE,
  HEARTBEAT, SHUTDOWN
};

// ---- string conversion (deterministic, for explain/CLI/JSON) ----
const char* to_string(FailureClass v) noexcept;
const char* to_string(FailureOrigin v) noexcept;
const char* to_string(Severity v) noexcept;
const char* to_string(Retryability v) noexcept;
const char* to_string(Ambiguity v) noexcept;
const char* to_string(SideEffectState v) noexcept;
const char* to_string(AuthorityStatus v) noexcept;
const char* to_string(RollbackRequirement v) noexcept;
const char* to_string(CompensationRequirement v) noexcept;
const char* to_string(RecoveryOwner v) noexcept;
const char* to_string(Terminality v) noexcept;
const char* to_string(Provenance v) noexcept;
const char* to_string(FailurePhase v) noexcept;
const char* to_string(OperationState v) noexcept;
const char* to_string(RecoveryAction v) noexcept;
const char* to_string(RollbackAction v) noexcept;
const char* to_string(ProtocolMessage v) noexcept;

// reverse lookup (used by CLI parsing). Returns false when unknown.
bool from_string(FailureClass& out, std::string_view s);
bool from_string(OperationState& out, std::string_view s);
bool from_string(RecoveryAction& out, std::string_view s);
bool from_string(RecoveryOwner& out, std::string_view s);
bool from_string(Retryability& out, std::string_view s);
bool from_string(Ambiguity& out, std::string_view s);

} // namespace ff
