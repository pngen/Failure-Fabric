#pragma once
// Strict versioned persistence: deterministic binary codec + checksum protection.
// Copyright 2026 Summon Software Labs.
#include "failure_fabric/id.hpp"
#include "failure_fabric/enum.hpp"
#include "failure_fabric/failure_record.hpp"
#include "failure_fabric/operation.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace ff {

constexpr uint32_t kFFMagic = 0x46464600u;
constexpr uint16_t kFFVersion = 1;

uint64_t fnv1a64(const uint8_t* data, size_t n);

class BinaryWriter {
public:
  void u8(uint8_t v);
  void u16(uint16_t v);
  void u32(uint32_t v);
  void u64(uint64_t v);
  void bytes(const uint8_t* p, size_t n);
  void str16(const std::string& s);
  void uuid(const uuid128& u);
  void generation(const Generation& g);
  void enumv(uint32_t v);
  const std::vector<uint8_t>& buffer() const { return buf_; }
  size_t size() const { return buf_.size(); }
private:
  std::vector<uint8_t> buf_;
};

class BinaryReader {
public:
  explicit BinaryReader(const uint8_t* data, size_t n) : p_(data), n_(n) {}
  bool ok() const { return ok_; }
  size_t remaining() const { return n_ - pos_; }
  uint8_t u8();
  uint16_t u16();
  uint32_t u32();
  uint64_t u64();
  void bytes(uint8_t* out, size_t n);
  std::string str16();
  uuid128 uuid();
  Generation generation();
  uint32_t enumv();
  size_t pos() const { return pos_; }
private:
  const uint8_t* p_; size_t n_; size_t pos_ = 0; bool ok_ = true;
  bool need(size_t n) { if (pos_ + n > n_) { ok_ = false; return false; } return true; }
};

struct FrameHeader { uint8_t kind; std::vector<uint8_t> payload; uint64_t checksum; };
std::vector<uint8_t> encode_frame(uint8_t kind, const uint8_t* payload, size_t n);
bool decode_frame(const uint8_t* data, size_t n, size_t offset, FrameHeader& out, size_t& next_offset);

void encode_failure_record(BinaryWriter& w, const FailureRecord& r);
bool decode_failure_record(BinaryReader& r, FailureRecord& rec);
bool validate_authority(const AuthorityEnvelope& e);

} // namespace ff
