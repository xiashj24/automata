#pragma once

#include <cmath>

// State-variable filter, topology-preserving transform (Andrew Simper's
// trapezoidal SVF). One pass yields lp/bp/hp as siblings; the node's output
// selector picks one (ADR 0005).

namespace automata {

class Svf {
public:
  struct Out {
    float lp, bp, hp;
  };

  void update_coeffs(float cutoff_cycles_per_sample, float q) {
    constexpr float Pi = 3.14159265358979323846f;
    constexpr float MaxCutoff = 0.49f;  // keep tan() finite below Nyquist
    const float w = cutoff_cycles_per_sample < MaxCutoff
                        ? cutoff_cycles_per_sample
                        : MaxCutoff;
    g_ = std::tan(Pi * (w > 0.f ? w : 0.f));
    k_ = 1.f / (q > 0.01f ? q : 0.01f);
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
  float ic1_ = 0.f, ic2_ = 0.f;                               // state
  float g_ = 0.f, k_ = 1.f, a1_ = 0.f, a2_ = 0.f, a3_ = 0.f;  // coefficients
};

}  // namespace automata
