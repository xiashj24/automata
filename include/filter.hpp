#pragma once
#include <algorithm>
#include <array>
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
  std::size_t sz =
      std::clamp(static_cast<std::size_t>(delay_s * SampleRate) + 1,
                 std::size_t{2}, MaxDelaySamples);
  struct State {
    std::array<float, MaxDelaySamples> buf{};
    std::size_t write_pos = 0;
  };
  return Stream([x, sz, st = State{}]() mutable -> float {
    std::size_t read_pos = (st.write_pos + 1u) % sz;
    float delayed = st.buf[read_pos];
    st.buf[st.write_pos] = x.next();
    st.write_pos = (st.write_pos + 1u) % sz;
    return delayed;
  });
}

// Non-interpolating feedback comb filter. Matches SC's CombN.ar(in, delay,
// decay). Feedback coefficient: exp(log(0.001) * delay / decay) — 60dB decay
// in `decay` seconds. max_delay_s is pre-allocated at construction; delay_time
// is clamped to that maximum at runtime.
[[nodiscard]] inline Stream comb_n(Stream x,
                                   Stream delay_time,
                                   Stream decay_time,
                                   float max_delay_s = 1.f) {
  std::size_t max_samples = std::max(
      std::size_t{2}, static_cast<std::size_t>(max_delay_s * SampleRate) + 1);
  std::vector<float> buf(max_samples, 0.f);
  return Stream([x, delay_time, decay_time, max_samples, buf = std::move(buf),
                 write_pos = std::size_t(0)]() mutable -> float {
    float dt = std::max(delay_time.next(), 0.f);
    float dcy = decay_time.next();
    std::size_t delay_samples =
        std::min(static_cast<std::size_t>(dt * SampleRate), max_samples - 1);
    float feedback = std::exp(std::log(0.001f) * dt / std::abs(dcy));
    if (dcy < 0.f)
      feedback = -feedback;
    std::size_t read_pos =
        (write_pos + max_samples - delay_samples) % max_samples;
    float yn = x.next() + feedback * buf[read_pos];
    buf[write_pos] = yn;
    write_pos = (write_pos + 1) % max_samples;
    return yn;
  });
}

// DC blocking one-pole high-pass filter (R ≈ 0.995, −3 dB at ~12 Hz @ 48 kHz).
[[nodiscard]] inline Stream dc_filter(Stream x) {
  return Stream([x, xz = 0.f, yz = 0.f]() mutable -> float {
    float xn = x.next();
    float yn = xn - xz + 0.995f * yz;
    xz = xn;
    yz = yn;
    return yn;
  });
}

// 4-pole Moog ladder filter (Stilson-Smith model, 24 dB/oct LP).
// res in [0, 1): 0 = clean, approaching 1 = self-oscillation.
[[nodiscard]] inline Stream moog_vcf(Stream x,
                                     Stream cutoff,
                                     Stream resonance) {
  struct State {
    float y1 = 0.f, y2 = 0.f, y3 = 0.f, y4 = 0.f;
    float ox = 0.f, o1 = 0.f, o2 = 0.f, o3 = 0.f;
  };
  return Stream([x, cutoff, resonance, st = State{}]() mutable -> float {
    constexpr float pi = std::numbers::pi_v<float>;
    float hz = std::clamp(cutoff.next(), 1.f, SampleRate * 0.4999f);
    float res = std::clamp(resonance.next(), 0.f, 0.9999f);
    float f = hz * 2.f / SampleRate;
    float p = f * (1.8f - 0.8f * f);
    float k = 2.f * std::sin(p * pi * 0.5f) - 1.f;
    float t1 = (1.f - p) * 1.386249f;
    float t2 = 12.f + t1 * t1;
    float r = res * (t2 + 6.f * t1) / (t2 - 6.f * t1);
    float xn = x.next() - r * st.y4;
    float y1 = xn * p + st.ox * p - k * st.y1;
    float y2 = y1 * p + st.o1 * p - k * st.y2;
    float y3 = y2 * p + st.o2 * p - k * st.y3;
    float y4 = y3 * p + st.o3 * p - k * st.y4;
    y4 -= (y4 * y4 * y4) / 6.f;
    st.ox = xn;
    st.o1 = y1;
    st.o2 = y2;
    st.o3 = y3;
    st.y1 = y1;
    st.y2 = y2;
    st.y3 = y3;
    st.y4 = y4;
    return y4;
  });
}

// Schroeder allpass delay: y[n] = -g·x[n] + x[n-D] + g·y[n-D].
// delay_s is fixed at construction; g is the feedback/absorption coefficient.
[[nodiscard]] inline Stream allpass_delay(Stream x,
                                          float delay_s,
                                          float g = 0.7f) {
  std::size_t d = std::clamp(static_cast<std::size_t>(delay_s * SampleRate),
                             std::size_t{1}, MaxDelaySamples - 1);
  struct State {
    std::array<float, MaxDelaySamples> xb{}, yb{};
    std::size_t pos = 0;
  };
  return Stream([x, g, d, st = State{}]() mutable -> float {
    float xn = x.next();
    float xd = st.xb[st.pos];
    float yd = st.yb[st.pos];
    float yn = -g * xn + xd + g * yd;
    st.xb[st.pos] = xn;
    st.yb[st.pos] = yn;
    st.pos = (st.pos + 1) % (d + 1);
    return yn;
  });
}

// Karplus-Strong string synthesis.
// Feedback delay of length SR/freq with a 2-point moving-average LP in the
// loop.
[[nodiscard]] inline Stream karplus_strong(Stream excitation,
                                           Stream freq_hz,
                                           float feedback = 0.98f) {
  struct State {
    std::array<float, 4096> buf{};
    std::size_t pos = 0;
    float prev = 0.f;
  };
  return Stream([excitation, freq_hz, feedback,
                 st = State{}]() mutable -> float {
    float hz = std::max(freq_hz.next(), 1.f);
    std::size_t d =
        std::min(static_cast<std::size_t>(SampleRate / hz), std::size_t{4095});
    std::size_t sz = d + 1;
    float delayed = st.buf[(st.pos + 1) % sz];
    float lp = 0.5f * (delayed + st.prev);
    st.prev = delayed;
    float yn = excitation.next() + feedback * lp;
    st.buf[st.pos] = yn;
    st.pos = (st.pos + 1) % sz;
    return yn;
  });
}

}  // namespace automata
