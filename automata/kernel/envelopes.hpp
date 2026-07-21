#pragma once

#include <cmath>

// Envelope kernels. Times are in samples; factories convert from seconds.

namespace automata {

// Linear attack-release envelope, retriggered by a rising edge on trig.
class Ar {
public:
  void set_attack(float samples) { attack_ = samples > 1.f ? samples : 1.f; }
  void set_release(float samples) { release_ = samples > 1.f ? samples : 1.f; }

  [[nodiscard]] float process(float trig) {
    if (trig > 0.5f && last_trig_ <= 0.5f) {
      stage_ = Stage::Attack;
    }
    last_trig_ = trig;

    switch (stage_) {
      case Stage::Idle:
        break;
      case Stage::Attack:
        env_ += 1.f / attack_;
        if (env_ >= 1.f) {
          env_ = 1.f;
          stage_ = Stage::Release;
        }
        break;
      case Stage::Release:
        env_ -= 1.f / release_;
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

  float env_ = 0.f;
  float last_trig_ = 0.f;
  float attack_ = 1.f;
  float release_ = 1.f;
  Stage stage_ = Stage::Idle;
};

// Exponential attack-release (Faust envelopes.lib `are`): a one-pole chasing
// 1 while the gate is high, 0 while it is low. Times are T60s — the level
// gets within 60 dB of the target after that many samples.
class Are {
public:
  void set_attack(float t60_samples) { attack_pole_ = pole(t60_samples); }
  void set_release(float t60_samples) { release_pole_ = pole(t60_samples); }

  [[nodiscard]] float process(float gate) {
    const bool high = gate > 0.f;
    const float p = high ? attack_pole_ : release_pole_;
    env_ = p * env_ + (1.f - p) * (high ? 1.f : 0.f);
    return env_;
  }

  void reset() { *this = Are{}; }

private:
  [[nodiscard]] static float pole(float t60_samples) {
    return std::exp(-6.91f / (t60_samples > 1.f ? t60_samples : 1.f));
  }

  float env_ = 0.f;
  float attack_pole_ = 0.f;
  float release_pole_ = 0.f;
};

}  // namespace automata
