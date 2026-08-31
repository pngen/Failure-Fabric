#include "failure_fabric/store.hpp"
#include "failure_fabric/persistence.hpp"
#include <fstream>
#include <unordered_set>
#include <cstring>

namespace ff {
namespace {
constexpr uint8_t K_FAIL = 1;
constexpr uint8_t K_OP   = 2;
constexpr uint8_t K_ID   = 3;
constexpr uint8_t K_OWN  = 4;

void enc_auth(BinaryWriter& w, const AuthorityEnvelope& e) {
  w.generation(e.coordinator_epoch); w.uuid(e.worker_boot.raw); w.uuid(e.attempt.raw);
  w.uuid(e.dispatch.raw); w.generation(e.attempt_generation); w.generation(e.operation_generation);
  w.generation(e.failure_generation); w.generation(e.recovery_generation);
}
bool dec_auth(BinaryReader& r, AuthorityEnvelope& e) {
  e.coordinator_epoch = CoordinatorEpoch(r.generation()); e.worker_boot = WorkerBootId(r.uuid());
  e.attempt = AttemptId(r.uuid()); e.dispatch = DispatchId(r.uuid());
  e.attempt_generation = AttemptGeneration(r.generation()); e.operation_generation = OperationGeneration(r.generation());
  e.failure_generation = FailureGeneration(r.generation()); e.recovery_generation = RecoveryGeneration(r.generation());
  return r.ok() && validate_authority(e);
}
}

bool FailureStore::append_failure(const FailureRecord& rec) {
  std::lock_guard<std::mutex> lk(mu_);
  if (rec.failure_id.is_null()) return false;
  if (failures_.count(rec.failure_id)) return false;
  if (rec.operation.is_null() || rec.attempt.is_null()) return false;
  if (!validate_authority(rec.authority)) return false;
  failures_.emplace(rec.failure_id, rec);
  return true;
}
bool FailureStore::has_failure(const FailureId& id) const {
  std::lock_guard<std::mutex> lk(mu_); return failures_.count(id) != 0;
}
const FailureRecord* FailureStore::find_failure(const FailureId& id) const {
  std::lock_guard<std::mutex> lk(mu_);
  auto it = failures_.find(id);
  return it == failures_.end() ? nullptr : &it->second;
}
std::vector<FailureRecord> FailureStore::failures() const {
  std::lock_guard<std::mutex> lk(mu_);
  std::vector<FailureRecord> v; v.reserve(failures_.size());
  for (auto& p : failures_) v.push_back(p.second);
  return v;
}

static void write_frame(std::ofstream& f, uint8_t kind, const std::vector<uint8_t>& payload) {
  auto fr = encode_frame(kind, payload.data(), payload.size());
  f.write(reinterpret_cast<const char*>(fr.data()), static_cast<std::streamsize>(fr.size()));
}

bool FailureStore::save(const std::string& path) const {
  std::lock_guard<std::mutex> lk(mu_);
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) return false;
  BinaryWriter hw;
  hw.u32(kFFMagic); hw.u16(kFFVersion);
  hw.u32(static_cast<uint32_t>(failures_.size()));
  hw.u32(static_cast<uint32_t>(ops_.size()));
  hw.u32(static_cast<uint32_t>(idem_.size()));
  write_frame(f, 0, hw.buffer());

  for (const auto& kv : failures_) {
    BinaryWriter w;
    encode_failure_record(w, kv.second);
    write_frame(f, K_FAIL, w.buffer());
  }
  for (const OperationId& op : ops_.operations()) {
    const OperationRuntime* rt = ops_.find(op);
    if (!rt) continue;
    BinaryWriter w;
    w.uuid(op.raw);
    w.enumv(static_cast<uint32_t>(rt->machine.state()));
    w.u32(rt->attempt_count);
    w.u8(rt->terminal_recorded ? 1 : 0);
    w.str16(rt->terminal_outcome);
    w.u8(rt->idempotent ? 1 : 0);
    w.uuid(rt->idem_key.raw);
    enc_auth(w, rt->authority);
    enc_auth(w, rt->terminal);
    write_frame(f, K_OP, w.buffer());
  }
  for (const auto& e : idem_.collect()) {
    BinaryWriter w;
    w.uuid(e.op.raw); w.uuid(e.key.raw);
    w.u64(e.payload_hash);
    w.enumv(static_cast<uint32_t>(e.state));
    w.str16(e.result_metadata);
    write_frame(f, K_ID, w.buffer());
  }
  BinaryWriter ow;
  ow.enumv(static_cast<uint32_t>(ownership_.owner()));
  ow.generation(ownership_.generation());
  ow.uuid(ownership_.holder_boot().raw);
  write_frame(f, K_OWN, ow.buffer());
  return f.good();
}

bool FailureStore::load(const std::string& path) {
  std::lock_guard<std::mutex> lk(mu_);
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (data.empty()) return false;

  std::vector<FailureRecord> frs;
  std::vector<OperationRuntime> orts;
  std::vector<OperationId> opids;
  std::vector<IdempotencyStore::EntryView> idents;
  uint32_t nfail = 0, nop = 0, nid = 0;
  bool first = true;
  size_t off = 0;
  while (off < data.size()) {
    FrameHeader h; size_t next = 0;
    if (!decode_frame(data.data(), data.size(), off, h, next)) return false;
    BinaryReader r(h.payload.data(), h.payload.size());
    if (first) {
      if (r.u32() != kFFMagic || r.u16() != kFFVersion) return false;
      nfail = r.u32(); nop = r.u32(); nid = r.u32();
      if (!r.ok()) return false;
      first = false;
    } else if (h.kind == K_FAIL) {
      FailureRecord rec;
      if (!decode_failure_record(r, rec)) return false;
      frs.push_back(std::move(rec));
    } else if (h.kind == K_OP) {
      if (!r.ok()) return false;
      OperationRuntime rt;
      OperationId op(r.uuid());
      rt.machine.reset(static_cast<OperationState>(r.enumv()));
      rt.attempt_count = r.u32();
      rt.terminal_recorded = r.u8() != 0;
      rt.terminal_outcome = r.str16();
      rt.idempotent = r.u8() != 0;
      rt.idem_key = IdempotencyKey{r.uuid()};
      if (!dec_auth(r, rt.authority)) return false;
      if (!dec_auth(r, rt.terminal)) return false;
      if (!r.ok()) return false;
      opids.push_back(op); orts.push_back(std::move(rt));
    } else if (h.kind == K_ID) {
      IdempotencyStore::EntryView ev;
      ev.op = OperationId(r.uuid());
      ev.key = IdempotencyKey{r.uuid()};
      ev.payload_hash = r.u64();
      ev.state = static_cast<IdempotencyState>(r.enumv());
      ev.result_metadata = r.str16();
      if (!r.ok()) return false;
      idents.push_back(std::move(ev));
    } else if (h.kind == K_OWN) {
      uint32_t owner = r.enumv(); if (!r.ok() || owner >= 9) return false;
      RecoveryGeneration g(r.generation()); WorkerBootId boot(r.uuid());
      if (!r.ok()) return false;
      ownership_.adopt(static_cast<RecoveryOwner>(owner), g, boot, AuthorityEnvelope{});
    } else {
      return false;
    }
    off = next;
  }
  // strict validation
  if (frs.size() != nfail || orts.size() != nop || idents.size() != nid) return false;
  std::unordered_map<FailureId, FailureRecord> newfails;
  for (auto& rec : frs) if (!newfails.emplace(rec.failure_id, rec).second) return false;
  // duplicate op guards
  {
    std::unordered_set<OperationId> seen;
    for (auto& op : opids) if (!seen.insert(op).second) return false;
  }
  // commit (only after all validation succeeds)
  failures_.clear(); ops_.clear(); idem_.clear();
  ownership_ = RecoveryOwnership{};
  for (auto& kv : newfails) failures_.emplace(kv.first, std::move(kv.second));
  for (size_t i = 0; i < opids.size(); ++i) ops_.restore(opids[i], std::move(orts[i]));
  for (auto& ev : idents) idem_.restore(ev.op, ev.key, ev.payload_hash, ev.state, ev.result_metadata);
  return true;
}

void FailureStore::clear() {
  std::lock_guard<std::mutex> lk(mu_);
  failures_.clear(); ops_.clear(); idem_.clear(); ownership_ = RecoveryOwnership{};
}
} // namespace ff