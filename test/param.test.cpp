#include <graph.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using automata::Param;
using automata::Stream;
using Catch::Approx;

TEST_CASE("Param: initial value readable via get()", "[param]") {
  Param p("cutoff", 440.f, 20.f, 20000.f);
  REQUIRE(p.get() == Approx(440.f));
}

TEST_CASE("Param: set() updates get()", "[param]") {
  Param p("cutoff", 440.f, 20.f, 20000.f);
  p.set(880.f);
  REQUIRE(p.get() == Approx(880.f));
}

TEST_CASE("Param: implicit Stream conversion reads current value", "[param]") {
  Param p("rate", 1.f, 0.f, 10.f);
  Stream s = p;
  REQUIRE(s.next() == Approx(1.f));
  p.set(3.5f);
  REQUIRE(s.next() == Approx(3.5f));
}

TEST_CASE("Param: Stream from copy shares the same slot", "[param]") {
  Param p("gain", 0.5f, 0.f, 1.f);
  Stream s1 = p;
  Stream s2 = p;
  p.set(0.9f);
  REQUIRE(s1.next() == Approx(0.9f));
  REQUIRE(s2.next() == Approx(0.9f));
}
