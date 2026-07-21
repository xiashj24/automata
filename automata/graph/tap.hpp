#pragma once

#include <cstring>
#include <string_view>

#include "automata/config.hpp"
#include "automata/graph/builder.hpp"

// Taps: the feedback primitive (ADR 0002/0011). A write node owns the delay
// buffer; read nodes reference it by tap id. How a read compensates for its
// schedule position is decided per graph (schedule.hpp): in a feedback cycle
// the delay floors at one sample — sub-block cycles run sample-serial —
// while outside a cycle any delay >= 0 is sample-exact. The id folds into
// type_hash (identity); the op bytes carry it again, unhashed, for
// read/write pairing when a Graph is built.

namespace automata {

namespace detail {

// The tap delay buffer. Deliberately graph-owned rather than a kernel: the
// graph layer cannot depend on the kernel layer.
class TapState {
public:
  void write(float sample) {
    buf_[head_ & Mask] = sample;
    ++head_;
  }

  // back = 0 reads the newest written sample; fractional values interpolate
  // toward older ones with 4-point Hermite (the modulated-delay standard),
  // falling back to linear inside the newest interval, where the 4-point
  // window would need a sample that doesn't exist yet.
  [[nodiscard]] float read(float back) const {
    constexpr float MaxBack = static_cast<float>(TapBufferSamples - 4);
    const float b = back < 0.f ? 0.f : (back > MaxBack ? MaxBack : back);
    const auto bi = static_cast<std::uint32_t>(b);
    const float frac = b - static_cast<float>(bi);
    const std::uint32_t newest = head_ - 1;
    const float x0 = buf_[(newest - bi) & Mask];
    const float x1 = buf_[(newest - bi - 1) & Mask];
    if (bi == 0) {
      return x0 + (x1 - x0) * frac;
    }
    const float xm1 = buf_[(newest - bi + 1) & Mask];
    const float x2 = buf_[(newest - bi - 2) & Mask];
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * frac + c2) * frac + c1) * frac + x0;
  }

private:
  static constexpr std::uint32_t Mask =
      static_cast<std::uint32_t>(TapBufferSamples - 1);

  float buf_[TapBufferSamples] = {};
  std::uint32_t head_ = 0;
};

inline void tap_write_process(void* state,
                              const float* const* ins,
                              const std::byte*,
                              const float*,
                              float* out,
                              std::uint32_t nframes) {
  auto& tap = *static_cast<TapState*>(state);
  for (std::uint32_t i = 0; i < nframes; ++i) {
    tap.write(ins[0][i]);
    out[i] = ins[0][i];
  }
}

// A read's state pointer aliases its write node's TapState (wired by Graph).
// The input is the delay in seconds. Three process variants cover the three
// schedule positions a read can occupy (schedule.hpp); each block-mode
// sample compensates for its own position so a constant delay stays
// sample-exact across block boundaries.

// Scheduled before its write (a block-mode feedback cycle): only samples up
// to the previous block boundary exist, so the delay floors at one block.
inline void tap_read_process(void* state,
                             const float* const* ins,
                             const std::byte*,
                             const float*,
                             float* out,
                             std::uint32_t nframes) {
  const auto& tap = *static_cast<const TapState*>(state);
  constexpr float MinDelay = static_cast<float>(BlockSize);
  for (std::uint32_t i = 0; i < nframes; ++i) {
    const float samples = ins[0][i] * SampleRateF;
    const float d = samples < MinDelay ? MinDelay : samples;
    out[i] = tap.read(d - 1.f - static_cast<float>(i));
  }
}

// Scheduled after its write (no cycle): the whole block is already written,
// so any delay >= 0 is sample-exact — zero is a passthrough.
inline void tap_read_post_write_process(void* state,
                                        const float* const* ins,
                                        const std::byte*,
                                        const float*,
                                        float* out,
                                        std::uint32_t nframes) {
  const auto& tap = *static_cast<const TapState*>(state);
  for (std::uint32_t i = 0; i < nframes; ++i) {
    const float samples = ins[0][i] * SampleRateF;
    const float d = samples < 0.f ? 0.f : samples;
    out[i] =
        tap.read(d + static_cast<float>(BlockSize - 1) - static_cast<float>(i));
  }
}

// Inside a sample-serial island: the write's head advances in lockstep one
// sample behind, so the floor is a single sample — read(0) is a true z^-1.
inline void tap_read_island_process(void* state,
                                    const float* const* ins,
                                    const std::byte*,
                                    const float*,
                                    float* out,
                                    std::uint32_t nframes) {
  const auto& tap = *static_cast<const TapState*>(state);
  for (std::uint32_t i = 0; i < nframes; ++i) {
    const float samples = ins[0][i] * SampleRateF;
    const float d = samples < 1.f ? 1.f : samples;
    out[i] = tap.read(d - 1.f);
  }
}

inline const KernelInfo TapWriteInfo{
    .type_hash = hash_string("automata.TapWrite"),
    .state_size = sizeof(TapState),
    .state_align = alignof(TapState),
    .output_count = 1,
    .construct = +[](void* s) { ::new (s) TapState{}; },
    .reset = +[](void* s) { *static_cast<TapState*>(s) = TapState{}; },
    .process = &tap_write_process,
};

inline const KernelInfo TapReadInfo{
    .type_hash = hash_string("automata.TapRead"),
    .state_size = 0,  // aliases the write node's state
    .state_align = 1,
    .output_count = 1,
    .construct = +[](void*) {},
    .reset = +[](void*) {},
    .process = &tap_read_process,
};

}  // namespace detail

class Tap {
public:
  // Delayed by seconds (value or signal). In a feedback cycle the floor is
  // one sample — read() is a true z^-1 — and sub-block cycles run
  // sample-serial; outside a cycle any delay >= 0 is sample-exact.
  [[nodiscard]] Signal read(Signal delay_seconds = 0.f) const {
    return ActiveGraph::current().add_node(
        &detail::TapReadInfo, hash_combine(detail::TapReadInfo.type_hash, id_),
        {{delay_seconds}}, {}, {}, id_bytes());
  }

  void write(Signal in) const {
    (void)ActiveGraph::current().add_node(
        &detail::TapWriteInfo,
        hash_combine(detail::TapWriteInfo.type_hash, id_), {{in}}, {}, {},
        id_bytes(), /*sink=*/true);
  }

private:
  friend class GraphBuilder;
  explicit Tap(Hash id) : id_(id) {}

  [[nodiscard]] std::span<const std::byte> id_bytes() const {
    return {reinterpret_cast<const std::byte*>(&id_), sizeof(id_)};
  }

  Hash id_;
};

inline Tap GraphBuilder::tap(std::string_view name) {
  return Tap{hash_combine(hash_string("automata.tap"), hash_string(name))};
}

inline Tap GraphBuilder::tap() {
  return Tap{hash_combine(hash_string("automata.tap.anon"), tap_count_++)};
}

// Recovers a tap node's pairing id from its op bytes.
[[nodiscard]] inline Hash tap_id(const GraphDef& def,
                                 const GraphDef::Node& node) {
  Hash id = 0;
  std::memcpy(&id, def.op_data.data() + node.op_begin, sizeof(id));
  return id;
}

}  // namespace automata
