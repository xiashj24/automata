#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "automata/kernel/oscillators.hpp"

#include <type_traits>

using Catch::Approx;

static_assert(std::is_trivially_copyable_v<automata::Phasor>);
static_assert(std::is_trivially_copyable_v<automata::Sine>);
static_assert(std::is_trivially_copyable_v<automata::Saw>);
static_assert(std::is_trivially_copyable_v<automata::Metro>);

TEST_CASE("phasor ramps and wraps", "[kernel][oscillators]") {
  automata::Phasor p;
  p.set_freq(0.25f);

  REQUIRE(p.process() == Approx(0.f));
  REQUIRE(p.process() == Approx(0.25f));
  REQUIRE(p.process() == Approx(0.5f));
  REQUIRE(p.process() == Approx(0.75f));
  REQUIRE(p.process() == Approx(0.f));  // wrapped
}

TEST_CASE("sine starts at zero and hits its quarter-cycle peak",
          "[kernel][oscillators]") {
  automata::Sine s;
  s.set_freq(1.f / 64.f);

  REQUIRE(s.process() == Approx(0.f).margin(1e-6));
  for (int i = 1; i < 16; ++i) {
    (void)s.process();
  }
  REQUIRE(s.process() == Approx(1.f).margin(1e-5));  // sample 16 of 64
}

TEST_CASE("saw spans -1..1 over one period", "[kernel][oscillators]") {
  automata::Saw s;
  s.set_freq(0.25f);

  REQUIRE(s.process() == Approx(-1.f));
  REQUIRE(s.process() == Approx(-0.5f));
  REQUIRE(s.process() == Approx(0.f));
  REQUIRE(s.process() == Approx(0.5f));
  REQUIRE(s.process() == Approx(-1.f));  // wrapped
}

TEST_CASE("metro fires on its first sample, then once per period",
          "[kernel][oscillators]") {
  automata::Metro m;
  m.set_freq(0.1f);  // period of 10 samples

  REQUIRE(m.process() == 1.f);
  for (int i = 1; i < 10; ++i) {
    REQUIRE(m.process() == 0.f);
  }
  REQUIRE(m.process() == 1.f);
}

TEST_CASE("reset returns an oscillator to its initial state",
          "[kernel][oscillators]") {
  automata::Phasor p;
  p.set_freq(0.3f);
  (void)p.process();
  (void)p.process();

  p.reset();

  REQUIRE(p.process() == Approx(0.f));
}
