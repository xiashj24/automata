#include <signal.hpp>
#include <catch2/catch_test_macros.hpp>

using automata::Gate;

static int count_hits(const Gate& sig, int length) {
  int hits = 0;
  for (int i = 0; i < length; ++i)
    if (sig(i))
      ++hits;
  return hits;
}

TEST_CASE("fill is always true", "[signal]") {
  auto sig = Gate::fill();
  REQUIRE(sig(0));
  REQUIRE(sig(7));
  REQUIRE(sig(15));
}

TEST_CASE("rest is always false", "[signal]") {
  auto sig = Gate::rest();
  REQUIRE_FALSE(sig(0));
  REQUIRE_FALSE(sig(7));
  REQUIRE_FALSE(sig(15));
}

TEST_CASE("from_pattern", "[signal]") {
  auto beat = Gate::from_pattern("x.x.x.x.");
  REQUIRE(beat(0));
  REQUIRE(beat(2));
  REQUIRE(beat(4));
  REQUIRE(beat(6));
  REQUIRE_FALSE(beat(1));
  REQUIRE_FALSE(beat(3));
  REQUIRE_FALSE(beat(5));
  REQUIRE_FALSE(beat(7));
}

TEST_CASE("from_pattern is periodic", "[signal]") {
  auto beat = Gate::from_pattern("x.x.");
  REQUIRE(beat.to_pattern(8) == "x.x.x.x.");
}

TEST_CASE("operator* (AND)", "[signal]") {
  auto lhs = Gate::from_pattern("x.x.");
  auto rhs = Gate::from_pattern("xx..");
  REQUIRE((lhs * rhs).to_pattern(4) == "x...");
}

TEST_CASE("operator+ (OR) same period", "[signal]") {
  auto lhs = Gate::from_pattern("x...");
  auto rhs = Gate::from_pattern("...x");
  REQUIRE((lhs + rhs).to_pattern(4) == "x..x");
}

TEST_CASE("operator+ (OR) different periods", "[signal]") {
  auto lhs = Gate::from_pattern("x...");    // period 4: hits at 0, 4, 8, ...
  auto rhs = Gate::from_pattern("...x..");  // period 6: hits at 3, 9, ...
  auto result = lhs + rhs;
  REQUIRE(result(0));
  REQUIRE_FALSE(result(1));
  REQUIRE_FALSE(result(2));
  REQUIRE(result(3));
  REQUIRE(result(4));
  REQUIRE(result(9));
}

TEST_CASE("operator~ (NOT)", "[signal]") {
  auto sig = Gate::from_pattern("x.x.");
  REQUIRE((~sig).to_pattern(4) == ".x.x");
}

TEST_CASE("operator- (AND NOT) same period", "[signal]") {
  auto lhs = Gate::from_pattern("xxxx");
  auto rhs = Gate::from_pattern("x.x.");
  REQUIRE((lhs - rhs).to_pattern(4) == ".x.x");
}

TEST_CASE("operator- (AND NOT) different periods", "[signal]") {
  auto lhs = Gate::from_pattern("xxxx");
  auto rhs = Gate::from_pattern("x.x.x.");
  auto result = lhs - rhs;
  REQUIRE_FALSE(result(0));
  REQUIRE(result(1));
  REQUIRE_FALSE(result(2));
  REQUIRE(result(3));
}

TEST_CASE("operator^ (XOR)", "[signal]") {
  auto lhs = Gate::from_pattern("xx..");
  auto rhs = Gate::from_pattern("x.x.");
  REQUIRE((lhs ^ rhs).to_pattern(4) == ".xx.");
}

TEST_CASE("rotation right", "[signal]") {
  auto beat = Gate::from_pattern("x...");
  REQUIRE((beat >> 1).to_pattern(4) == ".x..");
  REQUIRE((beat >> 2).to_pattern(4) == "..x.");
  REQUIRE((beat >> 3).to_pattern(4) == "...x");
  REQUIRE((beat >> 4).to_pattern(4) == "x...");
}

TEST_CASE("rotation left", "[signal]") {
  auto beat = Gate::from_pattern("x...");
  REQUIRE((beat << 1).to_pattern(4) == "...x");
  REQUIRE((beat << 2).to_pattern(4) == "..x.");
  REQUIRE((beat << 3).to_pattern(4) == ".x..");
  REQUIRE((beat << 4).to_pattern(4) == "x...");
}

TEST_CASE("stretch spaces hits by factor", "[signal]") {
  auto original = Gate::from_pattern("xxxx");
  REQUIRE((original.stretch(2)).to_pattern(8) == "x.x.x.x.");
  REQUIRE((original.stretch(4)).to_pattern(16) == "x...x...x...x...");
}

TEST_CASE("compress takes every factor-th step", "[signal]") {
  REQUIRE((Gate::from_pattern("x.x.x.x.").compress(2)).to_pattern(4) == "xxxx");
}

TEST_CASE("compress with non-divisor factor", "[signal]") {
  REQUIRE((Gate::from_pattern("x.......").compress(3)).to_pattern(3) == "x..");
}

TEST_CASE("compress discards intermediate steps", "[signal]") {
  REQUIRE((Gate::from_pattern(".x.x").compress(2)).to_pattern(2) == "..");
}

TEST_CASE("stretch then compress is identity", "[signal]") {
  auto original = Gate::from_pattern("x.x.");
  REQUIRE((original.stretch(2).compress(2)).to_pattern(4) == "x.x.");
}

TEST_CASE("compress then stretch is lossy", "[signal]") {
  auto original = Gate::from_pattern("xxx.");
  REQUIRE(original.compress(2).stretch(2).to_pattern(4) !=
          original.to_pattern(4));
}

TEST_CASE("euclid(3) on 8 steps produces tresillo", "[signal][euclid]") {
  REQUIRE((Gate::euclid(3, 8)).to_pattern(8) == "x..x.x..");
}

TEST_CASE("euclid(4) on 16 steps produces four on the floor",
          "[signal][euclid]") {
  REQUIRE((Gate::euclid(4, 16)).to_pattern(16) == "x...x...x...x...");
}

TEST_CASE("euclid(5) on 8 steps produces son clave", "[signal][euclid]") {
  REQUIRE((Gate::euclid(5, 8)).to_pattern(8) == "x.xx.xx.");
}

TEST_CASE("euclid hit count equals pulse count", "[signal][euclid]") {
  REQUIRE(count_hits(Gate::euclid(5, 16), 16) == 5);
  REQUIRE(count_hits(Gate::euclid(7, 12), 12) == 7);
}

TEST_CASE("count fires on every k-th hit", "[signal]") {
  REQUIRE((Gate::from_pattern("x...x.xx").count(2)).to_pattern(8) ==
          "....x..x");
  REQUIRE((Gate::from_pattern("x.x.x.x.").count(2)).to_pattern(8) ==
          "..x...x.");
}

TEST_CASE("gate to trigger emits pulse only on rising edge",
          "[signal][trigger]") {
  auto gate = Gate::from_pattern("x...xx..");
  REQUIRE((gate.trigger()).to_pattern(8) == "x...x...");
}

TEST_CASE("gate to trigger cyclic: wraps around end of pattern",
          "[signal][trigger]") {
  auto gate = Gate::from_pattern("x..x");
  REQUIRE((gate.trigger()).to_pattern(4) == "...x");
}
