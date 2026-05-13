#include <rhythm.hpp>
#include <tuple>
using namespace automata;

constexpr auto base() { return Rhythm<8>::euclid(3); }

constexpr auto tracks() {
    return std::make_tuple(base(), base().stretch<2>());
}
