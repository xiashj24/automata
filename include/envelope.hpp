#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

#include <samplerate.hpp>
#include <stream.hpp>

namespace automata {

// Attack-Sustain-Release one-shot envelope.
// Ramps 0→1 over attack, holds 1 for sustain, ramps 1→0 over release.
// Retriggers from the start on each trigger pulse.
[[nodiscard]] inline Stream asr(float attack_s,
                                float sustain_s,
                                float release_s,
                                Stream trigger) {
  int attack_n = std::max(1, static_cast<int>(attack_s * SampleRate));
  int sustain_n = static_cast<int>(sustain_s * SampleRate);
  int release_n = std::max(1, static_cast<int>(release_s * SampleRate));
  int total = attack_n + sustain_n + release_n;
  return Stream([trigger, attack_n, sustain_n,
                 release_n, total, phase = total]() mutable -> float {
    if (trigger.next() > 0.5f)
      phase = 0;
    if (phase >= total)
      return 0.f;
    float level;
    if (phase < attack_n) {
      level = static_cast<float>(phase) / static_cast<float>(attack_n);
    } else if (phase < attack_n + sustain_n) {
      level = 1.f;
    } else {
      int rel = phase - attack_n - sustain_n;
      level = 1.f - static_cast<float>(rel) / static_cast<float>(release_n);
    }
    ++phase;
    return level;
  });
}

// Envelope follower: tracks the amplitude of x with separate attack and release rates.
// attack_hz / release_hz are tracking speeds in Hz (higher = faster response).
[[nodiscard]] inline Stream env_follow(Stream x, Stream attack_hz, Stream release_hz) {
  constexpr float two_pi = 2.f * std::numbers::pi_v<float>;
  return Stream([x, attack_hz, release_hz, state = 0.f]() mutable -> float {
    float abs_in = std::abs(x.next());
    float rate = abs_in > state ? attack_hz.next() : release_hz.next();
    float alpha = std::min(1.f, two_pi * rate / SampleRate);
    state += alpha * (abs_in - state);
    return state;
  });
}

}  // namespace automata
