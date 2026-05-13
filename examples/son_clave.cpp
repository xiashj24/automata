#include <rhythm.hpp>
using namespace automata;

// Son clave: the 3-2 clave used throughout Latin music.
void rhythm_exprs() {
  emit(Rhythm<8>::euclid(5));
}
