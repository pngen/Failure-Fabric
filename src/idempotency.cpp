#include "failure_fabric/idempotency.hpp"
#include <unordered_map>

namespace ff {
const char* to_string(IdempotencyVerdict v) noexcept {
  switch (v) {
    case IdempotencyVerdict::NEW: return "NEW";
    case IdempotencyVerdict::IN_PROGRESS_REPLAY: return "IN_PROGRESS_REPLAY";
    case IdempotencyVerdict::COMPLETED_REPLAY: return "COMPLETED_REPLAY";
    case IdempotencyVerdict::CONFLICT: return "CONFLICT";
    case IdempotencyVerdict::STALE: return "STALE";
  }
  return "UNKNOWN";
}
IdempotencyVerdict IdempotencyStore::begin(const OperationId& op, const IdempotencyKey& key,
                                           uint64_t hash, const AuthorityEnvelope& auth) {
  PairKey pk{op, key};
  auto it = entries_.find(pk);
  if (it == entries_.end()) {
    Entry e; e.payload_hash = hash; e.state = IdempotencyState::IN_PROGRESS; e.authority = auth;
    entries_.emplace(pk, std::move(e));
    return IdempotencyVerdict::NEW;
  }
  Entry& e = it->second;
  if (e.payload_hash != hash) return IdempotencyVerdict::CONFLICT; // conflicting duplicate payload
  if (e.state == IdempotencyState::CONFLICT) return IdempotencyVerdict::CONFLICT;
  if (e.state == IdempotencyState::COMPLETED) return IdempotencyVerdict::COMPLETED_REPLAY;
  if (e.state == IdempotencyState::FAILED) return IdempotencyVerdict::NEW; // failed -> may retry fresh
  return IdempotencyVerdict::IN_PROGRESS_REPLAY;
}

void IdempotencyStore::complete(const OperationId& op, const IdempotencyKey& key, std::string meta) {
  PairKey pk{op, key};
  auto& e = entries_[pk];
  e.state = IdempotencyState::COMPLETED;
  e.result_metadata = std::move(meta);
}
void IdempotencyStore::fail(const OperationId& op, const IdempotencyKey& key) {
  PairKey pk{op, key};
  auto it = entries_.find(pk);
  if (it != entries_.end()) it->second.state = IdempotencyState::FAILED;
}
void IdempotencyStore::mark_conflict(const OperationId& op, const IdempotencyKey& key) {
  PairKey pk{op, key};
  auto& e = entries_[pk];
  e.state = IdempotencyState::CONFLICT;
}
bool IdempotencyStore::prior_result(const OperationId& op, const IdempotencyKey& key, std::string& out) const {
  PairKey pk{op, key};
  auto it = entries_.find(pk);
  if (it != entries_.end() && it->second.state == IdempotencyState::COMPLETED) {
    out = it->second.result_metadata; return true;
  }
  return false;
}
bool IdempotencyStore::has(const OperationId& op, const IdempotencyKey& key) const {
  return entries_.find(PairKey{op, key}) != entries_.end();
}
IdempotencyState IdempotencyStore::state(const OperationId& op, const IdempotencyKey& key) const {
  auto it = entries_.find(PairKey{op, key});
  return it == entries_.end() ? IdempotencyState::IN_PROGRESS : it->second.state;
}
uint64_t IdempotencyStore::payload_hash(const OperationId& op, const IdempotencyKey& key) const {
  auto it = entries_.find(PairKey{op, key});
  return it == entries_.end() ? 0 : it->second.payload_hash;
}

void IdempotencyStore::restore(const OperationId& op, const IdempotencyKey& key, uint64_t hash, IdempotencyState st, std::string meta) {
  Entry e; e.payload_hash = hash; e.state = st; e.result_metadata = std::move(meta);
  entries_[PairKey{op, key}] = std::move(e);
}
std::vector<IdempotencyStore::EntryView> IdempotencyStore::collect() const {
  std::vector<EntryView> out; out.reserve(entries_.size());
  for (auto& p : entries_) out.push_back(EntryView{p.first.op, p.first.key, p.second.payload_hash, p.second.state, p.second.result_metadata});
  return out;
}
} // namespace ff