#define _CRT_SECURE_NO_WARNINGS
#include "failure_fabric/protocol.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>

// Worker OS process: connects to the coordinator, registers, and waits for a
// RUN dispatch. It performs a real side effect (writes an attempt marker file
// scoped by attempt generation) and then reports completion. Under failure
// injection the harness TerminateProcess'es the worker before the COMPLETE
// ack, so the coordinator observes the side-effect-unknown ambiguity.
//
// Usage: ff_worker <host> <port> <workdir> <mode>
//   mode: normal  -> report COMPLETE after side effect
//         slow    -> sleep a while before COMPLETE (gives harness a kill window)
//         fail    -> report FAILURE_KNOWN before side effect (scenario: direct safe retry)
using namespace ff;
int main(int argc, char** argv) {
  if (argc < 5) { std::fprintf(stderr, "usage: ff_worker host port workdir mode\n"); return 2; }
  const char* host = argv[1];
  uint16_t port = static_cast<uint16_t>(std::atoi(argv[2]));
  std::string workdir = argv[3];
  std::string mode = argv[4];

  if (!winsock_init()) { std::fprintf(stderr, "worker: winsock init failed\n"); return 2; }
  TcpSocket sock;
  if (!sock.connect_to(host, port)) { std::fprintf(stderr, "worker: connect failed\n"); return 2; }

  WireMessage reg;
  reg.type = ProtocolMessage::REGISTER;
  reg.worker = WorkerId::generate();
  reg.boot = WorkerBootId::generate();
  reg.text = workdir;
  if (!sock.send(reg)) { std::fprintf(stderr, "worker: reg send failed\n"); return 2; }

  WireMessage ack;
  if (!sock.recv(ack) || ack.type != ProtocolMessage::REGISTER_ACK) {
    std::fprintf(stderr, "worker: no register ack\n"); return 2;
  }

  for (;;) {
    WireMessage m;
    if (!sock.recv(m)) { std::fprintf(stderr, "worker(%s): coordinator lost\n", mode.c_str()); return 0; }
    if (m.type == ProtocolMessage::DISPATCH) {
      WireMessage run;
      run.type = ProtocolMessage::RUNNING;
      run.op = m.op; run.attempt = m.attempt; run.worker = reg.worker; run.boot = reg.boot;
      run.authority = m.authority;
      if (!sock.send(run)) { std::fprintf(stderr, "worker: running send failed\n"); return 2; }

      // Real side effect: write an attempt marker file scoped by attempt gen.
      // This is the side effect that is "unknown" if the worker dies before ack.
      char path[512];
      std::snprintf(path, sizeof(path), "%s\attempt_%s_marker.txt", workdir.c_str(), m.authority.attempt_generation.is_unset() ? "0" : std::to_string(m.authority.attempt_generation.value).c_str());
      FILE* f = std::fopen(path, "a");
      if (f) { std::fprintf(f, "worker_%s_attempt_%s marker\n", mode.c_str(), std::to_string(m.authority.attempt_generation.value).c_str()); std::fclose(f); }

      if (mode == "fail") {
        WireMessage fail;
        fail.type = ProtocolMessage::FAILURE;
        fail.op = m.op; fail.attempt = m.attempt; fail.worker = reg.worker; fail.boot = reg.boot;
        fail.flag = false; // known failure, before side effect
        fail.authority = m.authority;
        fail.text = "known no-side-effect failure";
        if (!sock.send(fail)) return 2;
        return 0;
      }
      if (mode == "slow") std::this_thread::sleep_for(std::chrono::milliseconds(4000));
      // normal: report COMPLETE
      WireMessage comp;
      comp.type = ProtocolMessage::COMPLETE;
      comp.op = m.op; comp.attempt = m.attempt; comp.worker = reg.worker; comp.boot = reg.boot;
      comp.authority = m.authority;
      comp.text = "result=done";
      if (!sock.send(comp)) { std::fprintf(stderr, "worker: complete send failed (likely killed)\n"); return 0; }
      return 0;
    }
    if (m.type == ProtocolMessage::SHUTDOWN) { return 0; }
  }
  return 0;
}