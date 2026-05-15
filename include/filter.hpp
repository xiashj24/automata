#pragma once
#include <algorithm>

#include <sequential_signal.hpp>

/**
 * @brief Transposed Direct Form 2 biquad
 * @ref https://www.earlevel.com/main/2003/02/28/biquads/
 */

namespace automata {

// Coefficients are SequentialSignal so they can be modulated per sample.
// Pass a float or Signal where a constant is needed — both convert implicitly.
struct BiquadCoeffs {
  SequentialSignal a0 = 1.f, a1 = 0.f, a2 = 0.f;
  SequentialSignal b1 = 0.f, b2 = 0.f;
};

// TODO: make specialized version of the biquad coeffs
// e.g. DC blocker, one pole low pass, high pass, all pass
// refer to RBJ cookbook

struct BiquadState {
  float s1 = 0.f, s2 = 0.f;
};

// transposed direct form II
[[nodiscard]] inline SequentialSignal biquad(SequentialSignal x,
                                             BiquadCoeffs coeffs,
                                             BiquadState& state) {
  return SequentialSignal(
      [x = std::move(x), coeffs = std::move(coeffs), &state]() mutable {
        float xn = x.next();
        float a0 = coeffs.a0.next();
        float a1 = coeffs.a1.next();
        float a2 = coeffs.a2.next();
        float b1 = coeffs.b1.next();
        float b2 = coeffs.b2.next();
        float yn = a0 * xn + state.s1;
        state.s1 = a1 * xn - b1 * yn + state.s2;
        state.s2 = a2 * xn - b2 * yn;
        return yn;
      });
}

// rise/fall are max change per sample
[[nodiscard]] inline SequentialSignal slew(SequentialSignal x,
                                           SequentialSignal rise,
                                           SequentialSignal fall,
                                           float& yz) {
  return SequentialSignal([x = std::move(x), rise = std::move(rise),
                           fall = std::move(fall), &yz]() mutable {
    float xn = x.next();
    float rn = rise.next();
    float fn = fall.next();
    float yn = std::clamp(xn, yz - fn, yz + rn);
    yz = yn;
    return yn;
  });
}

// one pole lag filter
[[nodiscard]] inline SequentialSignal lag(SequentialSignal x,
                                          SequentialSignal a,
                                          float& yz) {
  return SequentialSignal([x = std::move(x), a = std::move(a), &yz]() mutable {
    float ac = std::clamp(a.next(), 0.f, 1.f);
    float xn = x.next();
    float yn = (1.f - ac) * xn + ac * yz;
    yz = yn;
    return yn;
  });
}

}  // namespace automata
