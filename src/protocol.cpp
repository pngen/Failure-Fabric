#include "failure_fabric/protocol.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <climits>

namespace ff {

static bool init_done = false;
static SOCKET to_sock(SocketHandle h) { return static_cast<SOCKET>(h); }
static SocketHandle to_handle(SOCKET s) { return static_cast<SocketHandle>(s); }

bool winsock_init() {
  if (init_done) return true;
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d) != 0) return false;
  init_done = true;
  return true;
}
void winsock_shutdown() { if (init_done) { WSACleanup(); init_done = false; } }

std::vector<uint8_t> encode_wire(const WireMessage& m) {
  BinaryWriter w;
  w.enumv(static_cast<uint32_t>(m.type));
  w.uuid(m.op.raw); w.uuid(m.attempt.raw); w.uuid(m.worker.raw); w.uuid(m.boot.raw);
  w.generation(m.authority.coordinator_epoch); w.uuid(m.authority.worker_boot.raw);
  w.uuid(m.authority.attempt.raw); w.uuid(m.authority.dispatch.raw);
  w.generation(m.authority.attempt_generation); w.generation(m.authority.operation_generation);
  w.generation(m.authority.failure_generation); w.generation(m.authority.recovery_generation);
  w.u8(m.flag ? 1 : 0);
  w.str16(m.text);
  w.u64(m.seq);
  return w.buffer();
}

bool decode_wire(const uint8_t* data, size_t n, WireMessage& out) {
  BinaryReader r(data, n);
  uint32_t t = r.enumv(); if (!r.ok() || t >= 16) return false;
  out.type = static_cast<ProtocolMessage>(t);
  out.op = OperationId(r.uuid()); out.attempt = AttemptId(r.uuid());
  out.worker = WorkerId(r.uuid()); out.boot = WorkerBootId(r.uuid());
  out.authority.coordinator_epoch = CoordinatorEpoch(r.generation());
  out.authority.worker_boot = WorkerBootId(r.uuid());
  out.authority.attempt = AttemptId(r.uuid());
  out.authority.dispatch = DispatchId(r.uuid());
  out.authority.attempt_generation = AttemptGeneration(r.generation());
  out.authority.operation_generation = OperationGeneration(r.generation());
  out.authority.failure_generation = FailureGeneration(r.generation());
  out.authority.recovery_generation = RecoveryGeneration(r.generation());
  out.flag = r.u8() != 0;
  out.text = r.str16();
  out.seq = r.u64();
  return r.ok();
}

static bool send_all(SOCKET s, const uint8_t* p, size_t n) {
  while (n > 0) {
    int sent = ::send(s, reinterpret_cast<const char*>(p), static_cast<int>(n > INT_MAX ? INT_MAX : n), 0);
    if (sent <= 0) return false;
    p += sent; n -= static_cast<size_t>(sent);
  }
  return true;
}
static bool recv_all(SOCKET s, uint8_t* p, size_t n) {
  while (n > 0) {
    int got = ::recv(s, reinterpret_cast<char*>(p), static_cast<int>(n > INT_MAX ? INT_MAX : n), 0);
    if (got <= 0) return false;
    p += got; n -= static_cast<size_t>(got);
  }
  return true;
}

bool send_frame(SocketHandle h, const WireMessage& m) {
  SOCKET s = to_sock(h);
  if (s == INVALID_SOCKET) return false;
  auto payload = encode_wire(m);
  auto frame = encode_frame(static_cast<uint8_t>(m.type), payload.data(), payload.size());
  return send_all(s, frame.data(), frame.size());
}
bool recv_frame(SocketHandle h, WireMessage& out) {
  SOCKET s = to_sock(h);
  uint8_t hdr[11];
  if (!recv_all(s, hdr, 11)) return false;
  BinaryReader hr(hdr, 11);
  if (hr.u32() != kFFMagic || hr.u16() != kFFVersion) return false;
  hr.u8();
  uint32_t len = hr.u32();
  if (!hr.ok()) return false;
  if (len > (32u * 1024u * 1024u)) return false;
  std::vector<uint8_t> body(len + 8);
  if (!recv_all(s, body.data(), len + 8)) return false;
  std::vector<uint8_t> full(hdr, hdr + 11);
  full.insert(full.end(), body.begin(), body.end());
  FrameHeader fh; size_t next = 0;
  if (!decode_frame(full.data(), full.size(), 0, fh, next)) return false;
  WireMessage m;
  if (!decode_wire(fh.payload.data(), fh.payload.size(), m)) return false;
  out = m;
  return true;
}

TcpSocket::~TcpSocket() { close(); }
TcpSocket& TcpSocket::operator=(TcpSocket&& o) noexcept {
  if (this != &o) { close(); s_ = o.s_; o.s_ = 0; }
  return *this;
}
void TcpSocket::close() {
  SOCKET s = to_sock(s_);
  if (s != INVALID_SOCKET) { ::shutdown(s, SD_BOTH); ::closesocket(s); }
  s_ = 0;
}
bool TcpSocket::send(const WireMessage& m) { std::lock_guard<std::mutex> lk(wmu_); return send_frame(s_, m); }
bool TcpSocket::recv(WireMessage& out) { return recv_frame(s_, out); }
bool TcpSocket::shutdown_send() { SOCKET s = to_sock(s_); return s != INVALID_SOCKET && ::shutdown(s, SD_SEND) == 0; }
bool TcpSocket::connect_to(const char* host, uint16_t port) {
  if (!winsock_init()) return false;
  SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET) return false;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, host, &addr.sin_addr);
  if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
    ::closesocket(s);
    return false;
  }
  s_ = to_handle(s);
  return true;
}

TcpListener::~TcpListener() {
  SOCKET s = to_sock(s_);
  if (s != INVALID_SOCKET) { ::closesocket(s); }
}
bool TcpListener::listen(uint16_t port) {
  if (!winsock_init()) return false;
  SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == INVALID_SOCKET) return false;
  int opt = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&opt), sizeof(opt));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);
  if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) { ::closesocket(s); return false; }
  if (::listen(s, SOMAXCONN) == SOCKET_ERROR) { ::closesocket(s); return false; }
  s_ = to_handle(s);
  return true;
}
TcpSocket TcpListener::accept() {
  SOCKET s = to_sock(s_);
  if (s == INVALID_SOCKET) return TcpSocket{};
  sockaddr_in cli{};
  int cliLen = sizeof(cli);
  SOCKET cs = ::accept(s, reinterpret_cast<sockaddr*>(&cli), &cliLen);
  if (cs == INVALID_SOCKET) return TcpSocket{};
  return TcpSocket(to_handle(cs));
}
} // namespace ff