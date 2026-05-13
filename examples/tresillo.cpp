#include <rhythm.hpp>
using namespace automata;

// Tresillo: the foundational 3-against-8 clave pattern in Afro-Cuban music.
void rhythm_exprs() {
  emit(Rhythm<8>::euclid(3));
}
