#include <signal.hpp>
#include <filter.hpp>

#include <catch2/catch_approx.hpp>
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
  REQUIRE(sig.next() == 1.f);
  REQUIRE(sig.next() == 0.5f);
  REQUIRE(sig.next() == 0.25f);
  REQUIRE(sig.next() == 0.f);
}

TEST_CASE("biquad feedback impulse response", "[signal][biquad]") {
  BiquadState state;
  auto sig = biquad(Signal::impulse(), {.a0 = 1.f, .b1 = 0.5f}, state);
  REQUIRE(sig.next() == 1.f);
  REQUIRE(sig.next() == -0.5f);
  REQUIRE(sig.next() == 0.25f);
  REQUIRE(sig.next() == -0.125f);
}

TEST_CASE("biquad state is externally visible", "[signal][biquad]") {
  BiquadState state;
  auto sig = biquad(Signal::impulse(), {.a0 = 1.f, .b1 = 0.5f}, state);
  sig.next();  // n=0: yn=1, s1 = -0.5, s2 = 0
  REQUIRE(state.s1 == -0.5f);
  REQUIRE(state.s2 == 0.f);
  sig.next();  // n=1: yn=-0.5
  REQUIRE(state.s1 == 0.25f);
}

TEST_CASE("phasor", "[signal][create]") {
  auto sig = Signal::phasor(0.25f);  // period = 4 samples
  REQUIRE(sig[0] == 0.f);
  REQUIRE(sig[1] == 0.25f);
  REQUIRE(sig[2] == 0.5f);
  REQUIRE(sig[3] == 0.75f);
  REQUIRE(sig[4] == 0.f);  // wraps
  REQUIRE(sig[8] == 0.f);  // wraps again
}

TEST_CASE("osc", "[signal][create]") {
  auto sig = Signal::osc(0.25f);  // period = 4 samples
  REQUIRE(sig[0] == Catch::Approx(0.f).margin(1e-6f));
  REQUIRE(sig[1] == Catch::Approx(1.f));                // sin(π/2)
  REQUIRE(sig[2] == Catch::Approx(0.f).margin(1e-6f));  // sin(π)
  REQUIRE(sig[3] == Catch::Approx(-1.f));               // sin(3π/2)
  REQUIRE(sig[4] == Catch::Approx(0.f).margin(1e-6f));  // wraps back to sin(2π)
}

TEST_CASE("lag with Signal-valued coefficient", "[signal][lag]") {
  float yz = 0.f;
  Signal coeff = Signal::phasor(1.f / 4);  // 0, 0.25, 0.5, 0.75, ...
  auto sig = lag(1.f, coeff, yz);
  // n=0: ac=0   → yn = 1*1 + 0*0   = 1.0
  // n=1: ac=0.25 → yn = 0.75*1 + 0.25*1 = 1.0
  REQUIRE(sig.next() == 1.f);
  REQUIRE(sig.next() == 1.f);
}

TEST_CASE("slew limiter", "[signal][slew]") {
  float yz = 0.f;
  auto sig = slew(1.f, 0.25f, 0.25f, yz);
  REQUIRE(sig.next() == 0.25f);
  REQUIRE(sig.next() == 0.5f);
  REQUIRE(sig.next() == 0.75f);
  REQUIRE(sig.next() == 1.0f);
}

TEST_CASE("lag filter", "[signal][lag]") {
  float yz = 0.f;
  auto sig = lag(1.f, 0.5f, yz);
  REQUIRE(sig.next() == 0.5f);
  REQUIRE(sig.next() == 0.75f);
  REQUIRE(sig.next() == 0.875f);
}

TEST_CASE("composed lag filters", "[signal][lag]") {
  float yz1 = 0.f, yz2 = 0.f;
  // Two lag filters in series — Signal::Stream output feeds directly into
  // next
  auto filtered = lag(lag(1.f, 0.5f, yz1), 0.5f, yz2);
  // Stage 1 output: 0.5, 0.75, 0.875, ...
  // Stage 2 input is stage 1 output, also with a=0.5:
  // n=0: yn2 = 0.5*0.5 + 0.5*0 = 0.25
  // n=1: yn2 = 0.5*0.75 + 0.5*0.25 = 0.375 + 0.125 = 0.5
  REQUIRE(filtered.next() == 0.25f);
  REQUIRE(filtered.next() == 0.5f);
}

TEST_CASE("Signal::Stream modulated by stateful signal", "[signal][lag]") {
  float env_yz = 0.f, lag_yz = 0.f;
  // Slew-limited envelope (0→1 at rate 0.5/sample) modulates the lag coeff
  auto filtered = lag(1.f, slew(1.f, 0.5f, 0.5f, env_yz), lag_yz);
  // Envelope: n=0→0.5, n=1→1.0
  // n=0: ac=clamp(0.5)=0.5, yn = 0.5*1 + 0.5*0 = 0.5
  // n=1: ac=clamp(1.0)=1.0, yn = 0*1 + 1*0.5  = 0.5
  REQUIRE(filtered.next() == 0.5f);
  REQUIRE(filtered.next() == 0.5f);
}
