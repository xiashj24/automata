#pragma once

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

}  // namespace automata
