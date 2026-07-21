#pragma once

#include <cmath>

// Waveshaper kernels.

namespace automata {

class SoftClip {
public:
  [[nodiscard]] float process(float in) { return std::tanh(in); }

  void reset() {}
};

// Fractional part, wrapping any value into [0, 1) — the phase-rotation
// shaper.
class Frac {
public:
  [[nodiscard]] float process(float in) { return in - std::floor(in); }

  void reset() {}
};

}  // namespace automata
