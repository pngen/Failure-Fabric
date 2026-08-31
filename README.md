# Failure Fabric 1.0.0

Failure Fabric is a C++20 runtime that owns the semantics of failure across distributed AI runtime operations. It is not a logger, exception wrapper, retry helper, or generic fault-injection library: it is the failure contract that scheduling, admission, quota, replica lifecycle, and observability systems consume.

## Systems boundary

Failure Fabric owns what failed, where it failed, when it failed, which attempt/generation/epoch it belonged to, whether completion is known/unknown/partial/ambiguous, whether side effects may have occurred, whether retry is safe, whether rollback or compensation is required, whether state must be fenced, who owns recovery, which recovery action is currently authorized, and whether a failure is terminal, transient, or stale. It does **not** own scheduling, admission, quota, replica lifecycle, or general observability.

## Core question

When distributed AI work fails, the system must classify what happened, determine what is still authoritative, decide what may safely retry or roll back, assign recovery ownership, and prevent ambiguous or stale outcomes from becoming real. Failure Fabric answers that question deterministically.

## Failure model

Failure classification is deliberately split across orthogonal dimensions: failure class, origin, severity, retryability, ambiguity, side-effect state, authority status, rollback requirement, compensation requirement, terminality, recovery owner, and provenance. No single enum collapses these. Classes include TRANSIENT, PERMANENT, RETRYABLE, NON_RETRYABLE, AMBIGUOUS, PARTIAL_SUCCESS, STALE_AUTHORITY, LOST_WORKER, LOST_DEVICE, TRANSPORT_FAILURE, TIMEOUT_OBSERVED, PROTOCOL_FAILURE, RESOURCE_EXHAUSTION, CORRUPTION, VALIDATION_FAILURE, INCOMPATIBILITY, CANCELLED, PREEMPTED, DEPENDENCY_FAILURE, STORAGE_FAILURE, EXECUTION_FAILURE, RECOVERY_FAILURE, and UNKNOWN.

A committed failure is an immutable canonical record (FailureRecord) containing all identity, classification, side-effect, authority, requirement, ownership, provenance, and generation state.

## Authority

Every failure and recovery action is fenced by a multi-dimensional authority envelope: coordinator epoch, WorkerBootId, AttemptId, AttemptGeneration, DispatchId, OperationGeneration, FailureGeneration, and RecoveryGeneration. Old failures cannot invalidate fresh work. Old completions cannot resurrect dead authority. A restarted worker does not inherit prior authority by logical name alone — a fresh WorkerBootId is required — and a stale recovery command cannot mutate current state.

## Ambiguous completion

Ambiguity is a first-class problem: a worker dies after side effects but before ack, a network fails after a commit, completion is emitted but the ack is lost, a storage write may have committed before a disconnect, or a transfer may have reached remote memory before the sender lost contact. Operation semantics (idempotent, deduplicated, transactional, compensatable, non-repeatable, externally committed, unknown side effects) drive whether retry is safe, requires the same idempotency key, requires a new attempt, must roll back first, requires compensation, or requires manual resolution.

## Idempotency

IdempotencyKey/operation-identity semantics prove that duplicate same-operation submission does not double-apply, retry under the same key is safe where policy permits, conflicting duplicate payloads under the same key are rejected, stale generations under a valid key are rejected, and completed idempotent operations return prior result metadata without re-executing. The runtime is explicit about at-most-once where enforceable, idempotent replay, deduplicated execution, transactional commit, and ambiguous outcome — it never fakes exactly-once.

## Rollback vs compensation

Typed rollback plans (release reservation, free allocation, invalidate generation, revert metadata, remove partial state, restore snapshot, cancel dependent work, release serving authority, mark artifact invalid, unwind transaction) are ordered, authority-fenced, idempotent where possible, individually reportable, resumable after crash, and deterministic. If rollback itself fails, a RecoveryFailure is recorded rather than hidden. Compensation is separate: it restores logical correctness when a side effect cannot be undone physically (an external effect escaped, an accounting adjustment, or a replacement action), carrying its own id, target, reason, preconditions, authority, side effects, and completion/failure state.

## Recovery ownership

Only one current authoritative recovery owner may mutate recovery state. Owners include coordinator, original worker, replacement worker, replica manager, storage subsystem, transfer subsystem, operator, and external system. Ownership transfers strictly increase the recovery generation; a dead owner retains no authority after restart.

## Recovery plans

Deterministic RecoveryPlan generation from failure state selects among RETRY, RETRY_ELSEWHERE, RESTART_WORKER, RESTART_DEVICE_CONTEXT, RELOAD_STATE, REHYDRATE_STATE, FAILOVER_REPLICA, ROLLBACK, COMPENSATE, INVALIDATE, RECOMPUTE, ABANDON, ESCALATE, and MANUAL_RESOLUTION. Plans expose preconditions, the selected action, rejected alternatives and their reasons, reason factors, authority/generation, and expected preserved/discarded state — with no opaque score.

## Persistence / replay

Strict versioned persistence uses deterministic binary encoding with checksum protection. It persists immutable failure records, operation states, recovery ownership, idempotency records, generations, and terminal outcomes, and rejects malformed lengths, truncation, corruption, duplicate/conflicting IDs, invalid enums/generations/transitions, impossible authority combinations, unsupported versions, and trailing garbage. Recovery never resurrects stale authority.

## Multiprocess proof

A real framed-TCP coordinator plus two worker OS processes reproduce worker death around a real side effect, ambiguous completion classification, fencing of a stale replayed completion, recovery ownership transfer under a fresh generation, idempotent/rollback retry, one authoritative terminal outcome, no duplicate side effects, persistence, and deterministic replay after a fresh-process reload. No in-process mocks.

## CUDA proof

A real RTX 5090 / sm_120 proof runs cudaMalloc, host-to-device copies, real kernels, controlled verification failure after execution (known failure → classification → fresh-attempt retry), worker-death-around-device-work (stale boot fencing, state re-establishment, CPU parity, exact allocation-baseline restoration), and ambiguous completion resolved by idempotency without a double logical commit.

## Benchmarks

The benchmark suite measures failure-record append, classification, recovery-plan generation, idempotency lookup/duplicate rejection, state reconstruction, persistence save/recovery, deterministic replay, concurrent failure ingestion, and explanation generation on meaningful dataset sizes, reporting actual measured values.

## Build / install / use

```sh
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

Install and consume:

```sh
cmake --install build --config Release --prefix <prefix>
# consumer: find_package(FailureFabric REQUIRED); target_link_libraries(app PRIVATE ff::FailureFabric)
```

The `ff_cli` tool exposes `list`, `inspect`, `failure`, `operation`, `attempt`, `classify`, `retry`, `rollback`, `compensate`, `recover`, `owner`, `plan`, `explain`, `snapshot`, `save`, `reload`, `replay`, `serve`, `worker`, `multiprocess`, `cuda`, and `benchmark`. See EXAMPLES.md for runnable scenarios and ARCHITECTURE.md for design details.

## License

Apache License 2.0. Copyright 2026 Summon Software Labs. No telemetry transmission.
