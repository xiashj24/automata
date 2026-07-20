#pragma once

#include <cmath>

// Waveshaper kernels.

namespace automata {

class SoftClip {
public:
  [[nodiscard]] float process(float in) { return std::tanh(in); }

  void reset() {}
};

}  // namespace automata
