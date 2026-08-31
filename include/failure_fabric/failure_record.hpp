#pragma once
// Immutable canonical failure record. Copyright 2026 Summon Software Labs.
#include "failure_fabric/id.hpp"
#include "failure_fabric/enum.hpp"
#include "failure_fabric/authority.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <random>

namespace ff {

// Once committed, a FailureRecord is immutable. It is constructed through
// FailureRecordBuilder (which validates required identity/authority) and is
// only ever appended to an immutable failure log.
struct FailureRecord {
  FailureId           failure_id{};
  OperationId         operation{};
  RequestId           request{};
  WorkloadId          workload{};
  AttemptId           attempt{};
  DispatchId          dispatch{};
  WorkerId            worker{};
  NodeId              node{};
  DeviceId            device{};

  FailureClass        failure_class = FailureClass::UNKNOWN;
  FailureOrigin       origin = FailureOrigin::UNKNOWN;
  uint32_t            failure_code = 0;
  std::string         summary;      // human-readable
  std::string         reason;       // machine-readable (stable "key=value; ...")
  FailurePhase        phase = FailurePhase::UNKNOWN;

  uint64_t            timestamp_ms = 0;
  Ambiguity           completion = Ambiguity::UNKNOWN_OUTCOME;
  SideEffectState     side_effect_state = SideEffectState::UNKNOWN;
  uint32_t            unknown_side_effects = 0;

  AuthorityEnvelope   authority{};
  Retryability        retryability = Retryability::UNKNOWN;
  RollbackRequirement rollback = RollbackRequirement::NONE;
  CompensationRequirement compensation = CompensationRequirement::NONE;
  Terminality         terminality = Terminality::UNKNOWN;
  RecoveryOwner       recovery_owner = RecoveryOwner::NONE;
  Provenance          provenance = Provenance::UNKNOWN;

  FailureId           causal_parent{};
  std::vector<FailureId> dependencies{};
};

// Builder validates required identity/authority before producing the record.
class FailureRecordBuilder {
public:
  FailureRecordBuilder& operation(OperationId v) { r.operation = v; return *this; }
  FailureRecordBuilder& request(RequestId v) { r.request = v; return *this; }
  FailureRecordBuilder& workload(WorkloadId v) { r.workload = v; return *this; }
  FailureRecordBuilder& attempt(AttemptId v) { r.attempt = v; return *this; }
  FailureRecordBuilder& dispatch(DispatchId v) { r.dispatch = v; return *this; }
  FailureRecordBuilder& worker(WorkerId v) { r.worker = v; return *this; }
  FailureRecordBuilder& node(NodeId v) { r.node = v; return *this; }
  FailureRecordBuilder& device(DeviceId v) { r.device = v; return *this; }
  FailureRecordBuilder& id(FailureId v) { r.failure_id = v; return *this; }
  FailureRecordBuilder& failure_class(FailureClass v) { r.failure_class = v; return *this; }
  FailureRecordBuilder& origin(FailureOrigin v) { r.origin = v; return *this; }
  FailureRecordBuilder& code(uint32_t v) { r.failure_code = v; return *this; }
  FailureRecordBuilder& summary(std::string v) { r.summary = std::move(v); return *this; }
  FailureRecordBuilder& reason(std::string v) { r.reason = std::move(v); return *this; }
  FailureRecordBuilder& phase(FailurePhase v) { r.phase = v; return *this; }
  FailureRecordBuilder& timestamp(uint64_t v) { r.timestamp_ms = v; return *this; }
  FailureRecordBuilder& completion(Ambiguity v) { r.completion = v; return *this; }
  FailureRecordBuilder& side_effect_state(SideEffectState v) { r.side_effect_state = v; return *this; }
  FailureRecordBuilder& unknown_side_effects(uint32_t v) { r.unknown_side_effects = v; return *this; }
  FailureRecordBuilder& authority(AuthorityEnvelope v) { r.authority = std::move(v); return *this; }
  FailureRecordBuilder& retryability(Retryability v) { r.retryability = v; retry_set_ = true; return *this; }
  FailureRecordBuilder& rollback(RollbackRequirement v) { r.rollback = v; rollback_set_ = true; return *this; }
  FailureRecordBuilder& compensation(CompensationRequirement v) { r.compensation = v; comp_set_ = true; return *this; }
  FailureRecordBuilder& terminality(Terminality v) { r.terminality = v; term_set_ = true; return *this; }
  FailureRecordBuilder& recovery_owner(RecoveryOwner v) { r.recovery_owner = v; return *this; }
  FailureRecordBuilder& provenance(Provenance v) { r.provenance = v; return *this; }
  FailureRecordBuilder& causal_parent(FailureId v) { r.causal_parent = v; return *this; }
  FailureRecordBuilder& dependency(FailureId v) { r.dependencies.push_back(v); return *this; }

  // Derives defaults for the orthogonal dimensions (retryability, rollback,
  // compensation, terminality) from the failure class when the caller did not
  // set them explicitly.
  void derive_defaults();

  FailureRecord build() const;
  FailureRecord build(std::mt19937_64& rng) const;
  const FailureRecord& peek() const { return r; }
private:
  FailureRecord r{};
  bool retry_set_ = false, rollback_set_ = false, comp_set_ = false, term_set_ = false;
};

void derive_dimensions(FailureRecord& rec);
std::string to_json(const FailureRecord& rec);
} // namespace ff
