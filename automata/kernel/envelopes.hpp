#pragma once

#include <cmath>
#include <limits>

#include "automata/config.hpp"

// Envelope kernels. Times are in seconds, converted in the setters, which
// cache their inputs and derive only on change (ADR 0010).

namespace automata {

// Linear attack-release envelope, retriggered by a rising edge on trig.
class Ar {
public:
  void set_attack(float seconds) {
    if (seconds == attack_in_) {
      return;
    }
    attack_in_ = seconds;
    attack_inc_ = 1.f / samples_at_least_one(seconds);
  }
  void set_release(float seconds) {
    if (seconds == release_in_) {
      return;
    }
    release_in_ = seconds;
    release_inc_ = 1.f / samples_at_least_one(seconds);
  }

  [[nodiscard]] float process(float trig) {
    if (trig > 0.5f && last_trig_ <= 0.5f) {
      stage_ = Stage::Attack;
    }
    last_trig_ = trig;

    switch (stage_) {
      case Stage::Idle:
        break;
      case Stage::Attack:
        env_ += attack_inc_;
        if (env_ >= 1.f) {
          env_ = 1.f;
          stage_ = Stage::Release;
        }
        break;
      case Stage::Release:
        env_ -= release_inc_;
        if (env_ <= 0.f) {
          env_ = 0.f;
          stage_ = Stage::Idle;
        }
        break;
    }
    return env_;
  }

  void reset() { *this = Ar{}; }

private:
  enum class Stage { Idle, Attack, Release };

  [[nodiscard]] static float samples_at_least_one(float seconds) {
    const float samples = seconds * SampleRateF;
    return samples > 1.f ? samples : 1.f;
  }

  static constexpr float Uncached = std::numeric_limits<float>::quiet_NaN();

  float env_ = 0.f;
  float last_trig_ = 0.f;
  float attack_inc_ = 1.f;
  float release_inc_ = 1.f;
  float attack_in_ = Uncached;  // NaN: the first setter call always derives
  float release_in_ = Uncached;
  Stage stage_ = Stage::Idle;
};

// Exponential attack-release (Faust envelopes.lib `are`): a one-pole chasing
// 1 while the gate is high, 0 while it is low. Times are T60s — the level
// gets within 60 dB of the target after that long.
class Are {
public:
  void set_attack(float t60_seconds) {
    if (t60_seconds == attack_in_) {
      return;
    }
    attack_in_ = t60_seconds;
    attack_pole_ = pole(t60_seconds);
  }
  void set_release(float t60_seconds) {
    if (t60_seconds == release_in_) {
      return;
    }
    release_in_ = t60_seconds;
    release_pole_ = pole(t60_seconds);
  }

  [[nodiscard]] float process(float gate) {
    const bool high = gate > 0.f;
    const float p = high ? attack_pole_ : release_pole_;
    env_ = p * env_ + (1.f - p) * (high ? 1.f : 0.f);
    return env_;
  }

  void reset() { *this = Are{}; }

private:
  [[nodiscard]] static float pole(float t60_seconds) {
    const float samples = t60_seconds * SampleRateF;
    return std::exp(-6.91f / (samples > 1.f ? samples : 1.f));
  }

  static constexpr float Uncached = std::numeric_limits<float>::quiet_NaN();

  float env_ = 0.f;
  float attack_pole_ = 0.f;
  float release_pole_ = 0.f;
  float attack_in_ = Uncached;
  float release_in_ = Uncached;
};

}  // namespace automata
