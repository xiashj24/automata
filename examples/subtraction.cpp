#include <rhythm.hpp>
#include <tuple>
using namespace automata;

constexpr auto kick() {
  return Rhythm<16>::euclid(4);
}
constexpr auto hihat() {
  return kick() >> 2;
}
constexpr auto snare() {
  return (Rhythm<16>::fill() - hihat()) ^ kick();
}

constexpr auto tracks() {
  return std::make_tuple(kick(), snare(), hihat());
}
