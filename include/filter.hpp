#pragma once
#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#include <samplerate.hpp>
#include <stream.hpp>

namespace automata {

struct BiquadCoeffs {
  float a0 = 1.f, a1 = 0.f, a2 = 0.f;
  float b1 = 0.f, b2 = 0.f;
};

// TODO: make specialized version of the biquad coeffs
// e.g. DC blocker, one pole low pass, high pass, all pass
// refer to RBJ cookbook

/**
 * @brief Transposed Direct Form 2 biquad
 * @ref https://www.earlevel.com/main/2003/02/28/biquads/
 */
[[nodiscard]] inline Stream biquad(Stream x, BiquadCoeffs coeffs) {
  return Stream([x, coeffs, s1 = 0.f, s2 = 0.f]() mutable {
    const float xn = x.next();
    const float yn = coeffs.a0 * xn + s1;
    s1 = coeffs.a1 * xn - coeffs.b1 * yn + s2;
    s2 = coeffs.a2 * xn - coeffs.b2 * yn;
    return yn;
  });
}

// rise/fall are max change per sample
[[nodiscard]] inline Stream slew(Stream x, Stream rise, Stream fall) {
  return Stream([x, rise, fall, yz = 0.f]() mutable {
    float xn = x.next();
    float rn = rise.next();
    float fn = fall.next();
    float yn = std::clamp(xn, yz - fn, yz + rn);
    yz = yn;
    return yn;
  });
}

// one pole lag filter
[[nodiscard]] inline Stream lag(Stream x, Stream a) {
  return Stream([x, a, yz = 0.f]() mutable {
    float ac = std::clamp(a.next(), 0.f, 1.f);
    float xn = x.next();
    float yn = (1.f - ac) * xn + ac * yz;
    yz = yn;
    return yn;
  });
}

// One-pole low-pass filter parameterized by cutoff frequency in Hz.
// Converts cutoff to the lag coefficient: a = 1 - 2π·fc/SR, then delegates to
// lag.
[[nodiscard]] inline Stream one_pole(Stream x, Stream cutoff_hz) {
  constexpr float two_pi = 2.f * std::numbers::pi_v<float>;
  return lag(x, Stream([cutoff_hz]() -> float {
               return 1.f -
                      std::min(1.f, two_pi * cutoff_hz.next() / SampleRate);
             }));
}

namespace detail {

enum class SvfMode { Lowpass, Bandpass, Highpass };

// cutoff in Hz. res in [0, 1): 0 = critically damped (Q=0.5).
// Ref: The Art of VA Filter Design, Zavalishin.
template <SvfMode Mode>
[[nodiscard]] inline Stream svf_impl(Stream x, Stream cutoff, Stream res) {
  return Stream([x, cutoff, res, s1 = 0.f, s2 = 0.f]() mutable {
    constexpr float pi = std::numbers::pi_v<float>;
    const float hz = std::clamp(cutoff.next(), 0.f, SampleRate * 0.4999f);
    const float rn = std::clamp(res.next(), 0.f, 0.9999f);
    const float g = std::tan(pi * hz / SampleRate);
    const float r2 = 2.f * (1.f - rn);
    const float h = 1.f / (1.f + r2 * g + g * g);
    const float xn = x.next();
    const float hp = h * (xn - s1 * (g + r2) - s2);
    const float bp = hp * g + s1;
    s1 = hp * g + bp;
    const float lp = bp * g + s2;
    s2 = bp * g + lp;
    if constexpr (Mode == SvfMode::Lowpass)
      return lp;
    else if constexpr (Mode == SvfMode::Bandpass)
      return bp *
             r2;  // normalize to unity peak gain (SVF BP peaks at Q = 1/r2)
    else
      return hp;
  });
}

}  // namespace detail

[[nodiscard]] inline Stream svf_lp(Stream x, Stream cutoff, Stream res) {
  return detail::svf_impl<detail::SvfMode::Lowpass>(x, cutoff, res);
}
[[nodiscard]] inline Stream svf_bp(Stream x, Stream cutoff, Stream res) {
  return detail::svf_impl<detail::SvfMode::Bandpass>(x, cutoff, res);
}
[[nodiscard]] inline Stream svf_hp(Stream x, Stream cutoff, Stream res) {
  return detail::svf_impl<detail::SvfMode::Highpass>(x, cutoff, res);
}

// Non-interpolating delay line with no feedback.
[[nodiscard]] inline Stream delay_n(Stream x, float delay_s) {
  std::size_t delay_samples = static_cast<std::size_t>(delay_s * SampleRate);
  std::vector<float> buf(delay_samples + 1, 0.f);
  return Stream(
      [x, buf = std::move(buf), write_pos = std::size_t(0)]() mutable -> float {
        std::size_t sz = buf.size();
        std::size_t read_pos = (write_pos + 1u) % sz;
        float delayed = buf[read_pos];
        buf[write_pos] = x.next();
        write_pos = (write_pos + 1u) % sz;
        return delayed;
      });
}

// Non-interpolating feedback comb filter. Matches SC's CombN.ar(in, delay,
// decay). Feedback coefficient: exp(log(0.001) * delay / decay) — 60dB decay in
// `decay` seconds. Buffer grows on first call and whenever delay_time
// increases. State is held behind a shared_ptr so copies of the Stream share
// the same buffer.
[[nodiscard]] inline Stream comb_n(Stream x,
                                   Stream delay_time,
                                   Stream decay_time) {
  struct State {
    std::vector<float> buf;
    std::size_t write_pos = 0;
  };
  return Stream([x, delay_time, decay_time,
                 state = std::make_shared<State>()]() mutable -> float {
    float dt = std::max(delay_time.next(), 0.f);
    float dcy = decay_time.next();
    std::size_t delay_samples = static_cast<std::size_t>(dt * SampleRate);
    std::size_t needed = delay_samples + 1;
    if (needed > state->buf.size())
      state->buf.resize(needed, 0.f);
    float feedback = std::exp(std::log(0.001f) * dt / std::abs(dcy));
    if (dcy < 0.f)
      feedback = -feedback;
    std::size_t sz = state->buf.size();
    std::size_t read_pos = (state->write_pos + sz - delay_samples) % sz;
    float yn = x.next() + feedback * state->buf[read_pos];
    state->buf[state->write_pos] = yn;
    state->write_pos = (state->write_pos + 1) % sz;
    return yn;
  });
}

}  // namespace automata
