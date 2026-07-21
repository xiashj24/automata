#include <catch2/catch_test_macros.hpp>

#include "automata/core/transport.hpp"

using automata::Quantize;
using automata::Transport;

TEST_CASE("immediate boundary is now", "[core][transport]") {
  Transport t;
  t.advance(12345);
  REQUIRE(t.next_boundary(Quantize::Immediate) == 12345);
}

TEST_CASE("beat and bar boundaries at 120 bpm", "[core][transport]") {
  Transport t;  // 120 bpm → 24000 samples per beat, 4/4 bars
  REQUIRE(t.next_boundary(Quantize::NextBeat) == 24000);
  REQUIRE(t.next_boundary(Quantize::NextBar) == 96000);

  t.advance(23999);
  REQUIRE(t.next_boundary(Quantize::NextBeat) == 24000);

  // Exactly on a boundary still means the *next* one.
  t.advance(1);
  REQUIRE(t.next_boundary(Quantize::NextBeat) == 48000);
}

TEST_CASE("bpm changes the grid", "[core][transport]") {
  Transport t;
  t.bpm = 60.f;  // 48000 samples per beat
  REQUIRE(t.next_boundary(Quantize::NextBeat) == 48000);
}

TEST_CASE("a tempo change keeps the beat grid continuous",
          "[core][transport]") {
  Transport t;
  t.advance(12000);  // half a beat into 120 bpm
  t.bpm = 60.f;      // the remaining half beat now takes 24000 samples
  REQUIRE(t.next_boundary(Quantize::NextBeat) == 12000 + 24000);
}

TEST_CASE("beat position accumulates by samples rendered",
          "[core][transport]") {
  Transport t;
  for (int i = 0; i < 750; ++i) {
    t.advance(128);  // 96000 samples = 4 beats at 120 bpm
  }
  REQUIRE(t.beat_pos > 3.9999);
  REQUIRE(t.beat_pos < 4.0001);
}
