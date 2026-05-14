#pragma once
#include <span>
#include <functional>
#include <algorithm>

#include <euclid.hpp>

/**
 * @brief generic signal type
 * can represent triggers or audio signals, depending on the tick rate
 * the API is largely inspired by discrete time signal processing theory
 */

// order of implementation
// impulse → step → delay → scale/add → difference → convolution → accumulator →
// feedback → FFT

// TODO: construct signal from samples (fir)

// should you use unsigned int for index and argument

namespace automata {

class Signal {
  using Func = std::function<float(int)>;
  // should you use std::shared_ptr?
public:
  explicit Signal(Func generate) : gen(std::move(generate)) {}

  // evaluation
  [[nodiscard]] float operator[](int i) const { return gen(i); }

  // fundamental signals

  // DC signal
  explicit Signal(float v) : gen([v](int) { return v; }) {}

  static Signal impulse() {
    return Signal([](int i) { return (i == 0) ? 1.f : 0.f; });
  }

  static Signal step() {
    return Signal([](int i) { return (i >= 0) ? 1.f : 0.f; });
  }

  // impulse train
  static Signal every(int period) {
    return Signal([period](int i) { return ((i % period) == 0) ? 1.f : 0.f; });
  }

  [[nodiscard]] Signal delay(int z) const {
    return Signal([x = *this, z](int i) { return x[i - z]; });
  }  // or, use z(-d)?, should you forbid minus delay?

  [[nodiscard]] Signal convolve(const Signal& h) const {
    // assume both are causal signals (0 for i < 0)
    return Signal([x = *this, h](int n) {
      float sum = 0.f;
      for (int k = 0; k <= n; ++k)
        sum += x[k] * h[n - k];
      return sum;
    });
  }

  // scale
  [[nodiscard]] Signal operator*(float v) const {
    return Signal([x = *this, v](int i) { return x[i] * v; });
  }

  [[nodiscard]] Signal operator/(float v) const {
    return Signal([x = *this, v](int i) { return x[i] / v; });
  }

  // DC offset
  [[nodiscard]] Signal operator+(float v) const {
    return Signal([x = *this, v](int i) { return x[i] + v; });
  }

  [[nodiscard]] Signal operator-(float v) const {
    return Signal([x = *this, v](int i) { return x[i] - v; });
  }

  // render block of samples
  void render(std::span<float> buf, int start = 0) {
    std::generate(buf.begin(), buf.end(),
                  [&, i = start]() mutable { return gen(i++); });
  }

private:
  Func gen;
};

// arithmetic
[[nodiscard]] inline Signal operator+(const Signal& lhs, const Signal& rhs) {
  return Signal([lhs, rhs](int i) { return lhs[i] + rhs[i]; });
}

[[nodiscard]] inline Signal operator-(const Signal& lhs, const Signal& rhs) {
  return Signal([lhs, rhs](int i) { return lhs[i] - rhs[i]; });
}

[[nodiscard]] inline Signal operator*(const Signal& lhs, const Signal& rhs) {
  return Signal([lhs, rhs](int i) { return lhs[i] * rhs[i]; });
}

[[nodiscard]] inline Signal operator/(const Signal& lhs, const Signal& rhs) {
  return Signal([lhs, rhs](int i) { return lhs[i] / rhs[i]; });
}

// scalar arithmetic (commutative forms)
[[nodiscard]] inline Signal operator*(float v, const Signal& s) {
  return s * v;
}

[[nodiscard]] inline Signal operator+(float v, const Signal& s) {
  return s + v;
}

[[nodiscard]] inline Signal operator-(float v, const Signal& s) {
  return Signal(v) - s;
}

}  // namespace automata