#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <span>

#include <signal.hpp>

namespace automata {

/**
 * @brief sequential stream of samples
 * @todo noise generator and rng (with seeding)
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

  float next() { return gen(); }

  void render(std::span<float> buf) {
    std::generate(buf.begin(), buf.end(), [this] { return next(); });
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

[[nodiscard]] inline Stream osc(Stream w, Stream phase_mod = 0.f) {
  constexpr float two_pi = 2.f * std::numbers::pi_v<float>;
  return Stream(
      [phase = phasor(w), phase_mod = std::move(phase_mod)]() mutable {
        return std::sin(two_pi * (phase.next() + phase_mod.next()));
      });
}

template <typename T>
T to_bipolar(T uni) {
  return uni * 2.f - 1.f;
}

template <typename T>
T to_unipolar(T bi) {
  return (bi + 1.f) * 0.5f;
}

[[nodiscard]] inline Stream saw(Stream w) {
  return to_bipolar(phasor(w));
}

}  // namespace automata
