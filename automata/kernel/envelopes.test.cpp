#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "automata/kernel/envelopes.hpp"

#include <type_traits>

using Catch::Approx;

static_assert(std::is_trivially_copyable_v<automata::Ar>);

TEST_CASE("ar stays idle without a trigger", "[kernel][envelopes]") {
  automata::Ar env;
  env.set_attack(10.f);
  env.set_release(20.f);

  for (int i = 0; i < 100; ++i) {
    REQUIRE(env.process(0.f) == 0.f);
  }
}

TEST_CASE("ar rises to one, then releases to zero", "[kernel][envelopes]") {
  automata::Ar env;
  env.set_attack(10.f);
  env.set_release(20.f);

  float peak = 0.f;
  float value = env.process(1.f);  // rising edge
  for (int i = 1; i < 10; ++i) {
    const float next = env.process(0.f);
    REQUIRE(next > value);
    value = next;
    peak = next;
  }
  REQUIRE(peak == Approx(1.f));

  for (int i = 0; i < 20; ++i) {
    const float next = env.process(0.f);
    REQUIRE(next < value);
    value = next;
  }
  REQUIRE(value == 0.f);
  REQUIRE(env.process(0.f) == 0.f);  // idle again
}

TEST_CASE("ar retriggers mid-release on a new rising edge",
          "[kernel][envelopes]") {
  automata::Ar env;
  env.set_attack(4.f);
  env.set_release(100.f);

  (void)env.process(1.f);
  for (int i = 0; i < 10; ++i) {
    (void)env.process(0.f);
  }
  const float mid = env.process(0.f);
  REQUIRE(mid < 1.f);

  // Attack resumes from the current level (click-free), so the peak lands
  // on exactly 1.0 within one attack length of the retrigger.
  float peak = env.process(1.f);
  REQUIRE(peak > mid);
  for (int i = 0; i < 4; ++i) {
    peak = std::max(peak, env.process(0.f));
  }
  REQUIRE(peak == 1.f);
}
