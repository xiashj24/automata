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

// Gated ADSR envelope. Gate high → attack → decay → sustain; gate low → release.
// sustain_level is the amplitude held during sustain (0 = silence, 1 = full).
[[nodiscard]] inline Stream adsr(float attack_s, float decay_s,
                                  float sustain_level, float release_s,
                                  Stream gate) {
  float att = 1.f / std::max(attack_s * SampleRate, 1.f);
  float dec = (1.f - sustain_level) / std::max(decay_s * SampleRate, 1.f);
  return Stream([gate, att, dec, sus = sustain_level, rel_s = release_s,
                 lvl = 0.f, prev = 0.f, ph = 0, rr = 0.f]() mutable -> float {
    float g = gate.next();
    bool on = g > 0.5f, was = prev > 0.5f;
    prev = g;
    if (!was && on) {
      ph = 1;
    } else if (was && !on && ph) {
      rr = lvl / std::max(rel_s * SampleRate, 1.f);
      ph = 4;
    }
    if (ph == 1) {
      lvl = std::min(lvl + att, 1.f);
      if (lvl >= 1.f) {
        if (!on) {
          rr = lvl / std::max(rel_s * SampleRate, 1.f);
          ph = 4;
        } else {
          ph = 2;
        }
      }
    } else if (ph == 2) {
      lvl = std::max(lvl - dec, sus);
      if (lvl <= sus)
        ph = 3;
    } else if (ph == 3) {
      lvl = sus;
    } else if (ph == 4) {
      lvl = std::max(lvl - rr, 0.f);
      if (lvl <= 0.f)
        ph = 0;
    }
    return lvl;
  });
}

// Linear ramp from `start` to `end` over `duration_s` seconds, triggered
// on each rising edge. Holds at `end` after completion.
[[nodiscard]] inline Stream line(float start, float end, float duration_s,
                                  Stream trigger) {
  int dur = std::max(1, static_cast<int>(duration_s * SampleRate));
  float step = (end - start) / static_cast<float>(dur);
  return Stream([trigger, start, end, step, lvl = start, phase = dur,
                 dur]() mutable -> float {
    if (trigger.next() > 0.5f) {
      lvl = start;
      phase = 0;
    }
    float out = lvl;
    if (phase < dur) {
      lvl = std::min(lvl + step, end);
      ++phase;
    }
    return out;
  });
}

// Rectangular (gate-like) envelope: outputs 1 for `duration_s` seconds after
// each trigger rising edge, then 0.
[[nodiscard]] inline Stream rect_env(float duration_s, Stream trigger) {
  int dur = std::max(1, static_cast<int>(duration_s * SampleRate));
  return Stream([trigger, dur, remaining = 0, prev = 0.f]() mutable -> float {
    float t = trigger.next();
    if (prev < 0.5f && t >= 0.5f)
      remaining = dur;
    prev = t;
    if (remaining > 0) {
      --remaining;
      return 1.f;
    }
    return 0.f;
  });
}

}  // namespace automata
