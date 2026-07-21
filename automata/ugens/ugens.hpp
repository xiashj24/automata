#pragma once

#include <algorithm>
#include <cmath>
#include <initializer_list>

#include "automata/config.hpp"
#include "automata/graph/clock.hpp"
#include "automata/graph/fn.hpp"
#include "automata/graph/make_node.hpp"
#include "automata/graph/param.hpp"
#include "automata/graph/sequence.hpp"
#include "automata/graph/tap.hpp"
#include "automata/kernel/arith.hpp"
#include "automata/kernel/envelopes.hpp"
#include "automata/kernel/oscillators.hpp"
#include "automata/kernel/rhythm.hpp"
#include "automata/kernel/shapers.hpp"
#include "automata/kernel/smooth.hpp"
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
inline Signal operator>=(Signal a, Signal b) {
  return make_node<GreaterEqual>(a, b);
}

// Stateless shapers are fn nodes (ADR 0009) — no kernel class; the probe
// registers each name so patches using them rebind and don't pin.
[[nodiscard]] inline Signal frac(Signal in) {
  return fn("frac", [](float x) { return x - std::floor(x); }, in);
}

// [0,1] -> [-1,1] and back.
[[nodiscard]] inline Signal bipolar(Signal in) {
  return fn("bipolar", [](float x) { return 2.f * x - 1.f; }, in);
}
[[nodiscard]] inline Signal unipolar(Signal in) {
  return fn("unipolar", [](float x) { return 0.5f * x + 0.5f; }, in);
}

// Clamp to [0, 1] / [-1, 1] (min/max, not branches: a vmin/vmax pair).
[[nodiscard]] inline Signal clip(Signal in) {
  return fn(
      "clip", [](float x) { return std::min(std::max(x, 0.f), 1.f); }, in);
}
[[nodiscard]] inline Signal hard_clip(Signal in) {
  return fn(
      "hard_clip", [](float x) { return std::min(std::max(x, -1.f), 1.f); },
      in);
}

// Bends a 0..1 ramp exponentially: k > 0 bows it down, k < 0 up, 0 is
// straight.
[[nodiscard]] inline Signal curve(Signal in, Signal k) {
  return fn("curve", &exp_curve, in, k);
}

[[nodiscard]] inline Signal phasor(Signal freq_hz) {
  return make_node<Phasor>().control(&Phasor::set_freq,
                                     freq_hz * detail::InvSampleRate);
}

[[nodiscard]] inline Signal sine(Signal freq_hz) {
  return fn("sine", &sine_from_phase, phasor(freq_hz));
}

// Phase-modulation form: phase is in cycles, added after the accumulator, so
// sine(c, sine(m) * index) is FM in the classic phase-modulation sense.
[[nodiscard]] inline Signal sine(Signal freq_hz, Signal phase) {
  return fn("sine", &sine_from_phase, phasor(freq_hz) + phase);
}

// Carrier phase-modulated by a sine at freq * fm_index — classic 2-op FM.
[[nodiscard]] inline Signal simple_fm(Signal freq_hz, Signal fm_index) {
  return sine(freq_hz, sine(freq_hz * fm_index));
}

[[nodiscard]] inline Signal saw(Signal freq_hz) {
  return fn("saw", &saw_from_phase, phasor(freq_hz));
}

// Shapes any 0..1 ramp — a clock cycle, a phasor — into a naive triangle;
// aliases at audio rate, where triangle() is the band-limited oscillator.
[[nodiscard]] inline Signal tri(Signal phase) {
  return fn("tri", &tri_from_phase, phase);
}

[[nodiscard]] inline Signal triangle(Signal freq_hz) {
  return fn("triangle", &tri_from_phase_aa, phasor(freq_hz),
            freq_hz * detail::InvSampleRate);
}

[[nodiscard]] inline Signal metro(Signal freq_hz) {
  return make_node<Metro>().control(&Metro::set_freq,
                                    freq_hz * detail::InvSampleRate);
}

// High for the first len seconds of each period-second cycle — a
// free-running gate source for the envelopes (Faust pulsen).
[[nodiscard]] inline Signal pulsen(Signal len_s, Signal period_s) {
  return phasor(1.f / period_s) < (len_s / period_s);
}

[[nodiscard]] inline Signal ar(Signal trig, Signal attack_s, Signal release_s) {
  constexpr float Sr = static_cast<float>(SampleRate);
  return make_node<Ar>(trig)
      .control(&Ar::set_attack, attack_s * Sr)
      .control(&Ar::set_release, release_s * Sr);
}

// Exponential gate follower: rises toward 1 while gate is high, falls
// toward 0 while low; times are T60s in seconds.
[[nodiscard]] inline Signal are(Signal gate,
                                Signal attack_s,
                                Signal release_s) {
  constexpr float Sr = static_cast<float>(SampleRate);
  return make_node<Are>(gate)
      .control(&Are::set_attack, attack_s * Sr)
      .control(&Are::set_release, release_s * Sr);
}

// One-pole lowpass on a control signal, so stepped changes glide instead of
// clicking; tau is the 1/e convergence time.
[[nodiscard]] inline Signal smooth(Signal in, Signal tau_s = 0.02f) {
  constexpr float Sr = static_cast<float>(SampleRate);
  return make_node<Smooth>(in).control(&Smooth::set_tau, tau_s * Sr);
}

// Gain in decibels, converted at describe time — the lifted Const glides on
// a value patch, so a live gain edit is already click-free.
[[nodiscard]] inline Signal gain(Signal in, float db) {
  return in * std::pow(10.f, db / 20.f);
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
  return fn("soft_clip", [](float x) { return std::tanh(x); }, in);
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
        fn("swing", &swing_shape, detail::clock_ramp(beats_ * 2.f), amount),
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

// Advances through `steps` on each rising edge of `trig` — the first
// trigger plays step 0 — wrapping at the end and holding between edges:
// the melody companion to seq, driven by euclid() or any gate. Editing a
// step is a value patch; changing the count is structural and restarts.
[[nodiscard]] inline Signal step(Signal trig,
                                 std::initializer_list<float> steps) {
  return detail::step_values(trig, {steps.begin(), steps.size()});
}

// Sample-and-hold: captures `in` at each rising edge of `trig` and holds
// it — e.g. latch(mouse_y(), beat().trig()) quantizes a gesture to the grid.
[[nodiscard]] inline Signal latch(Signal in, Signal trig) {
  return make_node<Latch>(in, trig);
}

}  // namespace automata
