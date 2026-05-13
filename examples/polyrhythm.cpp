#include <rhythm.hpp>
using namespace automata;

// 3-against-4 polyrhythm: two Euclidean patterns of different lengths tiled to
// their LCM (12). Each band shows one voice; the third shows their union.
void rhythm_exprs() {
  constexpr auto triple = Rhythm<3>::euclid(2);
  constexpr auto quad = Rhythm<4>::euclid(3);
  emit(triple);
  emit(quad);
  emit(triple + quad);
}
