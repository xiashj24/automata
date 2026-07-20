#include <catch2/catch_test_macros.hpp>

#include "automata/engine/transport.hpp"

using automata::Quantize;
using automata::Transport;

TEST_CASE("immediate boundary is now", "[engine][transport]") {
  Transport t;
  t.sample_pos = 12345;
  REQUIRE(t.next_boundary(Quantize::Immediate) == 12345);
}

TEST_CASE("beat and bar boundaries at 120 bpm", "[engine][transport]") {
  Transport t;  // 120 bpm → 24000 samples per beat, 4/4 bars
  REQUIRE(t.next_boundary(Quantize::NextBeat) == 24000);
  REQUIRE(t.next_boundary(Quantize::NextBar) == 96000);

  t.sample_pos = 23999;
  REQUIRE(t.next_boundary(Quantize::NextBeat) == 24000);

  // Exactly on a boundary still means the *next* one.
  t.sample_pos = 24000;
  REQUIRE(t.next_boundary(Quantize::NextBeat) == 48000);
}

TEST_CASE("bpm changes the grid", "[engine][transport]") {
  Transport t;
  t.bpm = 60.f;  // 48000 samples per beat
  REQUIRE(t.next_boundary(Quantize::NextBeat) == 48000);
}
