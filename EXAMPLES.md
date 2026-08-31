# Failure Fabric Examples

Runnable examples are in \`examples/\`. Build them (they are built by default) and run directly from \`build/Release\`.

## Build

\`\`\`sh
cmake --build build --config Release
\`\`\`

## Scenarios

- **Transient retry** (\`example_retry\`): a TRANSIENT failure is classified \`RETRY\` with a deterministic backoff, bounded by the policy.
- **Ambiguous completion** (\`example_ambiguous\`): a worker dies before ack; the same key+hash re-submission is deduplicated, a completed idempotent op returns prior result metadata, and a conflicting duplicate payload is rejected — no double apply.
- **Rollback vs compensation** (\`example_rollback_compensate\`): a typed rollback plan runs monotonically and resumably; a compensation restores logical correctness when physical undo is impossible.
- **Recovery ownership transfer** (\`example_ownership\`): ownership transfers strictly increase generation, a regressing transfer is rejected, and a completion from a stale worker boot is fenced.
- **Persistence / replay** (\`example_persist_replay\`): a store is saved and reloaded; the operation reproduces its authoritative terminal state with no change.

## CLI

Use \`ff_cli\` for the same scenarios interactively: \`classify\`, \`retry\`, \`recover\`, \`plan\`, \`rollback\`, \`compensate\`, \`owner\`, \`save\`/\`reload\`/\`replay\`, and \`explain\`. The \`cuda\` and \`multiprocess\` commands run the real proofs; \`serve\`/\`worker\` run the coordinator and worker processes.
