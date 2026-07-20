#pragma once

#include <cstring>
#include <span>
#include <string_view>

#include "automata/core/control_bus.hpp"
#include "automata/graph/builder.hpp"

// param: the external-control leaf (ADR 0007). The slot name is identity —
// param("cutoff") pairs with itself across every edit, and OSC "/cutoff"
// lands on it — while the fallback is a patchable value. Graph wires the
// slot from the engine's ControlBus; unwired (offline, or a full bus) a
// param simply plays its fallback. Either way the output glides through
// the shared ramp, so external control never steps.

namespace automata {

namespace detail {

struct ParamState {
  const ControlBus::Slot* slot = nullptr;
  RampState ramp;
};

inline void param_process(void* state,
                          const float* const*,
                          const std::byte*,
                          const float* values,
                          float* out,
                          std::uint32_t nframes) {
  auto& s = *static_cast<ParamState*>(state);
  const float v = s.slot != nullptr ? s.slot->read_or(values[0]) : values[0];
  ramp_toward(s.ramp, v, out, nframes);
}

inline const KernelInfo ParamInfo{
    .type_hash = hash_string("automata.Param"),
    .state_size = sizeof(ParamState),
    .state_align = alignof(ParamState),
    .output_count = 1,
    .construct = +[](void* s) { ::new (s) ParamState{}; },
    .reset =
        +[](void* s) {
          auto& p = *static_cast<ParamState*>(s);
          p = ParamState{.slot = p.slot, .ramp = {}};
        },
    .process = &param_process,
};

}  // namespace detail

// Reads the ControlBus slot `name`, playing `fallback` until something
// writes it. The name is an OSC address without its leading '/'.
[[nodiscard]] inline Signal param(std::string_view name, float fallback = 0.f) {
  const Hash name_hash = hash_string(name);
  const float values[1] = {fallback};
  const std::span<const std::byte> op{
      reinterpret_cast<const std::byte*>(&name_hash), sizeof(name_hash)};
  return ActiveGraph::current().add_node(
      &detail::ParamInfo, hash_combine(detail::ParamInfo.type_hash, name_hash),
      {}, values, {}, op);
}

// Recovers a param node's slot name from its op bytes.
[[nodiscard]] inline Hash param_name(const GraphDef& def,
                                     const GraphDef::Node& node) {
  Hash name = 0;
  std::memcpy(&name, def.op_data.data() + node.op_begin, sizeof(name));
  return name;
}

}  // namespace automata
