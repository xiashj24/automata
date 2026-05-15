#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>

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

struct SvfState {
  float s1 = 0.f, s2 = 0.f;
};

namespace detail {

enum class SvfMode { Lowpass, Bandpass, Highpass };

// w = normalized frequency (f/fs). res in [0, 1): 0 = critically damped
// (Q=0.5). Ref: The Art of VA Filter Design, Zavalishin.
template <SvfMode Mode>
[[nodiscard]] inline Stream svf_impl(Stream x,
                                     Stream w,
                                     Stream res,
                                     SvfState& state) {
  return Stream([x = std::move(x), w = std::move(w), res = std::move(res),
                 &state]() mutable {
    constexpr float pi = std::numbers::pi_v<float>;
    const float wn = std::clamp(w.next(), 0.f, 0.4999f);
    const float rn = std::clamp(res.next(), 0.f, 0.9999f);
    const float g = std::tan(pi * wn);
    const float r2 = 2.f * (1.f - rn);
    const float h = 1.f / (1.f + r2 * g + g * g);
    const float xn = x.next();
    const float hp = h * (xn - state.s1 * (g + r2) - state.s2);
    const float bp = hp * g + state.s1;
    state.s1 = hp * g + bp;
    const float lp = bp * g + state.s2;
    state.s2 = bp * g + lp;
    if constexpr (Mode == SvfMode::Lowpass)
      return lp;
    else if constexpr (Mode == SvfMode::Bandpass)
      return bp;
    else
      return hp;
  });
}

}  // namespace detail

[[nodiscard]] inline Stream svf_lp(Stream x,
                                   Stream cutoff,
                                   Stream res,
                                   SvfState& state) {
  return detail::svf_impl<detail::SvfMode::Lowpass>(
      std::move(x), std::move(cutoff), std::move(res), state);
}
[[nodiscard]] inline Stream svf_bp(Stream x,
                                   Stream cutoff,
                                   Stream res,
                                   SvfState& state) {
  return detail::svf_impl<detail::SvfMode::Bandpass>(
      std::move(x), std::move(cutoff), std::move(res), state);
}
[[nodiscard]] inline Stream svf_hp(Stream x,
                                   Stream cutoff,
                                   Stream res,
                                   SvfState& state) {
  return detail::svf_impl<detail::SvfMode::Highpass>(
      std::move(x), std::move(cutoff), std::move(res), state);
}

}  // namespace automata
