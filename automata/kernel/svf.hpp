#pragma once

#include <cmath>
#include <limits>

#include "automata/config.hpp"

// State-variable filter, topology-preserving transform (Andrew Simper's
// trapezoidal SVF). One pass yields lp/bp/hp as siblings; the node's output
// selector picks one (ADR 0005). Cutoff is in Hz; resonance is normalized:
// 0 is critically damped, 1 self-oscillation, capped just below so the
// filter always loses energy.

namespace automata {

class Svf {
public:
  struct Out {
    float lp, bp, hp;
  };

  void update_coeffs(float cutoff_hz, float resonance) {
    if (cutoff_hz == cutoff_in_ && resonance == resonance_in_) {
      return;
    }
    cutoff_in_ = cutoff_hz;
    resonance_in_ = resonance;
    constexpr float Pi = 3.14159265358979323846f;
    constexpr float MaxCutoff = 0.49f;  // keep tan() finite below Nyquist
    constexpr float MaxResonance = 0.999f;
    const float w = cutoff_hz / SampleRateF;
    g_ = std::tan(Pi * (w < 0.f ? 0.f : (w > MaxCutoff ? MaxCutoff : w)));
    const float r = resonance < 0.f
                        ? 0.f
                        : (resonance > MaxResonance ? MaxResonance : resonance);
    k_ = 2.f - 2.f * r;
    a1_ = 1.f / (1.f + g_ * (g_ + k_));
    a2_ = g_ * a1_;
    a3_ = g_ * a2_;
  }

  [[nodiscard]] Out process(float in) {
    const float v3 = in - ic2_;
    const float v1 = a1_ * ic1_ + a2_ * v3;
    const float v2 = ic2_ + a2_ * ic1_ + a3_ * v3;
    ic1_ = 2.f * v1 - ic1_;
    ic2_ = 2.f * v2 - ic2_;
    return {.lp = v2, .bp = v1, .hp = in - k_ * v1 - v2};
  }

  void reset() { *this = Svf{}; }

private:
  static constexpr float Uncached = std::numeric_limits<float>::quiet_NaN();

  float ic1_ = 0.f, ic2_ = 0.f;                               // state
  float g_ = 0.f, k_ = 1.f, a1_ = 0.f, a2_ = 0.f, a3_ = 0.f;  // coefficients
  float cutoff_in_ = Uncached;  // NaN: the first call always derives
  float resonance_in_ = Uncached;
};

}  // namespace automata
