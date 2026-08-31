# Contributing to Failure Fabric

Thank you for your interest in contributing to Failure Fabric. Contributions from individuals and organizations are welcome.

## License terms

Contributions are accepted on the terms of the [Apache License 2.0](LICENSE). Submitting a contribution to this repository means you agree to license your contribution under the terms of the Apache License 2.0.

**No Contributor License Agreement (CLA) is required.** We do not require a signed CLA; participation is open to individuals and organizations on the Apache License 2.0 terms.

## Code quality

- Language: C++20, Windows-first, MSVC.
- Build with CMake. The project builds with `/W4 /WX` for correctness; do not introduce warnings.
- Keep changes focused and covered by deterministic tests in `tests/`.
- Preserve the exact behavior of the failure semantics: one authoritative terminal outcome per operation, authority fencing, monotonic rollback/compensation progress, and strict stale-authority rejection.
- Run the full test suite (`ctest --test-dir build -C Release`) before opening a change.

## How to submit

1. Create a branch from `main`.
2. Make small, reviewable commits with clear messages.
3. Add or update tests for any semantic change.
4. Run the tests and the benchmarks.
5. Open a pull request describing the change and its motivation.

## Attribution

Commits are authored by their contributor. Please do not add co-author trailers or unattributed collaborators to commit messages; the commit author is the authoritative attribution.
