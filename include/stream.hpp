#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <numbers>
#include <span>

#include <samplerate.hpp>

namespace automata {

inline thread_local uint64_t current_tick = 0;

/**
 * @brief sequential stream of samples
 *
 * Copies share the same generator (ref-counted, O(1) copy). Within each
 * render sample, next() computes at most once — safe for multiple consumers.
 */
class Stream {
private:
  struct State {
    std::move_only_function<float()> gen;
    float cache_val = 0.f;
    uint64_t cache_tick = ~0ull;
  };
  std::shared_ptr<State> state;

public:
  explicit Stream(std::move_only_function<float()> gen)
      : state(std::make_shared<State>(State{std::move(gen)})) {}
  // cppcheck-suppress noExplicitConstructor
  Stream(float v) : Stream([v]() -> float { return v; }) {}
  // cppcheck-suppress noExplicitConstructor
  Stream(float* ptr) : Stream([ptr]() -> float { return *ptr; }) {}
  Stream(std::nullptr_t) = delete;

  float next() const {
    if (current_tick == 0)
      return state->gen();
    if (state->cache_tick == current_tick)
      return state->cache_val;
    state->cache_val = state->gen();
    state->cache_tick = current_tick;
    return state->cache_val;
  }

  // void render(std::span<float> buf) const {
  //   for (auto& s : buf) {
  //     ++current_tick;
  //     s = next();
  //   }
  // }

  [[nodiscard]] Stream bipolar() const {
    return Stream([src = *this]() { return src.next() * 2.f - 1.f; });
  }
  [[nodiscard]] Stream unipolar() const {
    return Stream([src = *this]() { return (src.next() + 1.f) * 0.5f; });
  }
  [[nodiscard]] Stream exp2() const {
    return Stream([src = *this]() { return std::exp2(src.next()); });
  }
  [[nodiscard]] Stream vpow(float n) const {
    return Stream([src = *this, n]() { return std::pow(src.next(), n); });
  }
  [[nodiscard]] Stream gain(float db) const {
    float gain = std::pow(10.0f, db / 20.0f);
    return Stream([src = *this, gain]() { return src.next() * gain; });
  }
};

// stream-stream arithmetic
[[nodiscard]] inline Stream operator+(Stream lhs, Stream rhs) {
  return Stream([lhs, rhs]() { return lhs.next() + rhs.next(); });
}
[[nodiscard]] inline Stream operator-(Stream lhs, Stream rhs) {
  return Stream([lhs, rhs]() { return lhs.next() - rhs.next(); });
}
[[nodiscard]] inline Stream operator*(Stream lhs, Stream rhs) {
  return Stream([lhs, rhs]() { return lhs.next() * rhs.next(); });
}
[[nodiscard]] inline Stream operator/(Stream lhs, Stream rhs) {
  return Stream([lhs, rhs]() { return lhs.next() / rhs.next(); });
}

// stream-scalar arithmetic (scalar promotes to Stream via implicit constructor)
[[nodiscard]] inline Stream operator+(Stream lhs, float rhs) {
  return Stream([lhs, rhs]() { return lhs.next() + rhs; });
}
[[nodiscard]] inline Stream operator-(Stream lhs, float rhs) {
  return Stream([lhs, rhs]() { return lhs.next() - rhs; });
}
[[nodiscard]] inline Stream operator*(Stream lhs, float rhs) {
  return Stream([lhs, rhs]() { return lhs.next() * rhs; });
}
[[nodiscard]] inline Stream operator/(Stream lhs, float rhs) {
  return Stream([lhs, rhs]() { return lhs.next() / rhs; });
}
[[nodiscard]] inline Stream operator+(float lhs, Stream rhs) {
  return rhs + lhs;
}
[[nodiscard]] inline Stream operator-(float lhs, Stream rhs) {
  return Stream(lhs) - rhs;
}
[[nodiscard]] inline Stream operator*(float lhs, Stream rhs) {
  return rhs * lhs;
}
[[nodiscard]] inline Stream operator/(float lhs, Stream rhs) {
  return Stream(lhs) / rhs;
}

[[nodiscard]] inline Stream hold(Stream src, Stream trigger) {
  return Stream([src, trigger, value = 0.f]() mutable -> float {
    if (trigger.next() > 0.5f)
      value = src.next();
    return value;
  });
}

[[nodiscard]] inline Stream phasor(Stream w) {
  return Stream([w, phase = 0.f]() mutable {
    float out = phase;
    phase = std::fmod(phase + w.next(), 1.f);
    return out;
  });
}

// TODO: make a function called osc_sin take takes phase in 0-1

[[nodiscard]] inline Stream osc(Stream hz, Stream phase_mod = 0.f) {
  constexpr float two_pi = 2.f * std::numbers::pi_v<float>;
  return Stream([phase = phasor(hz / SampleRate), phase_mod]() {
    return std::sin(two_pi * (phase.next() + phase_mod.next()));
  });
}

[[nodiscard]] inline Stream saw(Stream hz) {
  return phasor(hz / SampleRate).bipolar();
}


// Soft saturator: x / (1 + |x|), maps R -> (-1, 1).
[[nodiscard]] inline Stream distort(Stream x) {
  return Stream([x]() -> float {
    float xn = x.next();
    return xn / (1.f + std::abs(xn));
  });
}

// Hyperbolic-tangent soft clipper, maps R -> (-1, 1).
[[nodiscard]] inline Stream tanh(Stream x) {
  return Stream([x]() -> float { return std::tanh(x.next()); });
}

// Triangle oscillator: linear ramp -1→+1→-1 per cycle.
[[nodiscard]] inline Stream tri(Stream hz) {
  return Stream([ph = phasor(hz / SampleRate)]() -> float {
    float p = ph.next();
    return p < 0.5f ? 4.f * p - 1.f : 3.f - 4.f * p;
  });
}

// Square oscillator: +1 for first `width` of period, -1 otherwise.
[[nodiscard]] inline Stream sqr(Stream hz, Stream width = 0.5f) {
  return Stream([ph = phasor(hz / SampleRate), width]() -> float {
    return ph.next() < width.next() ? 1.f : -1.f;
  });
}

// Random log-uniform value from [lo, hi] resampled on each trigger.
[[nodiscard]] inline Stream rand_exp(float lo,
                                     float hi,
                                     Stream trigger,
                                     uint32_t seed = 1u) {
  constexpr uint32_t lcg_mul = 196314165u;
  constexpr uint32_t lcg_add = 907633515u;
  float log_lo = std::log(lo);
  float log_range = std::log(hi / lo);
  return Stream([trigger, log_lo, log_range, state = seed,
                 value = lo]() mutable -> float {
    if (trigger.next() > 0.5f) {
      state = state * lcg_mul + lcg_add;
      float t = static_cast<float>(state) / static_cast<float>(UINT32_MAX);
      value = std::exp(log_lo + t * log_range);
    }
    return value;
  });
}

// Map [in_lo, in_hi] linearly to [out_lo, out_hi] on a log scale.
[[nodiscard]] inline Stream lin_exp(Stream src,
                                    float in_lo,
                                    float in_hi,
                                    float out_lo,
                                    float out_hi) {
  float log_lo = std::log(out_lo);
  float log_range = std::log(out_hi / out_lo);
  float in_range = in_hi - in_lo;
  return Stream([src, in_lo, in_range, log_lo, log_range]() -> float {
    float t = std::clamp((src.next() - in_lo) / in_range, 0.f, 1.f);
    return std::exp(log_lo + t * log_range);
  });
}

// Wrap values into [lo, hi) with modular arithmetic.
[[nodiscard]] inline Stream wrap(Stream src, float lo, float hi) {
  float range = hi - lo;
  return Stream([src, lo, range]() -> float {
    float v = std::fmod(src.next() - lo, range);
    if (v < 0.f)
      v += range;
    return lo + v;
  });
}

// Flip-flop: output alternates between 0 and 1 on each rising edge of trigger.
[[nodiscard]] inline Stream toggle(Stream trigger) {
  return Stream([trigger, state = 0.f, prev = 0.f]() mutable -> float {
    float curr = trigger.next();
    if (prev < 0.5f && curr >= 0.5f)
      state = state > 0.5f ? 0.f : 1.f;
    prev = curr;
    return state;
  });
}

// Counter: increments on each rising edge of trigger, resets to 0 on rising edge of reset.
[[nodiscard]] inline Stream count(Stream trigger, Stream reset = 0.f) {
  return Stream([trigger, reset, state = 0.f, prev_trig = 0.f,
                 prev_reset = 0.f]() mutable -> float {
    float curr_trig = trigger.next();
    float curr_reset = reset.next();
    if (prev_reset < 0.5f && curr_reset >= 0.5f)
      state = 0.f;
    else if (prev_trig < 0.5f && curr_trig >= 0.5f)
      state += 1.f;
    prev_trig = curr_trig;
    prev_reset = curr_reset;
    return state;
  });
}

}  // namespace automata
