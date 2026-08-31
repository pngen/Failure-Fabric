# Failure Fabric Benchmarks

Run with \`ff_benchmark [n]\` (default \`n=200000\`). Results are measured on the build machine (RTX 5090 host, MSVC Release) and are reported in actual units. Values vary slightly run to run under host load; each is a single measured run.

| Metric | measured (n=200000) |
| --- | --- |
| failure-record append | 1.79 M ops/s |
| classification | 2.68 M ops/s |
| recovery-plan generation | 1.23 M ops/s |
| idempotency lookup (dup rejection) | 43.4 M ops/s |
| state reconstruction | 1.09 M ops/s |
| persistence save | 289.1 ms (138.4 MB/s) |
| persistence load | 455.7 ms |
| deterministic replay | 458 k ops/s |
| concurrent failure ingestion | 869 k ops/s (8 threads, 1.31M records) |
| explanation generation | 438 k ops/s |

Re-run on your target hardware for your own figures.
