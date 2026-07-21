#include <catch2/catch_test_macros.hpp>

#include "automata/graph/schedule.hpp"
#include "automata/ugens/ugens.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

using namespace automata;

namespace {

std::vector<std::uint32_t> nodes_of(const GraphDef& def, const KernelInfo* k) {
  std::vector<std::uint32_t> found;
  for (std::uint32_t i = 0; i < def.nodes.size(); ++i) {
    if (def.nodes[i].kernel == k) {
      found.push_back(i);
    }
  }
  return found;
}

std::vector<std::uint32_t> island_members(const detail::Schedule& s) {
  std::vector<std::uint32_t> members;
  for (const auto& seg : s.segments) {
    if (seg.island) {
      for (std::uint32_t k = seg.begin; k < seg.end; ++k) {
        members.push_back(s.order[k]);
      }
    }
  }
  return members;
}

bool contains(const std::vector<std::uint32_t>& v, std::uint32_t x) {
  return std::find(v.begin(), v.end(), x) != v.end();
}

}  // namespace

TEST_CASE("a pure DAG schedules as one block segment in def order",
          "[graph][schedule]") {
  const auto def = describe([](GraphBuilder& g) {
    auto s = sine(440.f) * 0.5f;
    g.out(s, s);
  });
  const auto sched = detail::compute_schedule(def);

  REQUIRE_FALSE(sched.whole_graph_island);
  REQUIRE(sched.segments.size() == 1);
  REQUIRE_FALSE(sched.segments[0].island);
  REQUIRE(sched.order.size() == def.nodes.size());
  for (std::uint32_t i = 0; i < sched.order.size(); ++i) {
    REQUIRE(sched.order[i] == i);
  }
}

TEST_CASE("a cycle through a long Const delay stays block-mode",
          "[graph][schedule]") {
  const auto def = describe([](GraphBuilder& g) {
    auto fb = g.tap("fb");
    auto wet = metro(1.f) + fb.read(0.25f) * 0.5f;
    fb.write(wet);
    g.out(wet, wet);
  });
  const auto sched = detail::compute_schedule(def);

  REQUIRE(island_members(sched).empty());
  const auto reads = nodes_of(def, &detail::TapReadInfo);
  REQUIRE(reads.size() == 1);
  REQUIRE(sched.read_modes[reads[0]] == detail::TapReadMode::PreWrite);
}

TEST_CASE("a sub-block cycle becomes a sample-serial island of its own nodes",
          "[graph][schedule]") {
  const auto def = describe([](GraphBuilder& g) {
    auto fb = g.tap("fb");
    auto wet = metro(1.f) + fb.read(0.001f) * 0.5f;
    fb.write(wet);
    g.out(wet, wet);
  });
  const auto sched = detail::compute_schedule(def);

  REQUIRE_FALSE(sched.whole_graph_island);
  const auto members = island_members(sched);
  const auto reads = nodes_of(def, &detail::TapReadInfo);
  const auto writes = nodes_of(def, &detail::TapWriteInfo);
  // Exactly the cycle: read -> mul -> add -> write. Feeders (metro, consts)
  // stay block-mode.
  REQUIRE(members.size() == 4);
  REQUIRE(contains(members, reads[0]));
  REQUIRE(contains(members, writes[0]));
  REQUIRE(sched.read_modes[reads[0]] == detail::TapReadMode::Island);
  // The write emits last within the sample.
  REQUIRE(members.back() == writes[0]);
}

TEST_CASE("a modulated delay islands conservatively", "[graph][schedule]") {
  const auto def = describe([](GraphBuilder& g) {
    auto fb = g.tap("fb");
    auto wet = metro(1.f) + fb.read(param("d", 0.5f)) * 0.5f;
    fb.write(wet);
    g.out(wet, wet);
  });
  const auto sched = detail::compute_schedule(def);

  const auto reads = nodes_of(def, &detail::TapReadInfo);
  REQUIRE(sched.read_modes[reads[0]] == detail::TapReadMode::Island);
}

TEST_CASE("a tap outside any cycle reads after its write",
          "[graph][schedule]") {
  const auto def = describe([](GraphBuilder& g) {
    auto d = g.tap("d");
    d.write(metro(1.f));
    auto y = d.read(0.001f);
    g.out(y, y);
  });
  const auto sched = detail::compute_schedule(def);

  REQUIRE(island_members(sched).empty());
  const auto reads = nodes_of(def, &detail::TapReadInfo);
  const auto writes = nodes_of(def, &detail::TapWriteInfo);
  REQUIRE(sched.read_modes[reads[0]] == detail::TapReadMode::PostWrite);
  const auto pos = [&](std::uint32_t v) {
    return std::find(sched.order.begin(), sched.order.end(), v) -
           sched.order.begin();
  };
  REQUIRE(pos(writes[0]) < pos(reads[0]));
}

TEST_CASE("an island reading an outside tap folds the write in",
          "[graph][schedule]") {
  const auto def = describe([](GraphBuilder& g) {
    auto dry = g.tap("dry");
    dry.write(metro(1.f));
    auto fb = g.tap("fb");
    // The dry read's delay is modulated by the cycle, pulling it into the
    // island; its write has no cycle of its own.
    auto wet = fb.read() * 0.5f + dry.read(fb.read() * 0.001f);
    fb.write(wet);
    g.out(wet, wet);
  });
  const auto sched = detail::compute_schedule(def);

  REQUIRE_FALSE(sched.whole_graph_island);
  const auto members = island_members(sched);
  const auto writes = nodes_of(def, &detail::TapWriteInfo);
  REQUIRE(writes.size() == 2);
  REQUIRE(contains(members, writes[0]));
  REQUIRE(contains(members, writes[1]));
  for (const std::uint32_t r : nodes_of(def, &detail::TapReadInfo)) {
    REQUIRE(sched.read_modes[r] == detail::TapReadMode::Island);
  }
  // Writes emit last within the sample.
  REQUIRE(contains({members.end()[-1], members.end()[-2]}, writes[0]));
  REQUIRE(contains({members.end()[-1], members.end()[-2]}, writes[1]));
}

TEST_CASE("two islands sharing a tap fall back to a whole-graph island",
          "[graph][schedule]") {
  const auto def = describe([](GraphBuilder& g) {
    auto shared = g.tap("shared");
    shared.write(metro(1.f));
    auto a = g.tap("a");
    auto wa = a.read() * 0.3f + shared.read(a.read() * 0.001f);
    a.write(wa);
    auto b = g.tap("b");
    auto wb = b.read() * 0.3f + shared.read(b.read() * 0.001f);
    b.write(wb);
    g.out(wa, wb);
  });
  const auto sched = detail::compute_schedule(def);

  REQUIRE(sched.whole_graph_island);
  REQUIRE(sched.segments.size() == 1);
  REQUIRE(sched.segments[0].island);
  REQUIRE(sched.segments[0].end == def.nodes.size());
  for (const std::uint32_t r : nodes_of(def, &detail::TapReadInfo)) {
    REQUIRE(sched.read_modes[r] == detail::TapReadMode::Island);
  }
  // All three writes emit last.
  const auto writes = nodes_of(def, &detail::TapWriteInfo);
  for (std::size_t k = 0; k < writes.size(); ++k) {
    REQUIRE(contains(writes, sched.order[sched.order.size() - 1 - k]));
  }
}

TEST_CASE("schedule equivalence flags a threshold-crossing delay edit",
          "[graph][schedule]") {
  const auto echo = [](float delay) {
    return describe([&](GraphBuilder& g) {
      auto fb = g.tap("fb");
      auto wet = metro(1.f) + fb.read(Signal{delay}) * 0.5f;
      fb.write(wet);
      g.out(wet, wet);
    });
  };
  REQUIRE(echo(0.25f).def_hash == echo(0.002f).def_hash);

  REQUIRE(detail::schedule_equivalent(echo(0.25f), echo(0.5f)));
  REQUIRE(detail::schedule_equivalent(echo(0.001f), echo(0.002f)));
  REQUIRE_FALSE(detail::schedule_equivalent(echo(0.25f), echo(0.002f)));
}
