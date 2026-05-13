#include <rhythm.hpp>
#include <tuple>
using namespace automata;

constexpr auto alpha() { return Rhythm<16>::euclid(5); }
constexpr auto beta()  { return Rhythm<16>::euclid(7); }

constexpr auto tracks() {
    return std::make_tuple(alpha(), beta(), alpha() ^ beta());
}
