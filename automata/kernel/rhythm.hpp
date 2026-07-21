#pragma once

// Rhythm kernels (ADR 0008): triggers are wrap events of a phase ramp, and
// stateful consumers fire on a rising edge through zero — so gates and
// single-sample impulses both drive them. Any ramp works: transport cycles
// and free-running phasors alike.

namespace automata {

// Emits a single-sample 1 when the ramp drops by more than half a cycle —
// a threshold, so warped ramps can't false-trigger. prev starts at 1 so a
// fresh trig fires the downbeat at cycle start; transferred state keeps a
// hot-swap from re-firing it.
class ClockTrig {
public:
  [[nodiscard]] float process(float phase) {
    const float out = prev_ - phase > 0.5f ? 1.f : 0.f;
    prev_ = phase;
    return out;
  }

  void reset() { *this = ClockTrig{}; }

private:
  float prev_ = 1.f;
};

// Sample-and-hold: captures the input at each rising edge of trig and
// holds it until the next.
class Latch {
public:
  [[nodiscard]] float process(float in, float trig) {
    if (prev_ <= 0.f && trig > 0.f) {
      held_ = in;
    }
    prev_ = trig;
    return held_;
  }

  void reset() { *this = Latch{}; }

private:
  float prev_ = 0.f;
  float held_ = 0.f;
};

}  // namespace automata
