#pragma once

#include <cmath>
#include <limits>

#include "automata/config.hpp"

// One-pole exponential smoother (Faust si.smooth): lowpasses a control
// signal so stepped changes glide instead of clicking.

namespace automata {

class Smooth {
public:
  // tau is the 1/e convergence time in seconds; <= 0 disables smoothing.
  void set_tau(float seconds) {
    if (seconds == tau_in_) {
      return;
    }
    tau_in_ = seconds;
    const float samples = seconds * SampleRateF;
    pole_ = samples > 0.f ? std::exp(-1.f / samples) : 0.f;
  }

  [[nodiscard]] float process(float in) {
    out_ = pole_ * out_ + (1.f - pole_) * in;
    return out_;
  }

  void reset() { *this = Smooth{}; }

private:
  static constexpr float Uncached = std::numeric_limits<float>::quiet_NaN();

  float out_ = 0.f;
  float pole_ = 0.f;
  float tau_in_ = Uncached;  // NaN: the first setter call always derives
};

}  // namespace automata
