#include <signal.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace automata {

Signal::Signal(Func generate)
    : gen(std::make_shared<Func>(std::move(generate))) {}

Signal::Signal(float v) : gen(std::make_shared<Func>([v](int) { return v; })) {}

Signal::Signal(std::vector<float> samples)
    : gen(std::make_shared<Func>([data = std::move(samples)](int i) {
        return (i >= 0 && static_cast<std::size_t>(i) < data.size()) ? data[i]
                                                                     : 0.f;
      })) {}

Signal::Signal(std::initializer_list<float> samples)
    : Signal(std::vector<float>(samples)) {}

float Signal::operator[](int i) const {
  return (*gen)(i);
}

Signal Signal::impulse() {
  return Signal([](int i) { return (i == 0) ? 1.f : 0.f; });
}

Signal Signal::step() {
  return Signal([](int i) { return (i >= 0) ? 1.f : 0.f; });
}

Signal Signal::every(int period) {
  return Signal([period](int i) { return ((i % period) == 0) ? 1.f : 0.f; });
}

// Signal Signal::saw(int period) {
//   return Signal([period](int i) {
//     return static_cast<float>(i % period) / static_cast<float>(period);
//   });
// }

// Signal Signal::square(int period) {
//   return Signal(
//       [period](int i) { return (i % period) < (period / 2) ? 1.f : 0.f; });
// }

// Signal Signal::sin(int period) {
//   return Signal([period](int i) {
//     constexpr float two_pi = 2.f * std::numbers::pi_v<float>;
//     return std::sin(two_pi * static_cast<float>(i) /
//                     static_cast<float>(period));
//   });
// }

Signal Signal::phasor(float w) {
  return Signal(
      [w](int i) { return std::fmod(w * static_cast<float>(i), 1.f); });
}

Signal Signal::osc(float w) {
  constexpr float two_pi = 2.f * std::numbers::pi_v<float>;
  Signal phase = phasor(w);
  return Signal([phase, two_pi](int i) { return std::sin(two_pi * phase[i]); });
}

Signal Signal::delay(int z) const {
  return Signal([x = *this, z](int i) { return x[i - z]; });
}

Signal Signal::operator>>(const Signal& h) const {
  return convolve(*this, h);
}

Signal Signal::operator*(float v) const {
  return Signal([x = *this, v](int i) { return x[i] * v; });
}

Signal Signal::operator/(float v) const {
  return Signal([x = *this, v](int i) { return x[i] / v; });
}

Signal Signal::operator+(float v) const {
  return Signal([x = *this, v](int i) { return x[i] + v; });
}

Signal Signal::operator-(float v) const {
  return Signal([x = *this, v](int i) { return x[i] - v; });
}

void Signal::render(std::span<float> buf, int start) {
  std::generate(buf.begin(), buf.end(),
                [&, i = start]() mutable { return (*gen)(i++); });
}

Signal operator+(const Signal& lhs, const Signal& rhs) {
  return Signal([lhs, rhs](int i) { return lhs[i] + rhs[i]; });
}

Signal operator-(const Signal& lhs, const Signal& rhs) {
  return Signal([lhs, rhs](int i) { return lhs[i] - rhs[i]; });
}

Signal operator*(const Signal& lhs, const Signal& rhs) {
  return Signal([lhs, rhs](int i) { return lhs[i] * rhs[i]; });
}

Signal operator/(const Signal& lhs, const Signal& rhs) {
  return Signal([lhs, rhs](int i) { return lhs[i] / rhs[i]; });
}

Signal convolve(const Signal& x, const Signal& h) {
  return Signal([x, h](int n) {
    float sum = 0.f;
    for (int k = 0; k <= n; ++k)
      sum += x[k] * h[n - k];
    return sum;
  });
}

Signal operator*(float v, const Signal& s) {
  return s * v;
}

Signal operator+(float v, const Signal& s) {
  return s + v;
}

Signal operator-(float v, const Signal& s) {
  return Signal(v) - s;
}

}  // namespace automata
