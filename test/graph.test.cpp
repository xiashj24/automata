#include <graph.hpp>

#include <array>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using automata::Graph;
using Catch::Approx;

TEST_CASE("Graph: output_count and param_count reflect build phase",
          "[graph]") {
  Graph g;
  REQUIRE(g.output_count() == 0);
  REQUIRE(g.param_count() == 0);

  g.make_param("cutoff", 440.f, 20.f, 20000.f);
  REQUIRE(g.param_count() == 1);

  g.add_output("main", 0.5f);
  REQUIRE(g.output_count() == 1);
}

TEST_CASE("Graph: render produces expected samples for constant output",
          "[graph]") {
  Graph g;
  g.add_output("main", 0.25f);

  std::array<float, 4> buf{};
  g.render(buf);

  for (float v : buf)
    REQUIRE(v == Approx(0.25f));
}

TEST_CASE("Graph: render sums multiple outputs", "[graph]") {
  Graph g;
  g.add_output("a", 0.3f);
  g.add_output("b", 0.2f);

  std::array<float, 4> buf{};
  g.render(buf);

  for (float v : buf)
    REQUIRE(v == Approx(0.5f));
}

TEST_CASE("Graph: lkg returns last rendered sample", "[graph]") {
  Graph g;
  g.add_output("main", 0.7f);

  std::array<float, 8> buf{};
  g.render(buf);

  REQUIRE(g.lkg(0) == Approx(0.7f));
}

TEST_CASE("Graph: lkg out-of-range returns 0", "[graph]") {
  Graph g;
  REQUIRE(g.lkg(99) == Approx(0.f));
}

TEST_CASE("Graph: Param drives output via implicit Stream conversion",
          "[graph]") {
  Graph g;
  auto& gain = g.make_param("gain", 0.5f, 0.f, 1.f);
  g.add_output("main", automata::Stream(gain) * 1.f);

  std::array<float, 2> buf{};
  g.render(buf);
  REQUIRE(buf[0] == Approx(0.5f));

  gain.set(0.8f);
  g.render(buf);
  REQUIRE(buf[0] == Approx(0.8f));
}

TEST_CASE("Graph: render_multi fills separate buffers per output", "[graph]") {
  Graph g;
  g.add_output("cv1", 0.1f);
  g.add_output("cv2", 0.9f);

  std::array<float, 4> buf1{}, buf2{};
  g.render_multi({buf1, buf2});

  for (float v : buf1)
    REQUIRE(v == Approx(0.1f));
  for (float v : buf2)
    REQUIRE(v == Approx(0.9f));
}
