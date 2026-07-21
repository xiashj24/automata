#pragma once

#include "automata/config.hpp"
#include "automata/graph/make_node.hpp"
#include "automata/graph/param.hpp"
#include "automata/graph/tap.hpp"
#include "automata/kernel/arith.hpp"
#include "automata/kernel/envelopes.hpp"
#include "automata/kernel/oscillators.hpp"
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

}  // namespace automata
