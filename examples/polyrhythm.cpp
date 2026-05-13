#include <rhythm.hpp>
#include <tuple>
using namespace automata;

constexpr auto triple() { return Rhythm<3>::euclid(2); }
constexpr auto quad()   { return Rhythm<4>::euclid(3); }

constexpr auto tracks() {
    return std::make_tuple(triple(), quad(), triple() + quad());
}
