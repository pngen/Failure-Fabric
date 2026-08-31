# Failure Fabric Benchmarks

Run with \`ff_benchmark [n]\` (default \`n=200000\`). Results are measured on the build machine and are reported in actual units.

| Metric | measured (n=200000) |
| --- | --- |
| failure-record append | 2.45 M ops/s |
| classification | 3.97 M ops/s |
| recovery-plan generation | 1.60 M ops/s |
| idempotency lookup (dup rejection) | 43.7 M ops/s |
| state reconstruction | 2.14 M ops/s |
| persistence save | 296.5 ms (134.9 MB/s) |
| persistence load | 489.7 ms |
| deterministic replay | 429 k ops/s |
| concurrent failure ingestion | 764 k ops/s (8 threads, 1.15M records) |
| explanation generation | 451 k ops/s |

These numbers are representative of a single run on the validation machine (RTX 5090 host, MSVC Release). Re-run on your target hardware for your own figures.
