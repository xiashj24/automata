#include <rhythm.hpp>
using namespace automata;

// Stretch: shows the tresillo at original density, then stretched 2x — each
// hit expanded by inserting a rest, halving the effective tempo.
void rhythm_exprs() {
  constexpr auto base = Rhythm<8>::euclid(3);
  emit(base);
  emit(base.stretch<2>());
}
