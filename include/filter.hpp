#pragma once
#include <algorithm>

#include <stream.hpp>

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

/**
 * @brief Transposed Direct Form 2 biquad
 * @ref https://www.earlevel.com/main/2003/02/28/biquads/
 */
[[nodiscard]] inline Stream biquad(Stream x,
                                   BiquadCoeffs coeffs,
                                   BiquadState& state) {
  return Stream([x = std::move(x), coeffs, &state]() mutable {
    const float xn = x.next();
    const float yn = coeffs.a0 * xn + state.s1;
    state.s1 = coeffs.a1 * xn - coeffs.b1 * yn + state.s2;
    state.s2 = coeffs.a2 * xn - coeffs.b2 * yn;
    return yn;
  });
}

// rise/fall are max change per sample
[[nodiscard]] inline Stream slew(Stream x,
                                 Stream rise,
                                 Stream fall,
                                 float& yz) {
  return Stream([x = std::move(x), rise = std::move(rise),
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
[[nodiscard]] inline Stream lag(Stream x, Stream a, float& yz) {
  return Stream([x = std::move(x), a = std::move(a), &yz]() mutable {
    float ac = std::clamp(a.next(), 0.f, 1.f);
    float xn = x.next();
    float yn = (1.f - ac) * xn + ac * yz;
    yz = yn;
    return yn;
  });
}

}  // namespace automata
