#pragma once

#include <cmath>

// Stateless shaper math: plain functions the fn factories wrap (ADR 0009).
// Each maps a phase ramp (or any float) with no memory, so a shape edit
// never has state to preserve.

namespace automata {

[[nodiscard]] inline float sine_from_phase(float phase) {
  constexpr float TwoPi = 6.28318530717958647692f;
  return std::sin(TwoPi * phase);
}

[[nodiscard]] inline float saw_from_phase(float phase) {
  return 2.f * phase - 1.f;
}

// PolyBLAMP correction for a slope discontinuity at phase 0; dt is the
// normalized phase increment, scale the one-sided slope change at the
// corner (cycfi::q's poly_blamp).
[[nodiscard]] inline float poly_blamp(float phase, float dt, float scale) {
  if (phase < dt) {
    const float t = phase / dt - 1.f;
    return -scale / 3.f * dt * t * t * t;
  }
  if (phase > 1.f - dt) {
    const float t = (phase - 1.f) / dt + 1.f;
    return scale / 3.f * dt * t * t * t;
  }
  return 0.f;
}

// Naive (aliasing) triangle: +1 at phase 0, -1 at 0.5; wraps any input.
[[nodiscard]] inline float tri_from_phase(float phase) {
  const float wrapped = phase - std::floor(phase);
  return 2.f * std::abs(2.f * wrapped - 1.f) - 1.f;
}

// Antialiased triangle: the naive shape with a polyBLAMP rounding each
// corner. dt is the normalized phase increment (freq / sample rate).
[[nodiscard]] inline float tri_from_phase_aa(float phase, float dt) {
  const float wrapped = phase - std::floor(phase);
  const float peak = tri_from_phase(wrapped) - poly_blamp(wrapped, dt, 4.f);
  float valley_phase = wrapped + 0.5f;
  valley_phase -= std::floor(valley_phase);
  return peak + poly_blamp(valley_phase, dt, 4.f);
}

// Warps a pair phase (one ramp covering two cycles) into two sub-cycle
// ramps whose boundary sits at 0.5 + amount/2: the first stretches, the
// second compresses, and everything derived downstream lands swung.
[[nodiscard]] inline float swing_shape(float phase, float amount) {
  constexpr float Max = 0.9f;
  const float a = amount < -Max ? -Max : (amount > Max ? Max : amount);
  const float split = 0.5f + 0.5f * a;
  return phase < split ? phase / split : (phase - split) / (1.f - split);
}

// Exponential curve through (0,0) and (1,1): k > 0 bows below the identity,
// k < 0 above, and k -> 0 is the identity (a removable 0/0 singularity).
[[nodiscard]] inline float exp_curve(float x, float k) {
  if (std::abs(k) < 1e-6f) {
    return x;
  }
  return std::expm1(k * x) / std::expm1(k);
}

}  // namespace automata
