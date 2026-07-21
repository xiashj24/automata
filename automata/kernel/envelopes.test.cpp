#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "automata/kernel/envelopes.hpp"

#include <algorithm>
#include <type_traits>

using Catch::Approx;

static_assert(std::is_trivially_copyable_v<automata::Ar>);
static_assert(std::is_trivially_copyable_v<automata::Are>);

namespace {
// Times reach the kernel in seconds; the samples→seconds→samples round trip
// can land a stage boundary one sample off, so probes leave slack.
constexpr float sec(float samples) {
  return samples / automata::SampleRateF;
}
}  // namespace

TEST_CASE("ar stays idle without a trigger", "[kernel][envelopes]") {
  automata::Ar env;
  env.set_attack(sec(10.f));
  env.set_release(sec(20.f));

  for (int i = 0; i < 100; ++i) {
    REQUIRE(env.process(0.f) == 0.f);
  }
}

TEST_CASE("ar rises to one, then releases to zero", "[kernel][envelopes]") {
  automata::Ar env;
  env.set_attack(sec(10.f));
  env.set_release(sec(20.f));

  float value = env.process(1.f);  // rising edge
  for (int i = 1; i < 9; ++i) {    // strictly rising, well inside the attack
    const float next = env.process(0.f);
    REQUIRE(next > value);
    value = next;
  }

  // The attack clamp lands exactly on 1 within a couple more samples.
  float peak = value;
  for (int i = 0; i < 3; ++i) {
    peak = std::max(peak, env.process(0.f));
  }
  REQUIRE(peak == 1.f);

  // Falls monotonically, clamping exactly to 0.
  float last = 1.f;
  for (int i = 0; i < 25; ++i) {
    const float next = env.process(0.f);
    REQUIRE(next <= last);
    last = next;
  }
  REQUIRE(last == 0.f);
  REQUIRE(env.process(0.f) == 0.f);  // idle again
}

TEST_CASE("ar retriggers mid-release on a new rising edge",
          "[kernel][envelopes]") {
  automata::Ar env;
  env.set_attack(sec(4.f));
  env.set_release(sec(100.f));

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
  for (int i = 0; i < 5; ++i) {
    peak = std::max(peak, env.process(0.f));
  }
  REQUIRE(peak == 1.f);
}

TEST_CASE("are chases the gate exponentially, within 60 dB after t60",
          "[kernel][envelopes]") {
  automata::Are env;
  env.set_attack(sec(100.f));
  env.set_release(sec(200.f));

  float value = 0.f;
  for (int i = 0; i < 100; ++i) {
    const float next = env.process(1.f);
    REQUIRE(next > value);
    value = next;
  }
  REQUIRE(value == Approx(1.f).margin(2e-3));  // 60 dB from the target

  for (int i = 0; i < 200; ++i) {
    const float next = env.process(0.f);
    REQUIRE(next < value);
    value = next;
  }
  REQUIRE(value == Approx(0.f).margin(2e-3));
}
