#pragma once

#include <cmath>

// One-pole exponential smoother (Faust si.smooth): lowpasses a control
// signal so stepped changes glide instead of clicking.

namespace automata {

class Smooth {
public:
  // tau is the 1/e convergence time in samples; <= 0 disables smoothing.
  void set_tau(float samples) {
    pole_ = samples > 0.f ? std::exp(-1.f / samples) : 0.f;
  }

  [[nodiscard]] float process(float in) {
    out_ = pole_ * out_ + (1.f - pole_) * in;
    return out_;
  }

  void reset() { *this = Smooth{}; }

private:
  float out_ = 0.f;
  float pole_ = 0.f;
};

}  // namespace automata
