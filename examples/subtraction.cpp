#include <rhythm.hpp>
#include <tuple>
using namespace automata;

constexpr int steps    = 16;
constexpr int pulses   = 4;
constexpr int rotation = 2;

constexpr auto kick()   { return Rhythm<steps>::euclid(pulses)<<3; }
constexpr auto snare()  { return kick() >> rotation; }
constexpr auto hi_hat() { return (kick() + snare()) >> 1; }

constexpr auto tracks() {
    return std::make_tuple(kick(), snare(), hi_hat());
}
