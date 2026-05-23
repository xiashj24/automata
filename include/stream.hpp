#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numbers>
#include <span>

#include <samplerate.hpp>
#include <signal.hpp>

namespace automata {

/**
 * @brief sequential stream of samples
 */
class Stream {
  using Func = std::function<float()>;
  Func gen;

public:
  explicit Stream(Func gen) : gen(std::move(gen)) {}
  // cppcheck-suppress noExplicitConstructor
  Stream(const Signal& sig, int start = 0)
      : gen([sig, n = start]() mutable { return sig[n++]; }) {}
  // cppcheck-suppress noExplicitConstructor
  Stream(float v) : gen([v]() { return v; }) {}
  // cppcheck-suppress noExplicitConstructor
  Stream(float* ptr) : gen([ptr]() { return *ptr; }) {}
  Stream(std::nullptr_t) = delete;

  float next() { return gen(); }

  void render(std::span<float> buf) {
    std::generate(buf.begin(), buf.end(), [this] { return next(); });
  }

  [[nodiscard]] Stream to_bipolar() {
    return Stream([g = std::move(gen)]() { return g() * 2.f - 1.f; });
  }
  [[nodiscard]] Stream to_unipolar() {
    return Stream([g = std::move(gen)]() { return (g() + 1.f) * 0.5f; });
  }
  [[nodiscard]] Stream exp2() {
    return Stream([g = std::move(gen)]() { return std::exp2(g()); });
  }
  [[nodiscard]] Stream vpow(float n) {
    return Stream([g = std::move(gen), n]() mutable { return std::pow(g(), n); });
  }
};

// stream-stream arithmetic
[[nodiscard]] inline Stream operator+(Stream lhs, Stream rhs) {
  return Stream([lhs = std::move(lhs), rhs = std::move(rhs)]() mutable {
    return lhs.next() + rhs.next();
  });
}
[[nodiscard]] inline Stream operator-(Stream lhs, Stream rhs) {
  return Stream([lhs = std::move(lhs), rhs = std::move(rhs)]() mutable {
    return lhs.next() - rhs.next();
  });
}
[[nodiscard]] inline Stream operator*(Stream lhs, Stream rhs) {
  return Stream([lhs = std::move(lhs), rhs = std::move(rhs)]() mutable {
    return lhs.next() * rhs.next();
  });
}
[[nodiscard]] inline Stream operator/(Stream lhs, Stream rhs) {
  return Stream([lhs = std::move(lhs), rhs = std::move(rhs)]() mutable {
    return lhs.next() / rhs.next();
  });
}

// stream-scalar arithmetic (scalar promotes to Stream via implicit constructor)
[[nodiscard]] inline Stream operator+(Stream lhs, float rhs) {
  return Stream(
      [lhs = std::move(lhs), rhs]() mutable { return lhs.next() + rhs; });
}
[[nodiscard]] inline Stream operator-(Stream lhs, float rhs) {
  return Stream(
      [lhs = std::move(lhs), rhs]() mutable { return lhs.next() - rhs; });
}
[[nodiscard]] inline Stream operator*(Stream lhs, float rhs) {
  return Stream(
      [lhs = std::move(lhs), rhs]() mutable { return lhs.next() * rhs; });
}
[[nodiscard]] inline Stream operator/(Stream lhs, float rhs) {
  return Stream(
      [lhs = std::move(lhs), rhs]() mutable { return lhs.next() / rhs; });
}
[[nodiscard]] inline Stream operator+(float lhs, Stream rhs) {
  return std::move(rhs) + lhs;
}
[[nodiscard]] inline Stream operator-(float lhs, Stream rhs) {
  return Stream(lhs) - std::move(rhs);
}
[[nodiscard]] inline Stream operator*(float lhs, Stream rhs) {
  return std::move(rhs) * lhs;
}
[[nodiscard]] inline Stream operator/(float lhs, Stream rhs) {
  return Stream(lhs) / std::move(rhs);
}

[[nodiscard]] inline Stream hold(Stream src, Stream trigger) {
  return Stream([src = std::move(src), trigger = std::move(trigger),
                 value = 0.f]() mutable -> float {
    if (trigger.next() > 0.5f)
      value = src.next();
    return value;
  });
}

[[nodiscard]] inline Stream phasor(Stream w) {
  return Stream([w = std::move(w), phase = 0.f]() mutable {
    float out = phase;
    phase = std::fmod(phase + w.next(), 1.f);
    return out;
  });
}

// TODO: make a function called osc_sin take takes phase in 0-1

[[nodiscard]] inline Stream osc(Stream hz, Stream phase_mod = 0.f) {
  constexpr float two_pi = 2.f * std::numbers::pi_v<float>;
  return Stream([phase = phasor(std::move(hz) / SampleRate),
                 phase_mod = std::move(phase_mod)]() mutable {
    return std::sin(two_pi * (phase.next() + phase_mod.next()));
  });
}

[[nodiscard]] inline Stream saw(Stream hz) {
  return phasor(std::move(hz) / SampleRate).to_bipolar();
}

// MusicDSP LCG (Hal Chamberlain / Phil Burk). Output: float in [0, max).
// maybe rethink Stream<int> ?
[[nodiscard]] inline Stream rand(uint32_t max, uint32_t seed = 1u) {
  constexpr uint32_t multiplier = 196314165u;
  constexpr uint32_t addend = 907633515u;
  return Stream([state = seed, max]() mutable -> float {
    state = state * multiplier + addend;
    return static_cast<float>((static_cast<uint64_t>(state) * max) >> 32);
  });
}

// White noise in (-1, 1).
[[nodiscard]] inline Stream noise(uint32_t seed = 1u) {
  constexpr float denom =
      static_cast<float>(std::numeric_limits<uint32_t>::max());
  return (rand(std::numeric_limits<uint32_t>::max(), seed) / denom)
      .to_bipolar();
}

// Stepped random noise in (-1, 1): new value drawn at `freq` Hz, held between
// steps.
[[nodiscard]] inline Stream lf_noise0(Stream freq, uint32_t seed = 1u) {
  return Stream([freq = std::move(freq), rng = noise(seed), current = 0.f,
                 samples_left = 0]() mutable -> float {
    if (samples_left <= 0) {
      current = rng.next();
      float hz = freq.next();
      samples_left = static_cast<int>(SampleRate / std::max(hz, 0.001f));
    }
    --samples_left;
    return current;
  });
}

// Linear interpolation between random values drawn at `freq` Hz.
[[nodiscard]] inline Stream lf_noise1(Stream freq, uint32_t seed = 1u) {
  return Stream([freq = std::move(freq), rng = noise(seed), start = 0.f,
                 target = 0.f, samples_left = 0,
                 period = 1]() mutable -> float {
    if (samples_left <= 0) {
      start = target;
      target = rng.next();
      float hz = freq.next();
      period = std::max(static_cast<int>(SampleRate / std::max(hz, 0.001f)), 1);
      samples_left = period;
    }
    float t =
        static_cast<float>(period - samples_left) / static_cast<float>(period);
    --samples_left;
    return start + (target - start) * t;
  });
}

// Smoothstep interpolation between random values drawn at `freq` Hz.
[[nodiscard]] inline Stream lf_noise2(Stream freq, uint32_t seed = 1u) {
  return Stream([freq = std::move(freq), rng = noise(seed), start = 0.f,
                 target = 0.f, samples_left = 0,
                 period = 1]() mutable -> float {
    if (samples_left <= 0) {
      start = target;
      target = rng.next();
      float hz = freq.next();
      period = std::max(static_cast<int>(SampleRate / std::max(hz, 0.001f)), 1);
      samples_left = period;
    }
    float t =
        static_cast<float>(period - samples_left) / static_cast<float>(period);
    --samples_left;
    float smooth_t = t * t * (3.f - 2.f * t);
    return start + (target - start) * smooth_t;
  });
}

// Soft saturator: x / (1 + |x|), maps R -> (-1, 1).
[[nodiscard]] inline Stream distort(Stream x) {
  return Stream([x = std::move(x)]() mutable -> float {
    float xn = x.next();
    return xn / (1.f + std::abs(xn));
  });
}

// Triangle oscillator: linear ramp -1→+1→-1 per cycle.
[[nodiscard]] inline Stream tri(Stream hz) {
  return Stream(
      [ph = phasor(std::move(hz) / SampleRate)]() mutable -> float {
        float p = ph.next();
        return p < 0.5f ? 4.f * p - 1.f : 3.f - 4.f * p;
      });
}

// Square oscillator: +1 for first `width` of period, -1 otherwise.
[[nodiscard]] inline Stream sqr(Stream hz, Stream width = 0.5f) {
  return Stream(
      [ph = phasor(std::move(hz) / SampleRate),
       width = std::move(width)]() mutable -> float {
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
  return Stream([trigger = std::move(trigger), log_lo, log_range,
                 state = seed, value = lo]() mutable -> float {
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
  return Stream([src = std::move(src), in_lo, in_range, log_lo,
                 log_range]() mutable -> float {
    float t = std::clamp((src.next() - in_lo) / in_range, 0.f, 1.f);
    return std::exp(log_lo + t * log_range);
  });
}

// Wrap values into [lo, hi) with modular arithmetic.
[[nodiscard]] inline Stream wrap(Stream src, float lo, float hi) {
  float range = hi - lo;
  return Stream([src = std::move(src), lo, range]() mutable -> float {
    float v = std::fmod(src.next() - lo, range);
    if (v < 0.f)
      v += range;
    return lo + v;
  });
}

}  // namespace automata
