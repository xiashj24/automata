#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>

#include <samplerate.hpp>
#include <v2/node.hpp>

namespace automata::v2 {

namespace detail {
inline uint64_t next_node_id() {
  static std::atomic<uint64_t> counter{0};
  return counter.fetch_add(1, std::memory_order_relaxed);
}
}  // namespace detail

// ─── SineOscillator
// ───────────────────────────────────────────────────────────
//
// freq and phase_mod are both Signal — any signal can drive either inlet,
// consistent with the Eurorack single-type model. phase_mod is added to the
// normalised phase [0,1) before the sin, so it is in cycles (1.0 = 2π).
//
// node_id seeds a deterministic per-instance initial phase on reset(), so two
// osc(440) instances in the same patch start at different phases.

class SineOscillator : public Node {
public:
  explicit SineOscillator(Signal freq, Signal phase_mod = {})
      : freq(freq), phase_mod(phase_mod), node_id(detail::next_node_id()) {}

  void reset() override {
    phase = static_cast<float>(node_id & 0xFFFF) / 65536.f;
    freq.reset();
    if (phase_mod)
      phase_mod.reset();
  }

protected:
  float process() override {
    constexpr float two_pi = 2.f * std::numbers::pi_v<float>;
    float prev = phase;
    phase += freq.next() / SampleRate;
    if (phase >= 1.f)
      phase -= 1.f;
    float pm = phase_mod ? phase_mod.next() : 0.f;
    return std::sin(two_pi * (prev + pm));
  }

private:
  Signal freq;
  Signal phase_mod;
  float phase = 0.f;
  uint64_t node_id;
};

inline Signal osc(Signal freq) {
  return Signal(std::make_shared<SineOscillator>(freq));
}

inline Signal osc(Signal freq, Signal phase_mod) {
  return Signal(std::make_shared<SineOscillator>(freq, phase_mod));
}

// ─── simple_fm
// ────────────────────────────────────────────────────────────────
//
// Carrier sine at freq, phase-modulated by a modulator sine at freq * fm_index.
// With fm_index = 1 the modulator runs at the same frequency as the carrier.

inline Signal simple_fm(Signal freq, Signal fm_index) {
  return osc(freq, osc(freq * fm_index));
}

}  // namespace automata::v2
