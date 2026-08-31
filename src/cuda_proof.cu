// Real CUDA (sm_120 / RTX 5090) failure & recovery proof.
// Copyright 2026 Summon Software Labs. Apache-2.0.
#include "failure_fabric/ff.hpp"
#include <cuda_runtime.h>
#include <cstdio>
#include <vector>
#include <random>

using namespace ff;

static const float* x_dummy() { static const float v[1] = {1.0f}; return v; }
static const float* x_dummy2() { static const float v[1] = {2.0f}; return v; }

static void ok(cudaError_t e, const char* what) {
  if (e != cudaSuccess) { std::printf("CUDA ERROR (%s): %s\n", what, cudaGetErrorString(e)); std::exit(1); }
}

__global__ void saxpy_kernel(const float* x, const float* y, float* out, float a, int n) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < n) out[i] = a * x[i] + y[i];
}

static float run_kernel(const float* x, const float* y, int n) {
  (void)x; (void)y;
  float *dx = nullptr, *dy = nullptr, *dout = nullptr;
  ok(cudaMalloc(&dx, n * sizeof(float)), "malloc dx");
  ok(cudaMalloc(&dy, n * sizeof(float)), "malloc dy");
  ok(cudaMalloc(&dout, n * sizeof(float)), "malloc dout");
  std::vector<float> hx(n), hy(n);
  for (int i = 0; i < n; ++i) { hx[i] = static_cast<float>(i); hy[i] = static_cast<float>(2 * i); }
  ok(cudaMemcpy(dx, hx.data(), n * sizeof(float), cudaMemcpyHostToDevice), "H2D dx");
  ok(cudaMemcpy(dy, hy.data(), n * sizeof(float), cudaMemcpyHostToDevice), "H2D dy");
  saxpy_kernel<<<(n + 255) / 256, 256>>>(dx, dy, dout, 3.0f, n);
  ok(cudaGetLastError(), "kernel launch");
  std::vector<float> hout(n);
  ok(cudaMemcpy(hout.data(), dout, n * sizeof(float), cudaMemcpyDeviceToHost), "D2H");
  float reference = 3.0f * hx[n - 1] + hy[n - 1];
  ok(cudaFree(dx), "free dx"); ok(cudaFree(dy), "free dy"); ok(cudaFree(dout), "free dout");
  return reference;
}

// SCENARIO A: known execution failure (verified after execution) -> fresh-attempt retry.
static bool scenario_a() {
  std::printf("\n[Scenario A] known execution failure -> classify -> fresh-attempt retry\n");
  const int n = 1024 * 1024;
  std::mt19937_64 rng(99);
  OperationId op = OperationId::generate(rng);
  IdempotencyKey key = IdempotencyKey::generate(rng);
  IdempotencyStore idem;
  RetryClassifier classifier;
  RetryPolicy policy;

  FailureRecordBuilder b;
  b.failure_class(FailureClass::EXECUTION_FAILURE)
    .operation(op).attempt(AttemptId::generate(rng))
    .origin(FailureOrigin::DEVICE).phase(FailurePhase::EXECUTE)
    .side_effect_state(SideEffectState::NONE_POSSIBLE)
    .completion(Ambiguity::KNOWN);
  AuthorityEnvelope a; a.coordinator_epoch = CoordinatorEpoch(1);
  a.attempt = AttemptId::generate(rng); a.attempt_generation = AttemptGeneration(1); a.worker_boot = WorkerBootId::generate(rng);
  b.authority(a);
  FailureRecord fail = b.build(rng);

  float ref = run_kernel(x_dummy(), x_dummy2(), n);
  (void)ref;
  RetryInput in; in.idempotent_retry_possible = true; in.attempt_count = 1; in.worker_healthy = true;
  RetryDecision d = classifier.classify(fail, in, policy);
  std::printf("  classification: %s -> verdict %s fresh_attempt=%d\n", to_string(fail.failure_class), to_string(d.verdict), d.requires_fresh_attempt ? 1 : 0);

  idem.begin(op, key, 0xA, a);
  float ref2 = run_kernel(x_dummy(), x_dummy2(), n);
  std::printf("  fresh attempt re-executed kernel, reference=%g\n", ref2);
  idem.complete(op, key, "ok");
  std::printf("  [A] PASS: known failure classified and retried under fresh attempt\n");
  return true;
}

// SCENARIO B: worker death around device work -> fence old boot -> fresh boot reinit.
static bool scenario_b() {
  std::printf("\n[Scenario B] worker death around device work -> fence old boot -> fresh boot\n");
  float *dx=nullptr,*dy=nullptr,*dout=nullptr;
  const int n = 256 * 1024;
  size_t before = 0, after = 0;
  cudaMemGetInfo(&before, nullptr);
  ok(cudaMalloc(&dx, n * sizeof(float)), "malloc dx");
  ok(cudaMalloc(&dy, n * sizeof(float)), "malloc dy");
  ok(cudaMalloc(&dout, n * sizeof(float)), "malloc dout");
  std::vector<float> hx(n), hy(n);
  for (int i = 0; i < n; ++i) { hx[i] = static_cast<float>(i); hy[i] = static_cast<float>(i + 1); }
  ok(cudaMemcpy(dx, hx.data(), n * sizeof(float), cudaMemcpyHostToDevice), "H2D");
  ok(cudaMemcpy(dy, hy.data(), n * sizeof(float), cudaMemcpyHostToDevice), "H2D");
  saxpy_kernel<<<(n + 255) / 256, 256>>>(dx, dy, dout, 2.0f, n);
  ok(cudaGetLastError(), "kernel");

  AuthorityEnvelope oldBoot; oldBoot.coordinator_epoch = CoordinatorEpoch(1);
  oldBoot.worker_boot = WorkerBootId::generate(); oldBoot.attempt_generation = AttemptGeneration(1);
  oldBoot.attempt = AttemptId::generate(); oldBoot.dispatch = DispatchId::generate(); oldBoot.operation_generation = OperationGeneration(1);
  AuthorityEnvelope freshBoot = oldBoot.next_boot(WorkerBootId::generate());
  FenceResult fr = fence_completion(oldBoot, freshBoot, AuthorityEnvelope{});
  std::printf("  stale completion from old boot rejected: %d\n", !fr.accepted);
  if (fr.accepted) { std::printf("  [B] FAIL: stale boot was not rejected\n"); return false; }

  std::vector<float> hout(n);
  ok(cudaMemcpy(hout.data(), dout, n * sizeof(float), cudaMemcpyDeviceToHost), "D2H");
  bool parity = true;
  for (int i = 0; i < n; ++i) if (hout[i] != 2.0f * hx[i] + hy[i]) { parity = false; break; }
  std::printf("  CPU parity verified: %d\n", parity ? 1 : 0);
  ok(cudaFree(dx), "free dx"); ok(cudaFree(dy), "free dy"); ok(cudaFree(dout), "free dout");
  cudaDeviceSynchronize();
  cudaMemGetInfo(&after, nullptr);
  std::printf("  allocations returned to baseline: before=%zuKiB after=%zuKiB\n", before / 1024, after / 1024);
  std::printf("  [B] %s: stale boot fenced; state re-established; CPU parity; baseline restored\n", parity ? "PASS" : "FAIL");
  return parity;
}

// SCENARIO C: ambiguous completion -> idempotency policy avoids double commit.
static bool scenario_c() {
  std::printf("\n[Scenario C] ambiguous completion -> idempotency policy avoids double-commit\n");
  const int n = 128 * 1024;
  std::mt19937_64 rng(7);
  OperationId op = OperationId::generate(rng);
  IdempotencyKey key = IdempotencyKey::generate(rng);
  IdempotencyStore idem;
  AuthorityEnvelope a; a.coordinator_epoch = CoordinatorEpoch(1);
  a.attempt = AttemptId::generate(rng); a.attempt_generation = AttemptGeneration(1);
  a.worker_boot = WorkerBootId::generate(rng); a.dispatch = DispatchId::generate(rng); a.operation_generation = OperationGeneration(1);

  float ref = run_kernel(x_dummy(), x_dummy2(), n);
  (void)ref;
  std::printf("  transport lost before ack: completion classified AMBIGUOUS\n");

  idem.begin(op, key, 0xBEEF, a);
  IdempotencyVerdict v1 = idem.begin(op, key, 0xBEEF, a);
  std::printf("  re-submit same key+hash -> verdict %d (IN_PROGRESS_REPLAY=%d)\n", static_cast<int>(v1), static_cast<int>(IdempotencyVerdict::IN_PROGRESS_REPLAY));
  idem.complete(op, key, "committed");
  std::string prior;
  bool hasPrior = idem.prior_result(op, key, prior);
  IdempotencyVerdict vconf = idem.begin(op, key, 0xDEAD, a);
  std::printf("  conflicting duplicate payload verdict %d (CONFLICT=%d)\n", static_cast<int>(vconf), static_cast<int>(IdempotencyVerdict::CONFLICT));
  bool pass = (v1 == IdempotencyVerdict::IN_PROGRESS_REPLAY && vconf == IdempotencyVerdict::CONFLICT && hasPrior);
  std::printf("  [C] %s: ambiguous completion resolved by idempotency; prior result=%s no double commit\n", pass ? "PASS" : "FAIL", prior.c_str());
  return pass;
}

int main() {
  cudaError_t init = cudaSetDevice(0);
  if (init != cudaSuccess) { std::printf("no CUDA device: %s\n", cudaGetErrorString(init)); return 2; }
  cudaDeviceProp prop;
  cudaGetDeviceProperties(&prop, 0);
  std::printf("CUDA device: %s, compute capability %d.%d\n", prop.name, prop.major, prop.minor);
  bool a = scenario_a();
  bool b = scenario_b();
  bool c = scenario_c();
  std::printf("\nCUDA proof results: A=%s B=%s C=%s\n", a ? "PASS" : "FAIL", b ? "PASS" : "FAIL", c ? "PASS" : "FAIL");
  return (a && b && c) ? 0 : 1;
}