#include <rhythm.hpp>
#include <tuple>
using namespace automata;

constexpr auto tracks() {
    return std::make_tuple(Rhythm<8>::euclid(5));
}
