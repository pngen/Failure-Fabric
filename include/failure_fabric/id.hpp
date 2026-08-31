#pragma once
// 128-bit strongly-typed identities with deterministic serialization and exact
// round-trip. Copyright 2026 Summon Software Labs.
#include <cstdint>
#include <cstring>
#include <array>
#include <string>
#include <string_view>
#include <functional>
#include <random>

namespace ff {

struct uuid128 {
  std::array<uint8_t, 16> bytes{};

  constexpr bool operator==(const uuid128& o) const noexcept { for (int i = 0; i < 16; ++i) if (bytes[i] != o.bytes[i]) return false; return true; }
  constexpr bool operator!=(const uuid128& o) const noexcept { return !(*this == o); }
  constexpr bool operator<(const uuid128& o) const noexcept {
    for (int i = 0; i < 16; ++i) if (bytes[i] != o.bytes[i]) return bytes[i] < o.bytes[i];
    return false;
  }
  bool is_zero() const noexcept { for (auto b : bytes) if (b != 0) return false; return true; }

  std::string to_hex() const;
  std::string to_display() const;
  static uuid128 from_hex(std::string_view s);
  static uuid128 generate(std::mt19937_64& rng);
  static uuid128 generate();
};

inline bool is_valid_hex(std::string_view s) {
  if (s.empty() || s.size() > 32) return false;
  for (char c : s) {
    bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    if (!ok) return false;
  }
  return true;
}

// Strongly-typed identity: distinct Tag types make ids non-interchangeable.
template <typename Tag>
struct Id {
  uuid128 raw{};
  constexpr Id() noexcept = default;
  explicit constexpr Id(const uuid128& u) noexcept : raw(u) {}

  constexpr bool operator==(const Id& o) const noexcept { return raw == o.raw; }
  constexpr bool operator!=(const Id& o) const noexcept { return raw != o.raw; }
  constexpr bool operator<(const Id& o) const noexcept { return raw < o.raw; }

  bool is_null() const noexcept { return raw.is_zero(); }
  std::string to_hex() const { return raw.to_hex(); }
  std::string to_display() const { return raw.to_display(); }
  static Id from_hex(std::string_view s) { return Id(uuid128::from_hex(s)); }
  static Id generate(std::mt19937_64& rng) { return Id(uuid128::generate(rng)); }
  static Id generate() { return Id(uuid128::generate()); }
};

#define FF_ID_TAG(name) struct name##Tag {};
FF_ID_TAG(FailureId) FF_ID_TAG(RequestId) FF_ID_TAG(WorkloadId) FF_ID_TAG(AttemptId)
FF_ID_TAG(DispatchId) FF_ID_TAG(WorkerId) FF_ID_TAG(WorkerBootId) FF_ID_TAG(NodeId)
FF_ID_TAG(DeviceId) FF_ID_TAG(ReplicaId) FF_ID_TAG(ReservationId) FF_ID_TAG(AllocationId)
FF_ID_TAG(TransferId) FF_ID_TAG(RecoveryId) FF_ID_TAG(RecoveryPlanId) FF_ID_TAG(CompensationId)
FF_ID_TAG(OperationId) FF_ID_TAG(DependencyId)
#undef FF_ID_TAG

using FailureId      = Id<FailureIdTag>;
using RequestId       = Id<RequestIdTag>;
using WorkloadId      = Id<WorkloadIdTag>;
using AttemptId       = Id<AttemptIdTag>;
using DispatchId      = Id<DispatchIdTag>;
using WorkerId        = Id<WorkerIdTag>;
using WorkerBootId    = Id<WorkerBootIdTag>;
using NodeId          = Id<NodeIdTag>;
using DeviceId        = Id<DeviceIdTag>;
using ReplicaId       = Id<ReplicaIdTag>;
using ReservationId   = Id<ReservationIdTag>;
using AllocationId    = Id<AllocationIdTag>;
using TransferId      = Id<TransferIdTag>;
using RecoveryId      = Id<RecoveryIdTag>;
using RecoveryPlanId  = Id<RecoveryPlanIdTag>;
using CompensationId  = Id<CompensationIdTag>;
using OperationId     = Id<OperationIdTag>;
using DependencyId    = Id<DependencyIdTag>;

struct Generation {
  uint64_t value = 0;
  constexpr Generation() noexcept = default;
  explicit constexpr Generation(uint64_t v) noexcept : value(v) {}
  constexpr bool operator==(const Generation& o) const noexcept { return value == o.value; }
  constexpr bool operator<(const Generation& o) const noexcept { return value < o.value; }
  constexpr bool operator<=(const Generation& o) const noexcept { return value <= o.value; }
  constexpr bool is_initial() const noexcept { return value == 0; }
  constexpr bool is_unset() const noexcept { return value == 0; }
  Generation next() const noexcept { return Generation(value + 1); }
};

using CoordinatorEpoch    = Generation;
using AttemptGeneration   = Generation;
using OperationGeneration = Generation;
using FailureGeneration   = Generation;
using RecoveryGeneration  = Generation;

namespace detail {
inline const char* hex_digits() { return "0123456789abcdef"; }
inline int hex_nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
}

inline std::string uuid128_to_hex(const uuid128& u) {
  std::string s; s.reserve(32);
  const char* h = detail::hex_digits();
  for (uint8_t b : u.bytes) { s.push_back(h[b >> 4]); s.push_back(h[b & 0x0F]); }
  return s;
}
inline std::string uuid128_to_display(const uuid128& u) {
  std::string h = uuid128_to_hex(u);
  std::string o; o.reserve(36);
  o.append(h, 0, 8); o.push_back('-'); o.append(h, 8, 4); o.push_back('-');
  o.append(h, 12, 4); o.push_back('-'); o.append(h, 16, 4); o.push_back('-');
  o.append(h, 20, 12);
  return o;
}
inline uuid128 uuid128_from_hex(std::string_view s) {
  uuid128 u; if (!is_valid_hex(s)) return u;
  std::string full(32 - s.size(), '0'); full += std::string(s);
  for (int i = 0; i < 16; ++i) {
    int hi = detail::hex_nibble(full[2 * i]);
    int lo = detail::hex_nibble(full[2 * i + 1]);
    u.bytes[static_cast<size_t>(i)] = static_cast<uint8_t>((hi << 4) | lo);
  }
  return u;
}
inline uuid128 uuid128_generate(std::mt19937_64& rng) {
  uuid128 u;
  for (size_t i = 0; i < 16; ++i) u.bytes[i] = static_cast<uint8_t>(rng() & 0xFF);
  u.bytes[6] = static_cast<uint8_t>((u.bytes[6] & 0x0F) | 0x40);
  u.bytes[8] = static_cast<uint8_t>((u.bytes[8] & 0x3F) | 0x80);
  return u;
}

inline std::string uuid128::to_hex() const { return uuid128_to_hex(*this); }
inline std::string uuid128::to_display() const { return uuid128_to_display(*this); }
inline uuid128 uuid128::from_hex(std::string_view s) { return uuid128_from_hex(s); }
inline uuid128 uuid128::generate(std::mt19937_64& rng) { return uuid128_generate(rng); }
inline uuid128 uuid128::generate() {
  thread_local std::mt19937_64 rng{std::random_device{}()};
  return uuid128_generate(rng);
}

} // namespace ff

namespace std {
template <> struct hash<ff::uuid128> {
  size_t operator()(const ff::uuid128& u) const noexcept {
    size_t h = 1469598103934665603ULL;
    for (uint8_t b : u.bytes) { h ^= b; h *= 1099511628211ULL; }
    return h;
  }
};
template <typename Tag> struct hash<ff::Id<Tag>> {
  size_t operator()(const ff::Id<Tag>& id) const noexcept { return std::hash<ff::uuid128>()(id.raw); }
};
}