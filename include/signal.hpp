#pragma once
#include <functional>
#include <initializer_list>
#include <memory>
#include <span>
#include <vector>

#include <euclid.hpp>

/**
 * @brief generic signal type
 * can represent triggers or audio signals, depending on the tick rate
 * the API is largely inspired by discrete time signal processing theory
 */

// order of implementation
// impulse → step → delay → scale/add → difference → convolution → accumulator →
// feedback → FFT

// TODO: construct signal from a span of samples (fir)

// should you use unsigned int for index and argument?

namespace automata {

class Signal {
  using Func = std::function<float(int)>;

public:
  explicit Signal(Func generate);
  explicit Signal(float v);

  explicit Signal(std::vector<float> samples);
  Signal(std::initializer_list<float> samples);

  [[nodiscard]] float operator[](int i) const;

  // fundamental signals
  static Signal impulse();
  static Signal step();
  static Signal every(int period);   // impulse train
  static Signal saw(int period);     // unipolar, does not reach 1.f
  static Signal square(int period);  // unipolar
  static Signal sin(int period);     // bipolar

  // TODO: construct a signal from a LUT of single cycle waveform

  [[nodiscard]] Signal delay(int z) const;
  [[nodiscard]] Signal operator>>(const Signal& h) const;  // convolution

  // scale
  [[nodiscard]] Signal operator*(float v) const;
  [[nodiscard]] Signal operator/(float v) const;

  // DC offset
  [[nodiscard]] Signal operator+(float v) const;
  [[nodiscard]] Signal operator-(float v) const;

  void render(std::span<float> buf, int start = 0);

private:
  std::shared_ptr<Func> gen;
};

// arithmetic
[[nodiscard]] Signal operator+(const Signal& lhs, const Signal& rhs);
[[nodiscard]] Signal operator-(const Signal& lhs, const Signal& rhs);
[[nodiscard]] Signal operator*(const Signal& lhs, const Signal& rhs);
[[nodiscard]] Signal operator/(const Signal& lhs, const Signal& rhs);

// assume both are causal signals (0 for i < 0)
[[nodiscard]] Signal convolve(const Signal& x, const Signal& h);

// scalar arithmetic (commutative forms)
[[nodiscard]] Signal operator*(float v, const Signal& s);
[[nodiscard]] Signal operator+(float v, const Signal& s);
[[nodiscard]] Signal operator-(float v, const Signal& s);

}  // namespace automata
