#pragma once
// Framed TCP protocol for the distributed coordinator/worker runtime.
// Copyright 2026 Summon Software Labs.
#include "failure_fabric/enum.hpp"
#include "failure_fabric/id.hpp"
#include "failure_fabric/authority.hpp"
#include "failure_fabric/persistence.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <mutex>

namespace ff {

// Opaque socket handle (Winsock SOCKET is not exposed in public headers so the
// enum/windows-macro namespace stays clean). Cast internally in protocol.cpp.
using SocketHandle = uint64_t;

struct WireMessage {
  ProtocolMessage   type = ProtocolMessage::HEARTBEAT;
  OperationId       op{};
  AttemptId         attempt{};
  WorkerId          worker{};
  WorkerBootId      boot{};
  AuthorityEnvelope authority{};
  bool              flag = false;
  std::string       text;
  uint64_t          seq = 0;
};

std::vector<uint8_t> encode_wire(const WireMessage& m);
bool decode_wire(const uint8_t* data, size_t n, WireMessage& out);

bool send_frame(SocketHandle sock, const WireMessage& m);
bool recv_frame(SocketHandle sock, WireMessage& out);

class TcpSocket {
public:
  TcpSocket() = default;
  explicit TcpSocket(SocketHandle s) : s_(s) {}
  ~TcpSocket();
  TcpSocket(const TcpSocket&) = delete;
  TcpSocket& operator=(const TcpSocket&) = delete;
  TcpSocket(TcpSocket&& o) noexcept : s_(o.s_) { o.s_ = 0; }
  TcpSocket& operator=(TcpSocket&& o) noexcept;

  bool valid() const { return s_ != 0; }
  SocketHandle get() const { return s_; }
  void close();
  bool send(const WireMessage& m);
  bool recv(WireMessage& out);
  bool shutdown_send();

  // Client connect (Winsock in cpp). Returns true on success.
  bool connect_to(const char* host, uint16_t port);

private:
  SocketHandle s_ = 0;
  mutable std::mutex wmu_;
};

// Server-side listen/accept.
class TcpListener {
public:
  TcpListener() = default;
  ~TcpListener();
  TcpListener(const TcpListener&) = delete;
  TcpListener& operator=(const TcpListener&) = delete;

  bool listen(uint16_t port);
  // Accept a single connection; returns a connected TcpSocket (invalid if failed).
  TcpSocket accept();
  SocketHandle handle() const { return s_; }
private:
  SocketHandle s_ = 0;
};

bool winsock_init();
void winsock_shutdown();

} // namespace ff
