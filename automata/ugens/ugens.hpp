#pragma once

#include <initializer_list>

#include "automata/config.hpp"
#include "automata/graph/clock.hpp"
#include "automata/graph/make_node.hpp"
#include "automata/graph/param.hpp"
#include "automata/graph/sequence.hpp"
#include "automata/graph/tap.hpp"
#include "automata/kernel/arith.hpp"
#include "automata/kernel/envelopes.hpp"
#include "automata/kernel/oscillators.hpp"
#include "automata/kernel/rhythm.hpp"
#include "automata/kernel/shapers.hpp"
#include "automata/kernel/svf.hpp"

// The UGen vocabulary: one-statement factories (ADR 0005). Frequencies in Hz
// and times in seconds convert here to the kernels' normalized units.

namespace automata {

namespace detail {
inline constexpr float InvSampleRate = 1.f / static_cast<float>(SampleRate);
}

inline Signal operator+(Signal a, Signal b) {
  return make_node<Add>(a, b);
}
inline Signal operator-(Signal a, Signal b) {
  return make_node<Sub>(a, b);
}
inline Signal operator*(Signal a, Signal b) {
  return make_node<Mul>(a, b);
}
inline Signal operator/(Signal a, Signal b) {
  return make_node<Div>(a, b);
}
inline Signal operator-(Signal a) {
  return make_node<Neg>(a);
}
inline Signal operator<(Signal a, Signal b) {
  return make_node<Less>(a, b);
}

[[nodiscard]] inline Signal frac(Signal in) {
  return make_node<Frac>(in);
}

[[nodiscard]] inline Signal phasor(Signal freq_hz) {
  return make_node<Phasor>().control(&Phasor::set_freq,
                                     freq_hz * detail::InvSampleRate);
}

[[nodiscard]] inline Signal sine(Signal freq_hz) {
  return make_node<SineShaper>(phasor(freq_hz));
}

// Phase-modulation form: phase is in cycles, added after the accumulator, so
// sine(c, sine(m) * index) is FM in the classic phase-modulation sense.
[[nodiscard]] inline Signal sine(Signal freq_hz, Signal phase) {
  return make_node<SineShaper>(phasor(freq_hz) + phase);
}

[[nodiscard]] inline Signal saw(Signal freq_hz) {
  return make_node<SawShaper>(phasor(freq_hz));
}

[[nodiscard]] inline Signal metro(Signal freq_hz) {
  return make_node<Metro>().control(&Metro::set_freq,
                                    freq_hz * detail::InvSampleRate);
}

[[nodiscard]] inline Signal ar(Signal trig, Signal attack_s, Signal release_s) {
  constexpr float Sr = static_cast<float>(SampleRate);
  return make_node<Ar>(trig)
      .control(&Ar::set_attack, attack_s * Sr)
      .control(&Ar::set_release, release_s * Sr);
}

[[nodiscard]] inline Signal svf_lp(Signal in, Signal cutoff_hz, Signal q) {
  return make_node<Svf>(in)
      .control(&Svf::update_coeffs, cutoff_hz * detail::InvSampleRate, q)
      .output(&Svf::Out::lp);
}

[[nodiscard]] inline Signal svf_bp(Signal in, Signal cutoff_hz, Signal q) {
  return make_node<Svf>(in)
      .control(&Svf::update_coeffs, cutoff_hz * detail::InvSampleRate, q)
      .output(&Svf::Out::bp);
}

[[nodiscard]] inline Signal svf_hp(Signal in, Signal cutoff_hz, Signal q) {
  return make_node<Svf>(in)
      .control(&Svf::update_coeffs, cutoff_hz * detail::InvSampleRate, q)
      .output(&Svf::Out::hp);
}

[[nodiscard]] inline Signal soft_clip(Signal in) {
  return make_node<SoftClip>(in);
}

// The pointer as a control surface: 0..1 across the virtual desktop, y = 1
// at the top. The host polls the cursor into these slots.
[[nodiscard]] inline Signal mouse_x() {
  return param("mouse/x", 0.5f);
}
[[nodiscard]] inline Signal mouse_y() {
  return param("mouse/y", 0.5f);
}

// A phase ramp with its cycle length in beats — the rhythm surface (ADR
// 0008). Describe-time sugar only: every method compiles to ordinary
// nodes. Rate algebra rebuilds on the transport grid, so it needs a
// transport-derived clock (beats > 0, asserted); any other ramp can still
// be wrapped for trig/gate.
class Clock {
public:
  explicit Clock(Signal ramp, float beats = 0.f) : ramp_(ramp), beats_(beats) {}

  [[nodiscard]] Signal ramp() const { return ramp_; }
  [[nodiscard]] float beats() const { return beats_; }

  // A single-sample impulse at each wrap; a fresh trig fires the downbeat.
  [[nodiscard]] Signal trig() const { return make_node<ClockTrig>(ramp_); }

  // High for the first `width` of each cycle (0..1).
  [[nodiscard]] Signal gate(Signal width) const { return ramp_ < width; }

  // n× faster / slower, phase-locked to the transport grid.
  [[nodiscard]] Clock operator*(float n) const {
    atm_assert(beats_ > 0.f && n > 0.f);
    return Clock(detail::clock_ramp(beats_ / n), beats_ / n);
  }
  [[nodiscard]] Clock operator/(float n) const {
    atm_assert(beats_ > 0.f && n > 0.f);
    return Clock(detail::clock_ramp(beats_ * n), beats_ * n);
  }

  // Rotates by `offset` cycles (0.5 = the offbeat).
  [[nodiscard]] Clock operator>>(Signal offset) const {
    return Clock(frac(ramp_ + offset), beats_);
  }

  // Alternating long/short cycles: amount 0 is straight, 0.5 delays every
  // second onset by a quarter cycle. Warps a transport-derived pair phase,
  // so everything downstream lands swung.
  [[nodiscard]] Clock swing(Signal amount) const {
    atm_assert(beats_ > 0.f);
    return Clock(
        make_node<SwingShaper>(detail::clock_ramp(beats_ * 2.f), amount),
        beats_);
  }

private:
  Signal ramp_;
  float beats_;
};

[[nodiscard]] inline Clock cycle(float beats) {
  atm_assert(beats > 0.f);
  return Clock(detail::clock_ramp(beats), beats);
}
[[nodiscard]] inline Clock beat() {
  return cycle(1.f);
}
[[nodiscard]] inline Clock bar() {
  return cycle(4.f);
}

// Steps through `steps` over one cycle of `clk`, holding each value; the
// position is derived from phase, so it survives any edit. Editing a step
// is a value patch; changing the count is structural.
[[nodiscard]] inline Signal seq(Clock clk, std::initializer_list<float> steps) {
  return detail::seq_steps(clk.ramp(), {steps.begin(), steps.size()});
}

// Euclidean triggers: `hits` onsets spread over `steps` steps per cycle of
// `clk`, rotated by `rotate` steps. All three are patchable values.
[[nodiscard]] inline Signal euclid(Clock clk,
                                   float hits,
                                   float steps,
                                   float rotate = 0.f) {
  return detail::euclid_trig(clk.ramp(), hits, steps, rotate);
}

}  // namespace automata
