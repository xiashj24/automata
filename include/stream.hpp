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
  constexpr float denom = static_cast<float>(std::numeric_limits<uint32_t>::max());
  return (rand(std::numeric_limits<uint32_t>::max(), seed) / denom).to_bipolar();
}

}  // namespace automata
