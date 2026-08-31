#pragma once
// Explicit idempotency / deduplication semantics. Copyright 2026 Summon Software Labs.
#include "failure_fabric/id.hpp"
#include "failure_fabric/authority.hpp"
#include <string>
#include <cstdint>
#include <unordered_map>
#include <functional>

namespace ff {

struct IdempotencyKey {
  uuid128 raw{};
  bool operator==(const IdempotencyKey& o) const { return raw == o.raw; }
  bool operator!=(const IdempotencyKey& o) const { return raw != o.raw; }
  std::string to_hex() const { return raw.to_hex(); }
  static IdempotencyKey generate(std::mt19937_64& rng) { return IdempotencyKey{uuid128::generate(rng)}; }
  static IdempotencyKey from_hex(std::string_view s) { return IdempotencyKey{uuid128::from_hex(s)}; }
};

enum class IdempotencyVerdict : uint32_t {
  NEW, IN_PROGRESS_REPLAY, COMPLETED_REPLAY, CONFLICT, STALE
};

enum class IdempotencyState : uint32_t { IN_PROGRESS, COMPLETED, FAILED, CONFLICT };

class IdempotencyStore {
public:
  IdempotencyStore() = default;
  IdempotencyVerdict begin(const OperationId& op, const IdempotencyKey& key,
                           uint64_t payload_hash, const AuthorityEnvelope& authority);
  void complete(const OperationId& op, const IdempotencyKey& key, std::string result_metadata);
  void fail(const OperationId& op, const IdempotencyKey& key);
  void mark_conflict(const OperationId& op, const IdempotencyKey& key);
  bool prior_result(const OperationId& op, const IdempotencyKey& key, std::string& out) const;
  bool has(const OperationId& op, const IdempotencyKey& key) const;
  IdempotencyState state(const OperationId& op, const IdempotencyKey& key) const;
  uint64_t payload_hash(const OperationId& op, const IdempotencyKey& key) const;
  // Persistence support: restore an entry directly and enumerate entries.
  void restore(const OperationId& op, const IdempotencyKey& key, uint64_t hash, IdempotencyState st, std::string meta);
  struct EntryView { OperationId op; IdempotencyKey key; uint64_t payload_hash; IdempotencyState state; std::string result_metadata; };
  std::vector<EntryView> collect() const;

  size_t size() const { return entries_.size(); }
  void clear() { entries_.clear(); }
private:
  struct Entry {
    uint64_t payload_hash = 0;
    IdempotencyState state = IdempotencyState::IN_PROGRESS;
    std::string result_metadata;
    AuthorityEnvelope authority{};
  };
  struct PairKey {
    OperationId op;
    IdempotencyKey key;
    bool operator==(const PairKey& o) const { return op == o.op && key == o.key; }
  };
  struct PairHash {
    size_t operator()(const PairKey& k) const noexcept {
      size_t h = std::hash<uuid128>()(k.op.raw);
      return h * 31 ^ std::hash<uuid128>()(k.key.raw);
    }
  };
  std::unordered_map<PairKey, Entry, PairHash> entries_;
};

} // namespace ff