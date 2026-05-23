#pragma once

#include <stream.hpp>
#include <samplerate.hpp>

namespace automata {

class Clock {
  float bpm;
  float rate;  // subdivisions per whole note (4=quarter, 8=eighth, 16=sixteenth, …)

public:
  Clock(float bpm, float rate) : bpm(bpm), rate(rate) {}

  [[nodiscard]] Clock at(float new_rate) const { return Clock(bpm, new_rate); }

  [[nodiscard]] Clock operator*(float factor) const {
    return Clock(bpm, rate * factor);
  }
  [[nodiscard]] Clock operator/(float factor) const {
    return Clock(bpm, rate / factor);
  }

  // Returns a ramp 0→1 per step. Power-user path.
  [[nodiscard]] Stream ramp() const {
    return phasor(bpm * rate / (240.f * SampleRate));
  }

  // Returns a trigger stream: 1.f for one sample at the start of each step.
  [[nodiscard]] Stream trigger() const {
    return Stream([ramp_stream = ramp(), prev = 1.f]() mutable -> float {
      float current = ramp_stream.next();
      bool fire = (current < prev);
      prev = current;
      return fire ? 1.f : 0.f;
    });
  }
};

inline Stream metro(float bpm) { return Clock(bpm, 4.f).trigger(); }

}  // namespace automata
