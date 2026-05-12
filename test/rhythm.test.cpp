#include <cstddef>
// #include <algorithm>

#include <rhythm.hpp>
#include <math.hpp>

#include <catch2/catch_test_macros.hpp>

using automata::lcm;
using automata::Rhythm;
// using automata::interleave;

TEST_CASE("noneness", "[rhythm]") {
  STATIC_REQUIRE(Rhythm<8>{}.none());
  STATIC_REQUIRE_FALSE(Rhythm<8>{"x......."}.none());
  STATIC_REQUIRE_FALSE(Rhythm<8>{"x.x.x.x."}.none());
}

TEST_CASE("default constructed rhythm is none", "[rhythm]") {
  constexpr Rhythm<4> beat;
  STATIC_REQUIRE(beat.none());
  STATIC_REQUIRE_FALSE(beat[0]);
  STATIC_REQUIRE_FALSE(beat[1]);
  STATIC_REQUIRE_FALSE(beat[2]);
  STATIC_REQUIRE_FALSE(beat[3]);
}

TEST_CASE("rhythm length", "[rhythm]") {
  constexpr Rhythm<1> beat_1;
  constexpr Rhythm<2> beat_2;
  constexpr Rhythm<8> beat_8;
  STATIC_REQUIRE(beat_1.length() == 1);
  STATIC_REQUIRE(beat_2.length() == 2);
  STATIC_REQUIRE(beat_8.length() == 8);
}

TEST_CASE("string constructor", "[rhythm]") {
  constexpr Rhythm<8> beat{"x.x.x.x."};
  STATIC_REQUIRE(beat[0]);
  STATIC_REQUIRE(beat[2]);
  STATIC_REQUIRE(beat[4]);
  STATIC_REQUIRE(beat[6]);
  STATIC_REQUIRE_FALSE(beat[1]);
  STATIC_REQUIRE_FALSE(beat[3]);
  STATIC_REQUIRE_FALSE(beat[5]);
  STATIC_REQUIRE_FALSE(beat[7]);
}

TEST_CASE("integer constructor", "[rhythm]") {
  constexpr Rhythm<4> beat(0b1010);
  STATIC_REQUIRE(beat[1]);
  STATIC_REQUIRE(beat[3]);
  STATIC_REQUIRE_FALSE(beat[0]);
  STATIC_REQUIRE_FALSE(beat[2]);
}

TEST_CASE("equality", "[rhythm]") {
  constexpr Rhythm<8> beat_1{"x.x.x.x."};
  constexpr Rhythm<8> beat_2{0b01010101};
  STATIC_REQUIRE(beat_1 == beat_2);
}

TEST_CASE("zero padding", "[rhythm]") {
  constexpr Rhythm<4> short_beat{"x.x."};
  constexpr Rhythm<8> long_beat = short_beat;
  STATIC_REQUIRE(long_beat == Rhythm{"x.x....."});
}

TEST_CASE("truncation", "[rhythm]") {
  constexpr Rhythm<8> long_beat{"x.x.x.x."};
  constexpr Rhythm<4> short_beat = long_beat;
  STATIC_REQUIRE(short_beat == Rhythm{"x.x."});
}

TEST_CASE("tiling", "[rhythm]") {
  constexpr Rhythm<4> short_beat{"x.x."};
  constexpr auto long_beat = short_beat.tile<8>();
  STATIC_REQUIRE(long_beat == Rhythm{"x.x.x.x."});
}

TEST_CASE("binary operator of same length)", "[rhythm]") {
  constexpr Rhythm<4> lhs("x.x.");
  constexpr Rhythm<4> rhs("xx..");
  constexpr auto result = lhs & rhs;
  STATIC_REQUIRE(result.length() == 4);
  STATIC_REQUIRE(result == Rhythm{"x..."});
}

TEST_CASE("binary operator of different length auto expand to lcm",
          "[rhythm]") {
  constexpr Rhythm<4> lhs("x.x.");
  constexpr Rhythm<9> rhs = Rhythm{"x..."}.tile<9>();
  constexpr auto result = lhs & rhs;
  STATIC_REQUIRE(result.length() == lcm(4, 9));
}

TEST_CASE("operator| different length", "[rhythm]") {
  constexpr Rhythm<4> lhs{"x..."};
  constexpr Rhythm<6> rhs{"...x.."};
  constexpr auto result = lhs | rhs;
  STATIC_REQUIRE(result.length() == lcm(4, 6));
  STATIC_REQUIRE(result[0]);
  STATIC_REQUIRE(result[3]);
  STATIC_REQUIRE(result[4]);
  STATIC_REQUIRE(result[9]);
  STATIC_REQUIRE_FALSE(result[1]);
  STATIC_REQUIRE_FALSE(result[2]);
}

TEST_CASE("operator^ different length expands to lcm", "[rhythm]") {
  constexpr Rhythm<4> lhs{"x.x."};
  constexpr Rhythm<6> rhs{"x.x.x."};
  constexpr auto result = lhs ^ rhs;
  STATIC_REQUIRE(result.length() == lcm(4, 6));
}

TEST_CASE("operator- different length expands to lcm", "[rhythm]") {
  constexpr Rhythm<4> lhs{"xxxx"};
  constexpr Rhythm<6> rhs{"x.x.x."};
  constexpr auto result = lhs - rhs;
  STATIC_REQUIRE(result.length() == lcm(4, 6));
  STATIC_REQUIRE_FALSE(result[0]);
  STATIC_REQUIRE(result[1]);
  STATIC_REQUIRE_FALSE(result[2]);
  STATIC_REQUIRE(result[3]);
}

TEST_CASE("rotation", "[rhythm]") {
  constexpr Rhythm<4> beat("x...");

  constexpr auto left_1 = beat << 1;
  STATIC_REQUIRE(left_1 == Rhythm{"...x"});
  constexpr auto left_2 = beat << 2;
  STATIC_REQUIRE(left_2 == Rhythm{"..x."});
  constexpr auto left_3 = beat << 3;
  STATIC_REQUIRE(left_3 == Rhythm{".x.."});
  constexpr auto left_4 = beat << 4;
  STATIC_REQUIRE(left_4 == beat);

  constexpr auto right_1 = beat >> 1;
  STATIC_REQUIRE(right_1 == Rhythm{".x.."});
  constexpr auto right_2 = beat >> 2;
  STATIC_REQUIRE(right_2 == Rhythm{"..x."});
  constexpr auto right_3 = beat >> 3;
  STATIC_REQUIRE(right_3 == Rhythm{"...x"});
  constexpr auto right_4 = beat >> 4;
  STATIC_REQUIRE(right_4 == beat);
}

TEST_CASE("concatenation", "[rhythm]") {
  constexpr Rhythm<2> first("x.");
  constexpr Rhythm<2> second(".x");
  constexpr auto cat = first + second;
  STATIC_REQUIRE(cat.length() == 4);
  STATIC_REQUIRE(cat == Rhythm{"x..x"});
}

TEST_CASE("dac", "[rhythm]") {
  constexpr Rhythm<4> beat("x.x.");
  constexpr auto val = beat.dac<4>();
  STATIC_REQUIRE(val == 0b0101);
  constexpr Rhythm<8> beat("x.x.");
}

// TEST_CASE("interleave", "[rhythm]") {
//   constexpr Rhythm<2> aa("x.");
//   constexpr Rhythm<2> bb(".x");
//   constexpr auto zipped = interleave(aa, bb);

//   static_assert(zipped.steps() == 4);
//   // result[j*2 + chan]: aa[0]=x at pos 0, bb[0]=. at pos 1, aa[1]=. at pos
//   2, bb[1]=x at pos 3 REQUIRE(zipped[0]); REQUIRE_FALSE(zipped[1]);
//   REQUIRE_FALSE(zipped[2]);
//   REQUIRE(zipped[3]);
// }

