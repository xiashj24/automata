#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

#include <samplerate.hpp>
#include <stream.hpp>

namespace automata {

namespace detail {
constexpr uint32_t lcg_step(uint32_t s) {
  return s * 196314165u + 907633515u;
}
}  // namespace detail

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
  return (rand(std::numeric_limits<uint32_t>::max(), seed) / denom).bipolar();
}

// Stepped random noise in (-1, 1): new value drawn at `freq` Hz, held between
// steps.
[[nodiscard]] inline Stream lf_noise_0(Stream freq, uint32_t seed = 1u) {
  return Stream([freq, rng = noise(seed), current = 0.f,
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
[[nodiscard]] inline Stream lf_noise_1(Stream freq, uint32_t seed = 1u) {
  return Stream([freq, rng = noise(seed), start = 0.f, target = 0.f,
                 samples_left = 0, period = 1]() mutable -> float {
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
[[nodiscard]] inline Stream lf_noise_2(Stream freq, uint32_t seed = 1u) {
  return Stream([freq, rng = noise(seed), start = 0.f, target = 0.f,
                 samples_left = 0, period = 1]() mutable -> float {
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

// Pink (1/f) noise using Paul Kellett's 6-octave IIR approximation.
[[nodiscard]] inline Stream pink_noise(uint32_t seed = 1u) {
  return Stream([rng = seed, b = std::array<float, 7>{}]() mutable -> float {
    rng = detail::lcg_step(rng);
    float w =
        static_cast<float>(rng) / static_cast<float>(UINT32_MAX) * 2.f - 1.f;
    b[0] = 0.99886f * b[0] + w * 0.0555179f;
    b[1] = 0.99332f * b[1] + w * 0.0750759f;
    b[2] = 0.96900f * b[2] + w * 0.1538520f;
    b[3] = 0.86650f * b[3] + w * 0.3104856f;
    b[4] = 0.55000f * b[4] + w * 0.5329522f;
    b[5] = -0.7616f * b[5] - w * 0.0168980f;
    float pink =
        (b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + w * 0.5362f) * 0.11f;
    b[6] = w * 0.115926f;
    return std::clamp(pink, -1.f, 1.f);
  });
}

// Gaussian noise via Box-Muller transform. Generates pairs — every other call
// returns the stored spare value.
[[nodiscard]] inline Stream gaussian_noise(float mean = 0.f,
                                           float std_dev = 1.f,
                                           uint32_t seed = 1u) {
  return Stream([mean, std_dev, rng = noise(seed), spare = 0.f,
                 has_spare = false]() mutable -> float {
    constexpr float two_pi = 2.f * std::numbers::pi_v<float>;
    if (has_spare) {
      has_spare = false;
      return spare;
    }
    float u = rng.next() * 0.5f + 0.5f;
    float v = rng.next() * 0.5f + 0.5f;
    float r = std::sqrt(-2.f * std::log(std::max(u, 1e-10f)));
    spare = (r * std::sin(two_pi * v)) * std_dev + mean;
    has_spare = true;
    return (r * std::cos(two_pi * v)) * std_dev + mean;
  });
}

// Bernoulli trigger: fires with probability `prob` on each rising edge of
// `trigger`.
[[nodiscard]] inline Stream rand_coin(Stream prob,
                                      Stream trigger,
                                      uint32_t seed = 1u) {
  return Stream([prob, trigger, rng = seed, prev = 0.f]() mutable -> float {
    float t = trigger.next();
    bool fired = prev < 0.5f && t >= 0.5f;
    prev = t;
    if (!fired)
      return 0.f;
    rng = detail::lcg_step(rng);
    float r = static_cast<float>(rng) / static_cast<float>(UINT32_MAX);
    return r < prob.next() ? 1.f : 0.f;
  });
}

// Poisson random impulse: fires with probability rate_hz/SR each sample,
// producing a one-sample pulse on average `rate_hz` times per second.
[[nodiscard]] inline Stream rand_impulse(Stream rate_hz, uint32_t seed = 1u) {
  return Stream([rate_hz, rng = seed]() mutable -> float {
    rng = detail::lcg_step(rng);
    float p = std::max(rate_hz.next(), 0.f) / SampleRate;
    float r = static_cast<float>(rng) / static_cast<float>(UINT32_MAX);
    return r < p ? 1.f : 0.f;
  });
}

// Logistic map chaos generator: x[n] = r·x[n-1]·(1−x[n-1]).
// Chaotic regime: r in [3.57, 4]. Output mapped to [−1, 1].
[[nodiscard]] inline Stream logistic(Stream r = 3.9f, float x_init = 0.5f) {
  return Stream([r, x = x_init]() mutable -> float {
    x = r.next() * x * (1.f - x);
    return x * 2.f - 1.f;
  });
}

}  // namespace automata
