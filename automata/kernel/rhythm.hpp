#pragma once

// Rhythm kernels (ADR 0008): triggers are wrap events of a phase ramp;
// swing is a stateless warp of a two-cycle "pair" phase. Both consume any
// ramp — transport cycles and free-running phasors alike.

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

// Warps a pair phase (one ramp covering two cycles) into two sub-cycle
// ramps whose boundary sits at 0.5 + amount/2: the first stretches, the
// second compresses, and everything derived downstream lands swung.
class SwingShaper {
public:
  [[nodiscard]] float process(float phase, float amount) {
    constexpr float Max = 0.9f;
    const float a = amount < -Max ? -Max : (amount > Max ? Max : amount);
    const float split = 0.5f + 0.5f * a;
    return phase < split ? phase / split : (phase - split) / (1.f - split);
  }

  void reset() {}
};

}  // namespace automata
