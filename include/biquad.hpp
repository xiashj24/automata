#pragma once
#include <algorithm>

#include <signal.hpp>

/**
 * @brief Transposed Direct Form 2 biquad
 * @ref https://www.earlevel.com/main/2003/02/28/biquads/
 */

namespace automata {

struct BiquadCoeffs {
  float a0 = 1.f, a1 = 0.f, a2 = 0.f;
  float b1 = 0.f, b2 = 0.f;
};

// TODO: make specialized version of the biquad coeffs
// e.g. DC blocker, one pole low pass, high pass, all pass
// refer to RBJ cookbook

struct BiquadState {
  float s1 = 0.f, s2 = 0.f;
};

// transposed direct form II — stateful, must evaluate sequentially
[[nodiscard]] inline Signal biquad(const Signal& x,
                                   const BiquadCoeffs& coeffs,
                                   BiquadState& state) {
  return Signal([x, coeffs, &state](int n) {
    float xn = x[n];
    float yn = coeffs.a0 * xn + state.s1;
    state.s1 = coeffs.a1 * xn - coeffs.b1 * yn + state.s2;
    state.s2 = coeffs.a2 * xn - coeffs.b2 * yn;
    return yn;
  });
}

// rise/fall are max change per sample — stateful, must evaluate sequentially
[[nodiscard]] inline Signal slew(const Signal& x,
                                 float rise,
                                 float fall,
                                 float& yz) {
  return Signal([x, rise, fall, &yz](int n) {
    float yn = std::clamp(x[n], yz - fall, yz + rise);
    yz = yn;
    return yn;
  });
}

}  // namespace automata
