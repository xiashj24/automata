#pragma once

#include <algorithm>
#include <cmath>

#include <stream.hpp>

namespace automata {

[[nodiscard]] inline Stream clip(Stream x, float lo = -1.f, float hi = 1.f) {
  return Stream([x, lo, hi]() -> float { return std::clamp(x.next(), lo, hi); });
}

// Fold-back saturation: reflects x at lo and hi boundaries.
[[nodiscard]] inline Stream fold(Stream x, float lo = -1.f, float hi = 1.f) {
  float range = hi - lo;
  float period = 2.f * range;
  return Stream([x, lo, range, period]() -> float {
    float v = std::fmod(x.next() - lo, period);
    if (v < 0.f)
      v += period;
    if (v > range)
      v = period - v;
    return lo + v;
  });
}

[[nodiscard]] inline Stream abs(Stream x) {
  return Stream([x]() -> float { return std::abs(x.next()); });
}

// Apply std::sin sample-wise (input in radians).
[[nodiscard]] inline Stream sin(Stream x) {
  return Stream([x]() -> float { return std::sin(x.next()); });
}

// Apply std::cos sample-wise (input in radians).
[[nodiscard]] inline Stream cos(Stream x) {
  return Stream([x]() -> float { return std::cos(x.next()); });
}

[[nodiscard]] inline Stream round(Stream x) {
  return Stream([x]() -> float { return std::round(x.next()); });
}

// Linear-to-linear range mapping with clamped input.
[[nodiscard]] inline Stream lin_lin(Stream src,
                                     float in_lo, float in_hi,
                                     float out_lo, float out_hi) {
  float in_range  = in_hi - in_lo;
  float out_range = out_hi - out_lo;
  return Stream([src, in_lo, in_range, out_lo, out_range]() -> float {
    float t = std::clamp((src.next() - in_lo) / in_range, 0.f, 1.f);
    return out_lo + t * out_range;
  });
}

// x^exp per sample.
[[nodiscard]] inline Stream pow(Stream x, Stream exp) {
  return Stream([x, exp]() -> float { return std::pow(x.next(), exp.next()); });
}

// Modulo per sample: fmod(x, m).
[[nodiscard]] inline Stream mod(Stream x, Stream m) {
  return Stream([x, m]() -> float { return std::fmod(x.next(), m.next()); });
}

// Decibels → linear amplitude: 10^(db/20).
[[nodiscard]] inline Stream db_to_amp(Stream db) {
  return Stream([db]() -> float { return std::pow(10.f, db.next() * 0.05f); });
}

// Linear amplitude → decibels: 20·log₁₀(amp), floored at −200 dB.
[[nodiscard]] inline Stream amp_to_db(Stream amp) {
  return Stream([amp]() -> float {
    return 20.f * std::log10(std::max(amp.next(), 1e-10f));
  });
}

}  // namespace automata
