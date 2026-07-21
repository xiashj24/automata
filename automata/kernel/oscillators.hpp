#pragma once

#include <cmath>

// Oscillator kernels (ADR 0005): self-contained, trivially copyable, no
// engine types. Frequencies are normalized (cycles per sample); factories do
// the Hz conversion. Waveforms are stateless phase shapers (shapers.hpp)
// composed over Phasor, so the phase accumulator lives in one kernel and a
// waveform edit preserves it.

namespace automata {

class Phasor {
public:
  void set_freq(float cycles_per_sample) { w_ = cycles_per_sample; }

  [[nodiscard]] float process() {
    const float out = p_;
    p_ += w_;
    p_ -= std::floor(p_);
    return out;
  }

  void reset() { *this = Phasor{}; }

private:
  float p_ = 0.f;
  float w_ = 0.f;
};

// Single-sample impulse train; fires on its first sample so a downstream
// envelope starts at t = 0 rather than one period in.
class Metro {
public:
  void set_freq(float cycles_per_sample) { w_ = cycles_per_sample; }

  [[nodiscard]] float process() {
    float out = 0.f;
    if (p_ >= 1.f) {
      p_ -= std::floor(p_);
      out = 1.f;
    }
    p_ += w_;
    return out;
  }

  void reset() { *this = Metro{}; }

private:
  float p_ = 1.f;
  float w_ = 0.f;
};

}  // namespace automata
