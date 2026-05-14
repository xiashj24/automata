#include <signal.hpp>
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


