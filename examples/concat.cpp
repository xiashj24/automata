#include <rhythm.hpp>
#include <tuple>
using namespace automata;

constexpr auto call() { return Rhythm<8>::euclid(3); }

constexpr auto tracks() {
    return std::make_tuple(call(), ~call(), call() | ~call());
}
