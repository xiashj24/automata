#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "automata/kernel/svf.hpp"

#include <cstring>
#include <type_traits>

using Catch::Approx;

static_assert(std::is_trivially_copyable_v<automata::Svf>);
static_assert(std::is_standard_layout_v<automata::Svf::Out>);
static_assert(sizeof(automata::Svf::Out) == 3 * sizeof(float));

TEST_CASE("lowpass passes DC, highpass blocks it", "[kernel][svf]") {
  automata::Svf f;
  f.update_coeffs(1000.f / 48000.f, 0.707f);

  automata::Svf::Out out{};
  for (int i = 0; i < 4800; ++i) {
    out = f.process(1.f);
  }

  REQUIRE(out.lp == Approx(1.f).margin(1e-3));
  REQUIRE(out.hp == Approx(0.f).margin(1e-3));
  REQUIRE(out.bp == Approx(0.f).margin(1e-3));
}

TEST_CASE("state transfer is a whole-object copy", "[kernel][svf]") {
  automata::Svf a;
  a.update_coeffs(2000.f / 48000.f, 2.f);
  for (int i = 0; i < 100; ++i) {
    (void)a.process(i % 7 == 0 ? 1.f : -0.25f);
  }

  automata::Svf b;
  std::memcpy(&b, &a, sizeof(automata::Svf));  // the ADR 0002 transfer

  for (int i = 0; i < 100; ++i) {
    const float in = i % 5 == 0 ? -1.f : 0.5f;
    const auto oa = a.process(in);
    const auto ob = b.process(in);
    REQUIRE(oa.lp == ob.lp);
    REQUIRE(oa.bp == ob.bp);
    REQUIRE(oa.hp == ob.hp);
  }
}

TEST_CASE("reset clears state", "[kernel][svf]") {
  automata::Svf f;
  f.update_coeffs(500.f / 48000.f, 0.707f);
  for (int i = 0; i < 32; ++i) {
    (void)f.process(1.f);
  }

  f.reset();
  f.update_coeffs(500.f / 48000.f, 0.707f);

  automata::Svf fresh;
  fresh.update_coeffs(500.f / 48000.f, 0.707f);
  const auto a = f.process(1.f);
  const auto b = fresh.process(1.f);
  REQUIRE(a.lp == b.lp);
  REQUIRE(a.bp == b.bp);
  REQUIRE(a.hp == b.hp);
}
