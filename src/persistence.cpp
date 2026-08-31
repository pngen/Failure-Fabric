#include "failure_fabric/persistence.hpp"
#include "failure_fabric/authority.hpp"
#include <cstring>
#include <algorithm>

namespace ff {

uint64_t fnv1a64(const uint8_t* data, size_t n) {
  uint64_t h = 1469598103934665603ULL;
  for (size_t i = 0; i < n; ++i) { h ^= data[i]; h *= 1099511628211ULL; }
  return h;
}

static void put_be64(std::vector<uint8_t>& v, uint64_t x) {
  for (int i = 7; i >= 0; --i) v.push_back(static_cast<uint8_t>((x >> (i * 8)) & 0xFF));
}
static void put_be32(std::vector<uint8_t>& v, uint32_t x) {
  for (int i = 3; i >= 0; --i) v.push_back(static_cast<uint8_t>((x >> (i * 8)) & 0xFF));
}
static void put_be16(std::vector<uint8_t>& v, uint16_t x) {
  for (int i = 1; i >= 0; --i) v.push_back(static_cast<uint8_t>((x >> (i * 8)) & 0xFF));
}

void BinaryWriter::u8(uint8_t v) { buf_.push_back(v); }
void BinaryWriter::u16(uint16_t v) { put_be16(buf_, v); }
void BinaryWriter::u32(uint32_t v) { put_be32(buf_, v); }
void BinaryWriter::u64(uint64_t v) { put_be64(buf_, v); }
void BinaryWriter::bytes(const uint8_t* p, size_t n) { buf_.insert(buf_.end(), p, p + n); }
void BinaryWriter::str16(const std::string& s) {
  if (s.size() > 0xFFFF) { u16(0); return; }
  u16(static_cast<uint16_t>(s.size()));
  bytes(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}
void BinaryWriter::uuid(const uuid128& u) { bytes(u.bytes.data(), 16); }
void BinaryWriter::generation(const Generation& g) { u64(g.value); }
void BinaryWriter::enumv(uint32_t v) { u32(v); }

static uint64_t get_be64(const uint8_t* p) {
  uint64_t x = 0;
  for (int i = 0; i < 8; ++i) x = (x << 8) | p[i];
  return x;
}
static uint32_t get_be32(const uint8_t* p) {
  uint32_t x = 0;
  for (int i = 0; i < 4; ++i) x = (x << 8) | p[i];
  return x;
}
static uint16_t get_be16(const uint8_t* p) {
  uint16_t x = 0;
  for (int i = 0; i < 2; ++i) x = static_cast<uint16_t>((x << 8) | p[i]);
  return x;
}

uint8_t BinaryReader::u8() { if (!need(1)) return 0; return p_[pos_++]; }
uint16_t BinaryReader::u16() { if (!need(2)) return 0; uint16_t v = get_be16(p_ + pos_); pos_ += 2; return v; }
uint32_t BinaryReader::u32() { if (!need(4)) return 0; uint32_t v = get_be32(p_ + pos_); pos_ += 4; return v; }
uint64_t BinaryReader::u64() { if (!need(8)) return 0; uint64_t v = get_be64(p_ + pos_); pos_ += 8; return v; }
void BinaryReader::bytes(uint8_t* out, size_t n) { if (!need(n)) return; memcpy(out, p_ + pos_, n); pos_ += n; }
std::string BinaryReader::str16() {
  uint16_t len = u16();
  if (!ok_ || pos_ + len > n_) { ok_ = false; return ""; }
  std::string s(reinterpret_cast<const char*>(p_ + pos_), len);
  pos_ += len;
  return s;
}
uuid128 BinaryReader::uuid() {
  uuid128 u;
  bytes(u.bytes.data(), 16);
  return u;
}
Generation BinaryReader::generation() { return Generation(u64()); }
uint32_t BinaryReader::enumv() { return u32(); }

std::vector<uint8_t> encode_frame(uint8_t kind, const uint8_t* payload, size_t n) {
  BinaryWriter w;
  w.u32(kFFMagic);
  w.u16(kFFVersion);
  w.u8(kind);
  w.u32(static_cast<uint32_t>(n));
  w.bytes(payload, n);
  const std::vector<uint8_t>& body = w.buffer();
  uint64_t ck = fnv1a64(body.data(), body.size());
  BinaryWriter out;
  out.bytes(body.data(), body.size());
  out.u64(ck);
  return out.buffer();
}

bool decode_frame(const uint8_t* data, size_t n, size_t offset, FrameHeader& out, size_t& next_offset) {
  if (offset + 11 > n) return false;
  BinaryReader r(data + offset, n - offset);
  uint32_t magic = r.u32();
  if (!r.ok() || magic != kFFMagic) return false;
  uint16_t ver = r.u16();
  if (!r.ok() || ver != kFFVersion) return false;
  out.kind = r.u8();
  uint32_t len = r.u32();
  if (!r.ok()) return false;
  if (static_cast<size_t>(len) > r.remaining()) return false; // truncation
  size_t payloadStart = offset + 11;
  if (payloadStart + static_cast<size_t>(len) + 8 > n) return false; // truncated checksum/tail
  out.payload.assign(data + payloadStart, data + payloadStart + len);
  uint64_t storedCk = get_be64(data + payloadStart + len);
  // checksum covers magic..payload (i.e. the full body before checksum)
  uint64_t ck = fnv1a64(data + offset, 11 + len);
  if (ck != storedCk) return false; // corruption
  out.checksum = storedCk;
  next_offset = payloadStart + len + 8;
  return true;
}

void encode_failure_record(BinaryWriter& w, const FailureRecord& r) {
  w.uuid(r.failure_id.raw); w.uuid(r.operation.raw); w.uuid(r.request.raw); w.uuid(r.workload.raw);
  w.uuid(r.attempt.raw); w.uuid(r.dispatch.raw); w.uuid(r.worker.raw); w.uuid(r.node.raw); w.uuid(r.device.raw);
  w.enumv(static_cast<uint32_t>(r.failure_class)); w.enumv(static_cast<uint32_t>(r.origin));
  w.u32(r.failure_code); w.str16(r.summary); w.str16(r.reason); w.enumv(static_cast<uint32_t>(r.phase));
  w.u64(r.timestamp_ms); w.enumv(static_cast<uint32_t>(r.completion)); w.enumv(static_cast<uint32_t>(r.side_effect_state));
  w.u32(r.unknown_side_effects);
  w.generation(r.authority.coordinator_epoch); w.uuid(r.authority.worker_boot.raw);
  w.uuid(r.authority.attempt.raw); w.uuid(r.authority.dispatch.raw);
  w.generation(r.authority.attempt_generation); w.generation(r.authority.operation_generation);
  w.generation(r.authority.failure_generation); w.generation(r.authority.recovery_generation);
  w.enumv(static_cast<uint32_t>(r.retryability)); w.enumv(static_cast<uint32_t>(r.rollback));
  w.enumv(static_cast<uint32_t>(r.compensation)); w.enumv(static_cast<uint32_t>(r.terminality));
  w.enumv(static_cast<uint32_t>(r.recovery_owner)); w.enumv(static_cast<uint32_t>(r.provenance));
  w.uuid(r.causal_parent.raw);
  w.u32(static_cast<uint32_t>(r.dependencies.size()));
  for (const auto& d : r.dependencies) w.uuid(d.raw);
}


bool decode_failure_record(BinaryReader& r, FailureRecord& rec) {
  if (!r.ok()) return false;
  rec.failure_id = FailureId(r.uuid()); rec.operation = OperationId(r.uuid());
  rec.request = RequestId(r.uuid()); rec.workload = WorkloadId(r.uuid());
  rec.attempt = AttemptId(r.uuid()); rec.dispatch = DispatchId(r.uuid());
  rec.worker = WorkerId(r.uuid()); rec.node = NodeId(r.uuid()); rec.device = DeviceId(r.uuid());
  uint32_t fc = r.enumv(), fo = r.enumv();
  if (!r.ok()) return false;
  if (fc >= 23 || fo >= 11) return false; // invalid enum -> reject
  rec.failure_class = static_cast<FailureClass>(fc);
  rec.origin = static_cast<FailureOrigin>(fo);
  rec.failure_code = r.u32();
  rec.summary = r.str16(); rec.reason = r.str16();
  uint32_t ph = r.enumv(); if (!r.ok() || ph >= 10) return false;
  rec.phase = static_cast<FailurePhase>(ph);
  rec.timestamp_ms = r.u64();
  uint32_t comp = r.enumv(); if (!r.ok() || comp >= 4) return false;
  rec.completion = static_cast<Ambiguity>(comp);
  uint32_t ses = r.enumv(); if (!r.ok() || ses >= 5) return false;
  rec.side_effect_state = static_cast<SideEffectState>(ses);
  rec.unknown_side_effects = r.u32();
  rec.authority.coordinator_epoch = CoordinatorEpoch(r.generation());
  rec.authority.worker_boot = WorkerBootId(r.uuid());
  rec.authority.attempt = AttemptId(r.uuid());
  rec.authority.dispatch = DispatchId(r.uuid());
  rec.authority.attempt_generation = AttemptGeneration(r.generation());
  rec.authority.operation_generation = OperationGeneration(r.generation());
  rec.authority.failure_generation = FailureGeneration(r.generation());
  rec.authority.recovery_generation = RecoveryGeneration(r.generation());
  uint32_t ret = r.enumv(); if (!r.ok() || ret >= 4) return false;
  rec.retryability = static_cast<Retryability>(ret);
  uint32_t rb = r.enumv(); if (!r.ok() || rb >= 4) return false;
  rec.rollback = static_cast<RollbackRequirement>(rb);
  uint32_t cp = r.enumv(); if (!r.ok() || cp >= 3) return false;
  rec.compensation = static_cast<CompensationRequirement>(cp);
  uint32_t tm = r.enumv(); if (!r.ok() || tm >= 4) return false;
  rec.terminality = static_cast<Terminality>(tm);
  uint32_t ro = r.enumv(); if (!r.ok() || ro >= 9) return false;
  rec.recovery_owner = static_cast<RecoveryOwner>(ro);
  uint32_t pr = r.enumv(); if (!r.ok() || pr >= 7) return false;
  rec.provenance = static_cast<Provenance>(pr);
  rec.causal_parent = FailureId(r.uuid());
  uint32_t ndeps = r.u32();
  if (!r.ok()) return false;
  if (ndeps > 4096) return false; // malformed length guard
  rec.dependencies.clear();
  for (uint32_t i = 0; i < ndeps; ++i) rec.dependencies.push_back(FailureId(r.uuid()));
  if (!r.ok()) return false;
  return validate_authority(rec.authority);
}

bool validate_authority(const AuthorityEnvelope& e) {
  // Impossible authority combinations: a generation zero alongside a set boot is
  // suspicious; an attempt_generation of zero with a real attempt is malformed.
  if (!e.attempt.is_null() && e.attempt_generation.is_unset()) return false;
  if (!e.worker_boot.is_null() && e.coordinator_epoch.is_unset()) return false;
  return true;
}
} // namespace ff