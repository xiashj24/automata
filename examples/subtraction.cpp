#include <rhythm.hpp>
#include <tuple>
using namespace automata;

constexpr int steps = 16;
constexpr int pulses = 4;
constexpr int rotation = 2;

constexpr auto kick() {
  return Rhythm<steps>::euclid(pulses);
}
constexpr auto snare() {
  return kick() >> rotation;
}
constexpr auto hi_hat() {
  return Rhythm<16>::fill() - snare();
}

constexpr auto tracks() {
  return std::make_tuple(kick(), snare(), hi_hat());
}
