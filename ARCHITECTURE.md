# Failure Fabric Architecture

Failure Fabric 1.0.0 is a C++20 runtime. It is organized as a compiled library (\`FailureFabric\`), a CLI (\`ff_cli\`), coordinator and worker executables (\`ff_coordinator\`, \`ff_worker\`), a CUDA proof, a benchmark suite, and runnable examples.

## Layers

- **Identity layer** — 128-bit strongly-typed identities (\`FailureId\`, \`AttemptId\`, \`WorkerId\`, \`WorkerBootId\`, \`RecoveryId\`, \`CompensationId\`, ...) and monotonic generations. Identifiers serialize deterministically and round-trip exactly.
- **Semantics layer** — orthogonal failure dimensions, the immutable \`FailureRecord\`, the guarded \`OperationStateMachine\`, and the \`AuthorityEnvelope\` fencing rules. Staleness is evaluated per dimension by \`fence_completion\`, \`fence_failure\`, and \`fence_recovery\`.
- **Recovery layer** — \`IdempotencyStore\`, \`RollbackExecutor\`, \`Compensator\`, \`RecoveryOwnership\`, \`RecoveryPlanner\`, and \`RetryClassifier\`. All decisions are deterministic rule-based; no opaque score.
- **Persistence layer** — deterministic binary codec (\`BinaryWriter\`/\`BinaryReader\`), checksum-protected frames, and a versioned snapshot (\`FailureStore::save\`/\`load\`). Malformed, truncated, corrupt, unsupported, and trailing-garbage inputs are rejected atomically.
- **Store layer** — \`FailureStore\` ties the immutable failure log, \`OperationRegistry\`, \`IdempotencyStore\`, and \`RecoveryOwnership\` under one mutex, enforcing one authoritative terminal outcome per operation.
- **Protocol layer** — framed TCP over Winsock (\`TcpSocket\`/\`TcpListener\`/\`TcpSocket::send\`/\`recv\`) with checksummed frames, safe socket ownership, and per-connection write serialization. Network I/O never happens while holding the global state mutex.
- **Runtime executables** — \`ff_coordinator\` (accepts workers, dispatches, fences, classifies worker loss as ambiguity, transfers ownership, confirms exactly one terminal, persists) and \`ff_worker\` (registers, performs a real side effect, reports). \`test_multiprocess\` drives them as real OS processes and \`TerminateProcess\`-kills a worker under live authority.

## Concurrency

The store's auxiliary state is guarded by one mutex. Worker threads perform socket recv/send outside the lock and only take the lock to mutate registry/store state. This guarantees no deadlock, no iterator invalidation on the concurrent maps, and no recovery split-brain (strictly increasing recovery generation).

## Explanation

\`explain_failure\`/\`explain_decision\` produce deterministic text and JSON: why a failure was classified this way, whether completion is known or ambiguous, whether retry/rollback/compensation is required, who owns recovery, which authority permits the action, rejected alternatives, and expected preserved/discarded state.
