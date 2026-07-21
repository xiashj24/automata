#pragma once

#include <cstring>
#include <span>

#include "automata/graph/builder.hpp"

// Phase-consuming rhythm nodes (ADR 0008): position is floor(phase · N),
// derived every sample, never counted — so a running sequence needs no
// state transfer to keep its place across a swap. Step contents and the
// euclid parameters are patchable values; a seq's step *count* folds into
// its identity, so a length edit is a structural swap.

namespace automata {

namespace detail {

inline void seq_process(void*,
                        const float* const* inputs,
                        const std::byte* op,
                        const float* values,
                        float* out,
                        std::uint32_t nframes) {
  std::uint32_t count = 0;
  std::memcpy(&count, op, sizeof(count));
  const float* phase = inputs[0];
  for (std::uint32_t i = 0; i < nframes; ++i) {
    auto idx = static_cast<std::int32_t>(phase[i] * static_cast<float>(count));
    idx = idx < 0 ? 0 : idx;
    idx = idx >= static_cast<std::int32_t>(count)
              ? static_cast<std::int32_t>(count) - 1
              : idx;
    out[i] = values[idx];
  }
}

inline const KernelInfo SeqInfo{
    .type_hash = hash_string("automata.Seq"),
    .state_size = 0,
    .state_align = 1,
    .output_count = 1,
    .construct = +[](void*) {},
    .reset = +[](void*) {},
    .process = &seq_process,
};

struct EuclidState {
  float prev_phase = 0.f;
  std::int32_t prev_step = -1;  // fresh fires the downbeat if step 0 hits
};

inline void euclid_process(void* state,
                           const float* const* inputs,
                           const std::byte*,
                           const float* values,
                           float* out,
                           std::uint32_t nframes) {
  auto& s = *static_cast<EuclidState*>(state);
  const float* phase = inputs[0];
  auto n = static_cast<std::int32_t>(values[1]);
  n = n < 1 ? 1 : (n > 4096 ? 4096 : n);
  auto k = static_cast<std::int32_t>(values[0]);
  k = k < 0 ? 0 : (k > n ? n : k);
  const auto r = static_cast<std::int32_t>(values[2]);
  for (std::uint32_t i = 0; i < nframes; ++i) {
    auto step = static_cast<std::int32_t>(phase[i] * static_cast<float>(n));
    step = step < 0 ? 0 : (step >= n ? n - 1 : step);
    const bool entered = step != s.prev_step || s.prev_phase - phase[i] > 0.5f;
    s.prev_phase = phase[i];
    s.prev_step = step;
    // sndkit's closed form (ADR 0008), rotation-normalized for negative r.
    const std::int32_t rotated = ((step + r) % n + n) % n;
    const bool hit = (k * rotated) % n + k >= n;
    out[i] = entered && hit ? 1.f : 0.f;
  }
}

inline const KernelInfo EuclidInfo{
    .type_hash = hash_string("automata.Euclid"),
    .state_size = sizeof(EuclidState),
    .state_align = alignof(EuclidState),
    .output_count = 1,
    .construct = +[](void* s) { ::new (s) EuclidState{}; },
    .reset = +[](void* s) { *static_cast<EuclidState*>(s) = EuclidState{}; },
    .process = &euclid_process,
};

[[nodiscard]] inline Signal seq_steps(Signal phase,
                                      std::span<const float> steps) {
  atm_assert(!steps.empty());
  const auto count = static_cast<std::uint32_t>(steps.size());
  const std::span<const std::byte> op{
      reinterpret_cast<const std::byte*>(&count), sizeof(count)};
  const Signal inputs[1] = {phase};
  return ActiveGraph::current().add_node(
      &SeqInfo, hash_combine(SeqInfo.type_hash, hash_value(count)), inputs,
      steps, {}, op);
}

[[nodiscard]] inline Signal euclid_trig(Signal phase,
                                        float hits,
                                        float steps,
                                        float rotate) {
  const float values[3] = {hits, steps, rotate};
  const Signal inputs[1] = {phase};
  return ActiveGraph::current().add_node(&EuclidInfo, EuclidInfo.type_hash,
                                         inputs, values, {}, {});
}

}  // namespace detail

}  // namespace automata
