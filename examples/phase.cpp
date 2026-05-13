#include <rhythm.hpp>
using namespace automata;

// Steve Reich-style phase: shows the base pattern, the rotated copy, then
// their union — the characteristic phasing texture.
void rhythm_exprs() {
  constexpr auto base = Rhythm<8>::euclid(3);
  constexpr auto shifted = base >> 2;
  emit(base);
  emit(shifted);
  emit(base + shifted);
}
