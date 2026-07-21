#pragma once

#include <cmath>
#include <span>

#include "automata/core/transport.hpp"
#include "automata/graph/builder.hpp"

// cycle: the musical-time leaf (ADR 0008). Emits a 0..1 phase ramp over
// `beats` beats, re-derived from the transport's absolute beat position
// every block — never integrated — so every cycle in every graph is
// phase-locked to every other, and position survives swaps and tempo
// changes with nothing to transfer. The cycle length is a patchable value;
// unbound (a bare Graph with no transport) a cycle emits 0.

namespace automata {

namespace detail {

struct ClockState {
  const Transport* transport = nullptr;
};

inline void clock_process(void* state,
                          const float* const*,
                          const std::byte*,
                          const float* values,
                          float* out,
                          std::uint32_t nframes) {
  const auto& s = *static_cast<ClockState*>(state);
  const double beats = static_cast<double>(values[0]);
  if (s.transport == nullptr || !(beats > 0.0)) {
    for (std::uint32_t i = 0; i < nframes; ++i) {
      out[i] = 0.f;
    }
    return;
  }
  const double pos = s.transport->beat_pos / beats;
  double phase = pos - std::floor(pos);
  const double inc = 1.0 / (beats * s.transport->samples_per_beat());
  for (std::uint32_t i = 0; i < nframes; ++i) {
    out[i] = static_cast<float>(phase);
    phase += inc;
    if (phase >= 1.0) {
      phase -= 1.0;
    }
  }
}

inline const KernelInfo ClockInfo{
    .type_hash = hash_string("automata.Cycle"),
    .state_size = sizeof(ClockState),
    .state_align = alignof(ClockState),
    .output_count = 1,
    .construct = +[](void* s) { ::new (s) ClockState{}; },
    .reset = +[](void*) {},  // nothing but the binding, which persists
    .process = &clock_process,
};

[[nodiscard]] inline Signal clock_ramp(float beats) {
  const float values[1] = {beats};
  return ActiveGraph::current().add_node(&ClockInfo, ClockInfo.type_hash, {},
                                         values, {}, {});
}

}  // namespace detail

}  // namespace automata
