#include <rhythm.hpp>
using namespace automata;

// XOR of two Euclidean rhythms: shows each voice, then the gaps where exactly
// one is active — a complementary, gap-filling texture.
void rhythm_exprs() {
  constexpr auto alpha = Rhythm<16>::euclid(5);
  constexpr auto beta = Rhythm<16>::euclid(7);
  emit(alpha);
  emit(beta);
  emit(alpha ^ beta);
}
