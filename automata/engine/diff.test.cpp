#include <catch2/catch_test_macros.hpp>

#include "automata/engine/diff.hpp"
#include "automata/engine/graph.hpp"
#include "automata/ugens/ugens.hpp"

#include <cstring>

using namespace automata;

TEST_CASE("reordered statements pair by structure, not flat ordinal",
          "[engine][diff]") {
  // Same graph, edited values, statements swapped: every Const shares one
  // structural hash, so only positional descent routes the values home.
  const auto running = describe([](GraphBuilder& g) {
    auto a = sine(440.f);
    auto b = saw(220.f);
    g.out(a, b);
  });
  const auto incoming = describe([](GraphBuilder& g) {
    auto b = saw(330.f);
    auto a = sine(550.f);
    g.out(a, b);
  });
  REQUIRE(running.def_hash == incoming.def_hash);

  const auto values = remap_values(running, incoming);
  // Running layout: sine's consts first (created first), then saw's.
  REQUIRE(values[0] == 550.f);
  REQUIRE(values[2] == 330.f);
}

TEST_CASE("a wrapped output still matches its inner subtree",
          "[engine][diff]") {
  const auto plain = describe([](GraphBuilder& g) {
    auto s = sine(440.f);
    g.out(s, s);
  });
  const auto wrapped = describe([](GraphBuilder& g) {
    auto s = soft_clip(sine(440.f));
    g.out(s, s);
  });

  const auto matches = match_nodes(plain, wrapped);
  // The sine node (last of plain's four) must find its counterpart even
  // though the roots differ.
  const std::uint32_t sine_index = plain.outs[0];
  REQUIRE(matches[sine_index] != NoMatch);
  REQUIRE(wrapped.nodes[matches[sine_index]].structural_hash ==
          plain.nodes[sine_index].structural_hash);
}

TEST_CASE("transfer plan copies matched state across graphs",
          "[engine][diff]") {
  const auto def = describe([](GraphBuilder& g) {
    auto s = sine(375.f);
    g.out(s, s);
  });

  Graph from(def);
  Graph to(def);
  for (int i = 0; i < 7; ++i) {
    from.process_block();
  }

  TransferPlan plan = plan_transfer(from, to);
  REQUIRE(!plan.copies.empty());
  REQUIRE(plan.total_bytes > 0);
  for (const auto& copy : plan.copies) {
    std::memcpy(copy.dst, copy.src, copy.size);
  }

  from.process_block();
  to.process_block();
  const auto a = from.left();
  const auto b = to.left();
  for (std::size_t i = 0; i < BlockSize; ++i) {
    REQUIRE(a[i] == b[i]);
  }
}
