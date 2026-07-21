#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "automata/engine/graph.hpp"
#include "automata/ugens/ugens.hpp"

#include <cmath>

using namespace automata;
using Catch::Approx;

TEST_CASE("a const renders its value", "[engine][graph]") {
  Graph graph(describe([](GraphBuilder& g) {
    Signal level = 0.5f;
    g.out(level, level);
  }));

  graph.process_block();

  for (const float sample : graph.left()) {
    REQUIRE(sample == 0.5f);
  }
}

TEST_CASE("signal arithmetic renders per sample", "[engine][graph]") {
  Graph graph(describe([](GraphBuilder& g) {
    auto s = (Signal{0.25f} + Signal{0.5f}) * Signal{2.f};
    g.out(s, -s);
  }));

  graph.process_block();

  REQUIRE(graph.left()[0] == Approx(1.5f));
  REQUIRE(graph.right()[0] == Approx(-1.5f));
}

TEST_CASE("sine renders at the requested frequency", "[engine][graph]") {
  // 375 Hz at 48 kHz = one cycle per 128-sample block.
  Graph graph(describe([](GraphBuilder& g) {
    auto s = sine(375.f);
    g.out(s, s);
  }));

  graph.process_block();
  const auto block = graph.left();

  REQUIRE(block[0] == Approx(0.f).margin(1e-5));
  REQUIRE(block[32] == Approx(1.f).margin(1e-4));   // quarter cycle
  REQUIRE(block[96] == Approx(-1.f).margin(1e-4));  // three quarters
}

TEST_CASE("audio-rate modulation reaches bound setters every sample",
          "[engine][graph]") {
  // A phasor whose frequency is another phasor: the setter must see a new
  // control sample each frame, so the output curve is quadratic, not linear.
  Graph graph(describe([](GraphBuilder& g) {
    auto ramp = phasor(375.f);
    Signal swept =
        make_node<Phasor>().control(&Phasor::set_freq, ramp * (1.f / 128.f));
    g.out(swept, swept);
  }));

  graph.process_block();
  const auto block = graph.left();

  // Phase accumulates sum(i/128 * 1/128) ≈ quadratic growth.
  const float mid = block[64];
  const float end = block[127];
  REQUIRE(mid > 0.f);
  REQUIRE(end / mid > 3.f);  // convex, so far more than the linear 2x
}

TEST_CASE("the selector picks the svf member without changing identity",
          "[engine][graph]") {
  const auto make = [](Signal (*ugen)(Signal, Signal, Signal)) {
    return describe([&](GraphBuilder& g) {
      auto s = ugen(Signal{1.f}, Signal{1000.f}, Signal{0.707f});
      g.out(s, s);
    });
  };
  GraphDef lp_def = make(&svf_lp);
  GraphDef hp_def = make(&svf_hp);
  REQUIRE(lp_def.def_hash == hp_def.def_hash);

  Graph lp(std::move(lp_def));
  Graph hp(std::move(hp_def));
  for (int i = 0; i < 40; ++i) {  // let the DC step settle
    lp.process_block();
    hp.process_block();
  }

  REQUIRE(lp.left().back() == Approx(1.f).margin(1e-3));
  REQUIRE(hp.left().back() == Approx(0.f).margin(1e-3));
}

TEST_CASE("metro drives an ar envelope", "[engine][graph]") {
  Graph graph(describe([](GraphBuilder& g) {
    auto env = ar(metro(2.f), 0.005f, 0.12f);
    g.out(env, env);
  }));

  float peak = 0.f;
  bool in_bounds = true;
  for (int b = 0; b < 375; ++b) {  // one second
    graph.process_block();
    for (const float sample : graph.left()) {
      in_bounds = in_bounds && sample >= 0.f && sample <= 1.f;
      peak = std::max(peak, sample);
    }
  }
  REQUIRE(in_bounds);
  REQUIRE(peak == Approx(1.f));
}

TEST_CASE("fn wraps a captureless function into a stateless node",
          "[engine][graph]") {
  Graph graph(describe([](GraphBuilder& g) {
    const Signal halved = fn("half", [](float x) { return x * 0.5f; }, 0.8f);
    const Signal mixed =
        fn("mix", [](float a, float b) { return a + 2.f * b; }, 0.25f, 0.5f);
    g.out(halved, mixed);
  }));

  graph.process_block();

  REQUIRE(graph.left()[0] == Approx(0.4f));
  REQUIRE(graph.right()[0] == Approx(1.25f));
}

TEST_CASE("fn identity is its name, not its body", "[engine][graph]") {
  const auto def_with = [](std::string_view name, float (*f)(float)) {
    return describe([&](GraphBuilder& g) {
      const Signal s = fn(name, f, 0.5f);
      g.out(s, s);
    });
  };
  REQUIRE(def_with(
              "gain", +[](float x) { return x * 2.f; })
              .def_hash ==
          def_with("gain", +[](float x) { return x * 3.f; }).def_hash);
  REQUIRE(def_with(
              "gain", +[](float x) { return x * 2.f; })
              .def_hash !=
          def_with("drive", +[](float x) { return x * 2.f; }).def_hash);
}
