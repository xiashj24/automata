#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "automata/kernel/rhythm.hpp"

#include <type_traits>

using Catch::Approx;

static_assert(std::is_trivially_copyable_v<automata::ClockTrig>);
static_assert(std::is_trivially_copyable_v<automata::Latch>);

TEST_CASE("a fresh trig fires the downbeat at cycle start",
          "[kernel][rhythm]") {
  automata::ClockTrig t;
  REQUIRE(t.process(0.f) == 1.f);
  REQUIRE(t.process(0.1f) == 0.f);
}

TEST_CASE("a trig created mid-cycle waits for the wrap", "[kernel][rhythm]") {
  automata::ClockTrig t;
  REQUIRE(t.process(0.6f) == 0.f);
  REQUIRE(t.process(0.8f) == 0.f);
  REQUIRE(t.process(0.0f) == 1.f);  // the wrap
  REQUIRE(t.process(0.2f) == 0.f);
}

TEST_CASE("small backward jitter is not a wrap", "[kernel][rhythm]") {
  automata::ClockTrig t;
  (void)t.process(0.5f);
  REQUIRE(t.process(0.45f) == 0.f);
}

TEST_CASE("latch captures on a rising edge and holds", "[kernel][rhythm]") {
  automata::Latch l;

  REQUIRE(l.process(0.3f, 0.f) == 0.f);   // nothing captured yet
  REQUIRE(l.process(0.7f, 1.f) == 0.7f);  // rising edge captures
  REQUIRE(l.process(0.9f, 1.f) == 0.7f);  // held while the gate stays high
  REQUIRE(l.process(0.2f, 0.f) == 0.7f);  // held through the gap
  REQUIRE(l.process(0.4f, 1.f) == 0.4f);  // next edge recaptures
}
