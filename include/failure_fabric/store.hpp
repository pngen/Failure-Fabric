#pragma once
// Thread-safe failure store: immutable failure log + operation registry +
// idempotency + recovery ownership, with strict versioned persistence.
// Copyright 2026 Summon Software Labs.
#include "failure_fabric/failure_record.hpp"
#include "failure_fabric/operation_registry.hpp"
#include "failure_fabric/idempotency.hpp"
#include "failure_fabric/recovery.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace ff {

class FailureStore {
public:
  // Immutable failure log: rejects duplicate failure ids.
  bool append_failure(const FailureRecord& rec);
  bool has_failure(const FailureId& id) const;
  const FailureRecord* find_failure(const FailureId& id) const;
  size_t failure_count() const { return failures_.size(); }
  std::vector<FailureRecord> failures() const;

  OperationRegistry&  ops() { return ops_; }
  const OperationRegistry& ops() const { return ops_; }
  IdempotencyStore&   idem() { return idem_; }
  const IdempotencyStore& idem() const { return idem_; }
  RecoveryOwnership&  ownership() { return ownership_; }
  const RecoveryOwnership& ownership() const { return ownership_; }

  std::mutex& mutex() { return mu_; }

  // Persist the full store as a versioned, checksummed snapshot. Save is
  // deterministic; load rejects malformed/truncated/corrupt/unsupported data
  // and returns false, leaving the store untouched.
  bool save(const std::string& path) const;
  bool load(const std::string& path);
  void clear();

private:
  mutable std::mutex mu_;
  std::unordered_map<FailureId, FailureRecord> failures_;
  OperationRegistry ops_;
  IdempotencyStore idem_;
  RecoveryOwnership ownership_;
};

} // namespace ff
