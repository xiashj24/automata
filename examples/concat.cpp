#include <rhythm.hpp>
using namespace automata;

// Concatenation: the first half, its inverse, and the full bar stitched
// together — a call-and-response feel over 16 steps.
void rhythm_exprs() {
  constexpr auto call = Rhythm<8>::euclid(3);
  constexpr auto response = ~call;
  emit(call);
  emit(response);
  emit(call | response);
}
