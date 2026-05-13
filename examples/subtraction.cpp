#include <rhythm.hpp>
using namespace automata;

// Rhythmic subtraction: shows kick and snare separately, then the gaps left
// when the snare punches holes in the kick with AND-NOT.
void rhythm_exprs() {
  constexpr auto kick = Rhythm<8>::euclid(5);
  constexpr auto snare = Rhythm<8>::euclid(3);
  emit(kick);
  emit(snare);
  emit(kick - snare);
}
