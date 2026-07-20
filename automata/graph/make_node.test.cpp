#include <catch2/catch_test_macros.hpp>

#include "automata/ugens/ugens.hpp"

#include <type_traits>

using namespace automata;

// The MSVC hazard (ADR 0005): noexcept is part of the deduced member-pointer
// type, so both flavors must sniff identically.
namespace {
struct NxProbe {
  float process(float a, float b) noexcept;
  void reset() noexcept;
  void set_x(float) noexcept;
};
struct PlainProbe {
  float process(float a, float b);
  void reset();
  void set_x(float);
};
}  // namespace

static_assert(detail::MemFnTraits<decltype(&NxProbe::process)>::Arity == 2);
static_assert(detail::MemFnTraits<decltype(&PlainProbe::process)>::Arity == 2);
static_assert(detail::MemFnTraits<decltype(&NxProbe::set_x)>::AllFloatParams);
static_assert(
    std::is_same_v<detail::MemFnTraits<decltype(&NxProbe::process)>::Return,
                   float>);
static_assert(detail::MemFnTraits<decltype(&Svf::update_coeffs)>::Arity == 2);
static_assert(detail::OutputTraits<Svf::Out>::Count == 3);
static_assert(detail::OutputTraits<float>::Count == 1);
static_assert(std::is_trivially_copyable_v<
              detail::SetterPack<decltype(&Ar::set_attack),
                                 decltype(&Ar::set_release)>>);
static_assert(detail::TotalControlInputs<decltype(&Svf::update_coeffs),
                                         decltype(&Ar::set_attack)> == 3);

TEST_CASE("value edits do not change the def hash", "[graph][identity]") {
  const GraphDef a = describe([](GraphBuilder& g) {
    auto s = sine(440.f);
    g.out(s, s);
  });
  const GraphDef b = describe([](GraphBuilder& g) {
    auto s = sine(880.f);
    g.out(s, s);
  });
  REQUIRE(a.def_hash == b.def_hash);
}

TEST_CASE("structural edits change the def hash", "[graph][identity]") {
  const GraphDef a = describe([](GraphBuilder& g) {
    auto s = sine(440.f);
    g.out(s, s);
  });
  const GraphDef b = describe([](GraphBuilder& g) {
    auto s = saw(440.f);
    g.out(s, s);
  });
  const GraphDef c = describe([](GraphBuilder& g) {
    auto s = soft_clip(sine(440.f));
    g.out(s, s);
  });
  REQUIRE(a.def_hash != b.def_hash);
  REQUIRE(a.def_hash != c.def_hash);
}

TEST_CASE("the output selector is a value, not identity", "[graph][identity]") {
  const auto lp = describe([](GraphBuilder& g) {
    auto s = svf_lp(saw(110.f), 800.f, 0.7f);
    g.out(s, s);
  });
  const auto bp = describe([](GraphBuilder& g) {
    auto s = svf_bp(saw(110.f), 800.f, 0.7f);
    g.out(s, s);
  });
  REQUIRE(lp.def_hash == bp.def_hash);
  REQUIRE(lp.values != bp.values);  // the selector rides the values table
}

TEST_CASE("sharing is author-expressed: one Signal is one node",
          "[graph][identity]") {
  const auto shared = describe([](GraphBuilder& g) {
    auto s = sine(440.f);
    g.out(s, s);
  });
  const auto twice =
      describe([](GraphBuilder& g) { g.out(sine(440.f), sine(440.f)); });
  REQUIRE(twice.nodes.size() == 2 * shared.nodes.size());
}

TEST_CASE("statement order of independent chains does not change the def hash",
          "[graph][identity]") {
  const auto ab = describe([](GraphBuilder& g) {
    auto a = sine(440.f);
    auto b = saw(220.f);
    g.out(a, b);
  });
  const auto ba = describe([](GraphBuilder& g) {
    auto b = saw(220.f);
    auto a = sine(440.f);
    g.out(a, b);
  });
  REQUIRE(ab.def_hash == ba.def_hash);
}

TEST_CASE("unreachable nodes do not affect the def hash", "[graph][identity]") {
  const auto lean = describe([](GraphBuilder& g) {
    auto a = sine(440.f);
    g.out(a, a);
  });
  const auto noisy = describe([](GraphBuilder& g) {
    auto a = sine(440.f);
    (void)saw(97.f);  // dead code in the patch
    g.out(a, a);
  });
  REQUIRE(lean.def_hash == noisy.def_hash);
}

TEST_CASE("floats lift to Const nodes with the value patchable",
          "[graph][identity]") {
  const auto def = describe([](GraphBuilder& g) {
    Signal level = 0.5f;
    g.out(level, level);
  });
  REQUIRE(def.nodes.size() == 1);
  REQUIRE(def.values.size() == 1);
  REQUIRE(def.values[0] == 0.5f);
}

TEST_CASE("equal-hash siblings stay distinct nodes", "[graph][identity]") {
  const auto def =
      describe([](GraphBuilder& g) { g.out(sine(440.f), sine(440.f)); });
  const auto& left = def.nodes[def.outs[0]];
  const auto& right = def.nodes[def.outs[1]];
  REQUIRE(def.outs[0] != def.outs[1]);
  REQUIRE(left.structural_hash == right.structural_hash);  // ordinal-paired
}
