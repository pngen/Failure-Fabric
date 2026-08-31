#include "failure_fabric/enum.hpp"

namespace ff {
#define FF_TS(E, ...) const char* to_string(E v) noexcept { switch (v) { __VA_ARGS__ } return "UNKNOWN"; }

FF_TS(FailureClass,
  case FailureClass::TRANSIENT: return "TRANSIENT";
  case FailureClass::PERMANENT: return "PERMANENT";
  case FailureClass::RETRYABLE: return "RETRYABLE";
  case FailureClass::NON_RETRYABLE: return "NON_RETRYABLE";
  case FailureClass::AMBIGUOUS: return "AMBIGUOUS";
  case FailureClass::PARTIAL_SUCCESS: return "PARTIAL_SUCCESS";
  case FailureClass::STALE_AUTHORITY: return "STALE_AUTHORITY";
  case FailureClass::LOST_WORKER: return "LOST_WORKER";
  case FailureClass::LOST_DEVICE: return "LOST_DEVICE";
  case FailureClass::TRANSPORT_FAILURE: return "TRANSPORT_FAILURE";
  case FailureClass::TIMEOUT_OBSERVED: return "TIMEOUT_OBSERVED";
  case FailureClass::PROTOCOL_FAILURE: return "PROTOCOL_FAILURE";
  case FailureClass::RESOURCE_EXHAUSTION: return "RESOURCE_EXHAUSTION";
  case FailureClass::CORRUPTION: return "CORRUPTION";
  case FailureClass::VALIDATION_FAILURE: return "VALIDATION_FAILURE";
  case FailureClass::INCOMPATIBILITY: return "INCOMPATIBILITY";
  case FailureClass::CANCELLED: return "CANCELLED";
  case FailureClass::PREEMPTED: return "PREEMPTED";
  case FailureClass::DEPENDENCY_FAILURE: return "DEPENDENCY_FAILURE";
  case FailureClass::STORAGE_FAILURE: return "STORAGE_FAILURE";
  case FailureClass::EXECUTION_FAILURE: return "EXECUTION_FAILURE";
  case FailureClass::RECOVERY_FAILURE: return "RECOVERY_FAILURE";
  case FailureClass::UNKNOWN: return "UNKNOWN";
)

FF_TS(FailureOrigin,
  case FailureOrigin::COORDINATOR: return "COORDINATOR";
  case FailureOrigin::WORKER: return "WORKER";
  case FailureOrigin::STORAGE: return "STORAGE";
  case FailureOrigin::TRANSPORT: return "TRANSPORT";
  case FailureOrigin::DEVICE: return "DEVICE";
  case FailureOrigin::PROTOCOL: return "PROTOCOL";
  case FailureOrigin::RESOURCE: return "RESOURCE";
  case FailureOrigin::VALIDATION: return "VALIDATION";
  case FailureOrigin::SCHEDULER: return "SCHEDULER";
  case FailureOrigin::EXTERNAL: return "EXTERNAL";
  case FailureOrigin::UNKNOWN: return "UNKNOWN";
)
FF_TS(Severity,
  case Severity::INFO: return "INFO";
  case Severity::WARNING: return "WARNING";
  case Severity::ERROR: return "ERROR";
  case Severity::CRITICAL: return "CRITICAL";
)
FF_TS(Retryability,
  case Retryability::RETRYABLE: return "RETRYABLE";
  case Retryability::NON_RETRYABLE: return "NON_RETRYABLE";
  case Retryability::CONDITIONALLY_RETRYABLE: return "CONDITIONALLY_RETRYABLE";
  case Retryability::UNKNOWN: return "UNKNOWN";
)
FF_TS(Ambiguity,
  case Ambiguity::KNOWN: return "KNOWN";
  case Ambiguity::PARTIAL: return "PARTIAL";
  case Ambiguity::AMBIGUOUS: return "AMBIGUOUS";
  case Ambiguity::UNKNOWN_OUTCOME: return "UNKNOWN_OUTCOME";
)
FF_TS(SideEffectState,
  case SideEffectState::NONE_POSSIBLE: return "NONE_POSSIBLE";
  case SideEffectState::KNOWN_NOT_OCCURRED: return "KNOWN_NOT_OCCURRED";
  case SideEffectState::MAY_OCCURRED: return "MAY_OCCURRED";
  case SideEffectState::KNOWN_OCCURRED: return "KNOWN_OCCURRED";
  case SideEffectState::UNKNOWN: return "UNKNOWN";
)
FF_TS(AuthorityStatus,
  case AuthorityStatus::AUTHORITATIVE: return "AUTHORITATIVE";
  case AuthorityStatus::STALE: return "STALE";
  case AuthorityStatus::FENCED: return "FENCED";
  case AuthorityStatus::SUPERSEDED: return "SUPERSEDED";
  case AuthorityStatus::UNKNOWN: return "UNKNOWN";
)
FF_TS(RollbackRequirement,
  case RollbackRequirement::NONE: return "NONE";
  case RollbackRequirement::OPTIONAL: return "OPTIONAL";
  case RollbackRequirement::REQUIRED: return "REQUIRED";
  case RollbackRequirement::UNAVAILABLE: return "UNAVAILABLE";
)
FF_TS(CompensationRequirement,
  case CompensationRequirement::NONE: return "NONE";
  case CompensationRequirement::OPTIONAL: return "OPTIONAL";
  case CompensationRequirement::REQUIRED: return "REQUIRED";
)
FF_TS(RecoveryOwner,
  case RecoveryOwner::COORDINATOR: return "COORDINATOR";
  case RecoveryOwner::ORIGINAL_WORKER: return "ORIGINAL_WORKER";
  case RecoveryOwner::REPLACEMENT_WORKER: return "REPLACEMENT_WORKER";
  case RecoveryOwner::REPLICA_MANAGER: return "REPLICA_MANAGER";
  case RecoveryOwner::STORAGE_SUBSYSTEM: return "STORAGE_SUBSYSTEM";
  case RecoveryOwner::TRANSFER_SUBSYSTEM: return "TRANSFER_SUBSYSTEM";
  case RecoveryOwner::OPERATOR: return "OPERATOR";
  case RecoveryOwner::EXTERNAL_SYSTEM: return "EXTERNAL_SYSTEM";
  case RecoveryOwner::NONE: return "NONE";
)
FF_TS(Terminality,
  case Terminality::TERMINAL: return "TERMINAL";
  case Terminality::TRANSIENT: return "TRANSIENT";
  case Terminality::PENDING: return "PENDING";
  case Terminality::UNKNOWN: return "UNKNOWN";
)
FF_TS(Provenance,
  case Provenance::FAILURE_FABRIC: return "FAILURE_FABRIC";
  case Provenance::WORKER_REPORT: return "WORKER_REPORT";
  case Provenance::COORDINATOR_INFERENCE: return "COORDINATOR_INFERENCE";
  case Provenance::OPERATOR: return "OPERATOR";
  case Provenance::EXTERNAL_SYSTEM: return "EXTERNAL_SYSTEM";
  case Provenance::SYSTEM_HEURISTIC: return "SYSTEM_HEURISTIC";
  case Provenance::UNKNOWN: return "UNKNOWN";
)
FF_TS(FailurePhase,
  case FailurePhase::SUBMIT: return "SUBMIT";
  case FailurePhase::DISPATCH: return "DISPATCH";
  case FailurePhase::EXECUTE: return "EXECUTE";
  case FailurePhase::TRANSFER: return "TRANSFER";
  case FailurePhase::STORE: return "STORE";
  case FailurePhase::COMMIT: return "COMMIT";
  case FailurePhase::ACK: return "ACK";
  case FailurePhase::RECOVERY: return "RECOVERY";
  case FailurePhase::POST_RECOVERY: return "POST_RECOVERY";
  case FailurePhase::UNKNOWN: return "UNKNOWN";
)
FF_TS(OperationState,
  case OperationState::CREATED: return "CREATED";
  case OperationState::DISPATCHED: return "DISPATCHED";
  case OperationState::RUNNING: return "RUNNING";
  case OperationState::COMPLETION_REPORTED: return "COMPLETION_REPORTED";
  case OperationState::COMPLETION_CONFIRMED: return "COMPLETION_CONFIRMED";
  case OperationState::FAILED_KNOWN: return "FAILED_KNOWN";
  case OperationState::FAILED_AMBIGUOUS: return "FAILED_AMBIGUOUS";
  case OperationState::PARTIALLY_SUCCEEDED: return "PARTIALLY_SUCCEEDED";
  case OperationState::RETRY_PENDING: return "RETRY_PENDING";
  case OperationState::RETRYING: return "RETRYING";
  case OperationState::ROLLBACK_PENDING: return "ROLLBACK_PENDING";
  case OperationState::ROLLING_BACK: return "ROLLING_BACK";
  case OperationState::COMPENSATION_PENDING: return "COMPENSATION_PENDING";
  case OperationState::COMPENSATING: return "COMPENSATING";
  case OperationState::RECOVERY_PENDING: return "RECOVERY_PENDING";
  case OperationState::RECOVERING: return "RECOVERING";
  case OperationState::RECOVERED: return "RECOVERED";
  case OperationState::TERMINAL_FAILED: return "TERMINAL_FAILED";
  case OperationState::CANCELLED: return "CANCELLED";
  case OperationState::ABANDONED: return "ABANDONED";
)
FF_TS(RecoveryAction,
  case RecoveryAction::RETRY: return "RETRY";
  case RecoveryAction::RETRY_ELSEWHERE: return "RETRY_ELSEWHERE";
  case RecoveryAction::RESTART_WORKER: return "RESTART_WORKER";
  case RecoveryAction::RESTART_DEVICE_CONTEXT: return "RESTART_DEVICE_CONTEXT";
  case RecoveryAction::RELOAD_STATE: return "RELOAD_STATE";
  case RecoveryAction::REHYDRATE_STATE: return "REHYDRATE_STATE";
  case RecoveryAction::FAILOVER_REPLICA: return "FAILOVER_REPLICA";
  case RecoveryAction::ROLLBACK: return "ROLLBACK";
  case RecoveryAction::COMPENSATE: return "COMPENSATE";
  case RecoveryAction::INVALIDATE: return "INVALIDATE";
  case RecoveryAction::RECOMPUTE: return "RECOMPUTE";
  case RecoveryAction::ABANDON: return "ABANDON";
  case RecoveryAction::ESCALATE: return "ESCALATE";
  case RecoveryAction::MANUAL_RESOLUTION: return "MANUAL_RESOLUTION";
)
FF_TS(RollbackAction,
  case RollbackAction::RELEASE_RESERVATION: return "RELEASE_RESERVATION";
  case RollbackAction::FREE_ALLOCATION: return "FREE_ALLOCATION";
  case RollbackAction::INVALIDATE_GENERATION: return "INVALIDATE_GENERATION";
  case RollbackAction::REVERT_METADATA: return "REVERT_METADATA";
  case RollbackAction::REMOVE_PARTIAL_STATE: return "REMOVE_PARTIAL_STATE";
  case RollbackAction::RESTORE_SNAPSHOT: return "RESTORE_SNAPSHOT";
  case RollbackAction::CANCEL_DEPENDENT: return "CANCEL_DEPENDENT";
  case RollbackAction::RELEASE_SERVING_AUTHORITY: return "RELEASE_SERVING_AUTHORITY";
  case RollbackAction::MARK_ARTIFACT_INVALID: return "MARK_ARTIFACT_INVALID";
  case RollbackAction::UNWIND_TRANSACTION: return "UNWIND_TRANSACTION";
)
FF_TS(ProtocolMessage,
  case ProtocolMessage::REGISTER: return "REGISTER";
  case ProtocolMessage::REGISTER_ACK: return "REGISTER_ACK";
  case ProtocolMessage::DISPATCH: return "DISPATCH";
  case ProtocolMessage::RUNNING: return "RUNNING";
  case ProtocolMessage::COMPLETE: return "COMPLETE";
  case ProtocolMessage::FAILURE: return "FAILURE";
  case ProtocolMessage::ACK: return "ACK";
  case ProtocolMessage::RETRY: return "RETRY";
  case ProtocolMessage::ROLLBACK: return "ROLLBACK";
  case ProtocolMessage::COMPENSATE: return "COMPENSATE";
  case ProtocolMessage::RECOVERY_ASSIGN: return "RECOVERY_ASSIGN";
  case ProtocolMessage::RECOVERY_COMPLETE: return "RECOVERY_COMPLETE";
  case ProtocolMessage::CANCEL: return "CANCEL";
  case ProtocolMessage::FENCE: return "FENCE";
  case ProtocolMessage::HEARTBEAT: return "HEARTBEAT";
  case ProtocolMessage::SHUTDOWN: return "SHUTDOWN";
)
#undef FF_TS

#define FF_FS(T, ...)   bool from_string(T& out, std::string_view s) {     using E = T;     __VA_ARGS__    return false;   }
FF_FS(FailureClass,
  if (s == "TRANSIENT") { out = E::TRANSIENT; return true; }
  if (s == "PERMANENT") { out = E::PERMANENT; return true; }
  if (s == "RETRYABLE") { out = E::RETRYABLE; return true; }
  if (s == "NON_RETRYABLE") { out = E::NON_RETRYABLE; return true; }
  if (s == "AMBIGUOUS") { out = E::AMBIGUOUS; return true; }
  if (s == "PARTIAL_SUCCESS") { out = E::PARTIAL_SUCCESS; return true; }
  if (s == "STALE_AUTHORITY") { out = E::STALE_AUTHORITY; return true; }
  if (s == "LOST_WORKER") { out = E::LOST_WORKER; return true; }
  if (s == "LOST_DEVICE") { out = E::LOST_DEVICE; return true; }
  if (s == "TRANSPORT_FAILURE") { out = E::TRANSPORT_FAILURE; return true; }
  if (s == "TIMEOUT_OBSERVED") { out = E::TIMEOUT_OBSERVED; return true; }
  if (s == "PROTOCOL_FAILURE") { out = E::PROTOCOL_FAILURE; return true; }
  if (s == "RESOURCE_EXHAUSTION") { out = E::RESOURCE_EXHAUSTION; return true; }
  if (s == "CORRUPTION") { out = E::CORRUPTION; return true; }
  if (s == "VALIDATION_FAILURE") { out = E::VALIDATION_FAILURE; return true; }
  if (s == "INCOMPATIBILITY") { out = E::INCOMPATIBILITY; return true; }
  if (s == "CANCELLED") { out = E::CANCELLED; return true; }
  if (s == "PREEMPTED") { out = E::PREEMPTED; return true; }
  if (s == "DEPENDENCY_FAILURE") { out = E::DEPENDENCY_FAILURE; return true; }
  if (s == "STORAGE_FAILURE") { out = E::STORAGE_FAILURE; return true; }
  if (s == "EXECUTION_FAILURE") { out = E::EXECUTION_FAILURE; return true; }
  if (s == "RECOVERY_FAILURE") { out = E::RECOVERY_FAILURE; return true; }
  if (s == "UNKNOWN") { out = E::UNKNOWN; return true; }
)
FF_FS(OperationState,
  if (s == "CREATED") { out = E::CREATED; return true; }
  if (s == "DISPATCHED") { out = E::DISPATCHED; return true; }
  if (s == "RUNNING") { out = E::RUNNING; return true; }
  if (s == "COMPLETION_REPORTED") { out = E::COMPLETION_REPORTED; return true; }
  if (s == "COMPLETION_CONFIRMED") { out = E::COMPLETION_CONFIRMED; return true; }
  if (s == "FAILED_KNOWN") { out = E::FAILED_KNOWN; return true; }
  if (s == "FAILED_AMBIGUOUS") { out = E::FAILED_AMBIGUOUS; return true; }
  if (s == "PARTIALLY_SUCCEEDED") { out = E::PARTIALLY_SUCCEEDED; return true; }
  if (s == "RETRY_PENDING") { out = E::RETRY_PENDING; return true; }
  if (s == "RETRYING") { out = E::RETRYING; return true; }
  if (s == "ROLLBACK_PENDING") { out = E::ROLLBACK_PENDING; return true; }
  if (s == "ROLLING_BACK") { out = E::ROLLING_BACK; return true; }
  if (s == "RECOVERY_PENDING") { out = E::RECOVERY_PENDING; return true; }
  if (s == "RECOVERING") { out = E::RECOVERING; return true; }
  if (s == "RECOVERED") { out = E::RECOVERED; return true; }
  if (s == "TERMINAL_FAILED") { out = E::TERMINAL_FAILED; return true; }
  if (s == "CANCELLED") { out = E::CANCELLED; return true; }
  if (s == "ABANDONED") { out = E::ABANDONED; return true; }
)
FF_FS(RecoveryAction,
  if (s == "RETRY") { out = E::RETRY; return true; }
  if (s == "RETRY_ELSEWHERE") { out = E::RETRY_ELSEWHERE; return true; }
  if (s == "RESTART_WORKER") { out = E::RESTART_WORKER; return true; }
  if (s == "RESTART_DEVICE_CONTEXT") { out = E::RESTART_DEVICE_CONTEXT; return true; }
  if (s == "RELOAD_STATE") { out = E::RELOAD_STATE; return true; }
  if (s == "REHYDRATE_STATE") { out = E::REHYDRATE_STATE; return true; }
  if (s == "FAILOVER_REPLICA") { out = E::FAILOVER_REPLICA; return true; }
  if (s == "ROLLBACK") { out = E::ROLLBACK; return true; }
  if (s == "COMPENSATE") { out = E::COMPENSATE; return true; }
  if (s == "INVALIDATE") { out = E::INVALIDATE; return true; }
  if (s == "RECOMPUTE") { out = E::RECOMPUTE; return true; }
  if (s == "ABANDON") { out = E::ABANDON; return true; }
  if (s == "ESCALATE") { out = E::ESCALATE; return true; }
  if (s == "MANUAL_RESOLUTION") { out = E::MANUAL_RESOLUTION; return true; }
)
FF_FS(RecoveryOwner,
  if (s == "COORDINATOR") { out = E::COORDINATOR; return true; }
  if (s == "ORIGINAL_WORKER") { out = E::ORIGINAL_WORKER; return true; }
  if (s == "REPLACEMENT_WORKER") { out = E::REPLACEMENT_WORKER; return true; }
  if (s == "REPLICA_MANAGER") { out = E::REPLICA_MANAGER; return true; }
  if (s == "STORAGE_SUBSYSTEM") { out = E::STORAGE_SUBSYSTEM; return true; }
  if (s == "TRANSFER_SUBSYSTEM") { out = E::TRANSFER_SUBSYSTEM; return true; }
  if (s == "OPERATOR") { out = E::OPERATOR; return true; }
  if (s == "EXTERNAL_SYSTEM") { out = E::EXTERNAL_SYSTEM; return true; }
  if (s == "NONE") { out = E::NONE; return true; }
)
FF_FS(Retryability,
  if (s == "RETRYABLE") { out = E::RETRYABLE; return true; }
  if (s == "NON_RETRYABLE") { out = E::NON_RETRYABLE; return true; }
  if (s == "CONDITIONALLY_RETRYABLE") { out = E::CONDITIONALLY_RETRYABLE; return true; }
  if (s == "UNKNOWN") { out = E::UNKNOWN; return true; }
)
FF_FS(Ambiguity,
  if (s == "KNOWN") { out = E::KNOWN; return true; }
  if (s == "PARTIAL") { out = E::PARTIAL; return true; }
  if (s == "AMBIGUOUS") { out = E::AMBIGUOUS; return true; }
  if (s == "UNKNOWN_OUTCOME") { out = E::UNKNOWN_OUTCOME; return true; }
)
#undef FF_FS
}