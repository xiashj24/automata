#include <signal.hpp>
#include <filter.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace automata;

TEST_CASE("empty signal", "[signal][create]") {
  Signal sig(0.f);
  REQUIRE(sig[0] == 0.f);
  REQUIRE(sig[1] == 0.f);
  REQUIRE(sig[2] == 0.f);
  REQUIRE(sig[42] == 0.f);
  REQUIRE(sig[10000] == 0.f);
  REQUIRE(sig[-1] == 0.f);
}

TEST_CASE("dc signal", "[signal][create]") {
  Signal sig(1.f);
  REQUIRE(sig[0] == 1.f);
  REQUIRE(sig[1] == 1.f);
  REQUIRE(sig[2] == 1.f);
  REQUIRE(sig[42] == 1.f);
  REQUIRE(sig[10000] == 1.f);
  REQUIRE(sig[-1] == 1.f);
}

TEST_CASE("impulse signal", "[signal][create]") {
  auto sig = Signal::impulse();
  REQUIRE(sig[0] == 1.f);
  REQUIRE(sig[1] == 0.f);
  REQUIRE(sig[2] == 0.f);
  REQUIRE(sig[42] == 0.f);
  REQUIRE(sig[10000] == 0.f);
  REQUIRE(sig[-1] == 0.f);
}

TEST_CASE("step signal", "[signal][create]") {
  auto sig = Signal::step();
  REQUIRE(sig[0] == 1.f);
  REQUIRE(sig[1] == 1.f);
  REQUIRE(sig[2] == 1.f);
  REQUIRE(sig[42] == 1.f);
  REQUIRE(sig[10000] == 1.f);
  REQUIRE(sig[-1] == 0.f);
}

TEST_CASE("fir filter", "[signal][create]") {
  Signal sig = {1.f, 0.f, -1.f, 0.5f, -0.5f};
  REQUIRE(sig[0] == 1.f);
  REQUIRE(sig[1] == 0.f);
  REQUIRE(sig[2] == -1.f);
  REQUIRE(sig[3] == 0.5f);
  REQUIRE(sig[4] == -0.5f);
}

TEST_CASE("delayed signal", "[signal][delay]") {
  Signal original = {1.f, 0.f, -1.f, 0.5f, -0.5f};
  Signal delayed = original.delay(2);
  REQUIRE(delayed[0] == 0.f);
  REQUIRE(delayed[1] == 0.f);
  REQUIRE(delayed[2] == 1.f);
  REQUIRE(delayed[3] == 0.f);
  REQUIRE(delayed[4] == -1.f);
  REQUIRE(delayed[5] == 0.5f);
  REQUIRE(delayed[6] == -0.5f);
}

TEST_CASE("impulse train", "[signal][create]") {
  auto sig = Signal::every(4);
  REQUIRE(sig[0] == 1.f);
  REQUIRE(sig[1] == 0.f);
  REQUIRE(sig[2] == 0.f);
  REQUIRE(sig[3] == 0.f);
  REQUIRE(sig[4] == 1.f);
  REQUIRE(sig[8] == 1.f);
  REQUIRE(sig[40] == 1.f);
  REQUIRE(sig[-1] == 0.f);
  REQUIRE(sig[-4] == 1.f);
}

TEST_CASE("biquad feedforward impulse response", "[signal][biquad]") {
  BiquadState state;
  auto sig =
      biquad(Signal::impulse(), {.a0 = 1.f, .a1 = 0.5f, .a2 = 0.25f}, state);
  REQUIRE(sig[0] == 1.f);
  REQUIRE(sig[1] == 0.5f);
  REQUIRE(sig[2] == 0.25f);
  REQUIRE(sig[3] == 0.f);
}

TEST_CASE("biquad feedback impulse response", "[signal][biquad]") {
  BiquadState state;
  auto sig = biquad(Signal::impulse(), {.a0 = 1.f, .b1 = 0.5f}, state);
  REQUIRE(sig[0] == 1.f);
  REQUIRE(sig[1] == -0.5f);
  REQUIRE(sig[2] == 0.25f);
  REQUIRE(sig[3] == -0.125f);
}
