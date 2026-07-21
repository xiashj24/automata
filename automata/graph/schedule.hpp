#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <utility>
#include <vector>

#include "automata/config.hpp"
#include "automata/core/assert.hpp"
#include "automata/graph/builder.hpp"
#include "automata/graph/def.hpp"
#include "automata/graph/tap.hpp"

// Build-time execution schedule (ADR 0004/0011). Tarjan SCCs over the def's
// input edges plus one synthetic write->read dependency per tap make
// feedback cycles visible: a cycle whose every in-cycle delay is a Const of
// at least one block stays block-mode; anything tighter or modulated becomes
// a sample-serial island. The def's node order is never rewritten — identity
// and state transfer key off it (ADR 0002).

namespace automata::detail {

// How a tap read compensates for its position in the schedule.
enum class TapReadMode : std::uint8_t {
  None,       // not a tap read
  PreWrite,   // block mode before its write: delay floors at one block
  PostWrite,  // block mode after its write: any delay >= 0 is sample-exact
  Island,     // sample-serial lockstep with its write: floor one sample
};

struct Schedule {
  struct Segment {
    std::uint32_t begin = 0;  // range into order
    std::uint32_t end = 0;
    bool island = false;
  };

  std::vector<std::uint32_t> order;  // node indices in execution order
  std::vector<Segment> segments;
  std::vector<TapReadMode> read_modes;  // indexed by def node
  bool whole_graph_island = false;      // cross-island tap coupling fallback
};

[[nodiscard]] inline Schedule compute_schedule(const GraphDef& def) {
  constexpr std::uint32_t NoNode = 0xffffffffu;
  const auto n = static_cast<std::uint32_t>(def.nodes.size());

  // Pair each tap read with its write; the read->write dependency below is
  // what turns a feedback loop into a visible cycle.
  std::vector<std::uint32_t> read_write(n, NoNode);
  for (std::uint32_t r = 0; r < n; ++r) {
    if (def.nodes[r].kernel != &TapReadInfo) {
      continue;
    }
    const Hash id = tap_id(def, def.nodes[r]);
    for (std::uint32_t w = 0; w < n; ++w) {
      if (def.nodes[w].kernel == &TapWriteInfo &&
          tap_id(def, def.nodes[w]) == id) {
        read_write[r] = w;
        break;
      }
    }
  }

  // Dependency edges, CSR: declared inputs plus the tap back-edge.
  std::vector<std::uint32_t> dep_begin(n + 1, 0);
  for (std::uint32_t v = 0; v < n; ++v) {
    dep_begin[v + 1] = dep_begin[v] + def.nodes[v].input_count +
                       (read_write[v] != NoNode ? 1 : 0);
  }
  std::vector<std::uint32_t> deps(dep_begin[n]);
  for (std::uint32_t v = 0; v < n; ++v) {
    std::uint32_t at = dep_begin[v];
    for (std::uint32_t j = 0; j < def.nodes[v].input_count; ++j) {
      deps[at++] = def.inputs[def.nodes[v].input_begin + j];
    }
    if (read_write[v] != NoNode) {
      deps[at++] = read_write[v];
    }
  }

  // Tarjan strongly connected components.
  std::vector<std::uint32_t> visit(n, NoNode);
  std::vector<std::uint32_t> low(n, 0);
  std::vector<std::uint32_t> comp(n, NoNode);
  std::vector<std::uint32_t> stack;
  std::vector<std::uint8_t> on_stack(n, 0);
  std::uint32_t next_visit = 0;
  std::uint32_t comp_count = 0;
  auto connect = [&](this auto&& self, std::uint32_t v) -> void {
    visit[v] = low[v] = next_visit++;
    stack.push_back(v);
    on_stack[v] = 1;
    for (std::uint32_t e = dep_begin[v]; e < dep_begin[v + 1]; ++e) {
      const std::uint32_t w = deps[e];
      if (visit[w] == NoNode) {
        self(w);
        low[v] = std::min(low[v], low[w]);
      } else if (on_stack[w] != 0) {
        low[v] = std::min(low[v], visit[w]);
      }
    }
    if (low[v] == visit[v]) {
      while (true) {
        const std::uint32_t w = stack.back();
        stack.pop_back();
        on_stack[w] = 0;
        comp[w] = comp_count;
        if (w == v) {
          break;
        }
      }
      ++comp_count;
    }
  };
  for (std::uint32_t v = 0; v < n; ++v) {
    if (visit[v] == NoNode) {
      connect(v);
    }
  }

  std::vector<std::uint32_t> comp_size(comp_count, 0);
  for (std::uint32_t v = 0; v < n; ++v) {
    ++comp_size[comp[v]];
  }

  // A multi-node component is a feedback cycle. It stays block-mode only
  // when every in-cycle delay is a Const of at least one block (ADR 0004);
  // a modulated delay could dip under a block, so it islands conservatively.
  std::vector<std::uint8_t> demotable(comp_count, 1);
  for (std::uint32_t r = 0; r < n; ++r) {
    if (read_write[r] == NoNode || comp[r] != comp[read_write[r]]) {
      continue;
    }
    const std::uint32_t delay = def.inputs[def.nodes[r].input_begin];
    const bool long_const =
        def.nodes[delay].kernel == &ConstInfo &&
        def.values[def.nodes[delay].value_begin] * SampleRateF >=
            static_cast<float>(BlockSize);
    if (!long_const) {
      demotable[comp[r]] = 0;
    }
  }
  std::vector<std::uint8_t> island(comp_count, 0);
  for (std::uint32_t c = 0; c < comp_count; ++c) {
    island[c] = comp_size[c] > 1 && demotable[c] == 0 ? 1 : 0;
  }

  // A read inside an island needs its write's head advancing in lockstep,
  // so a free-standing outside write joins the island. Anything tighter —
  // two islands sharing a tap, or the write inside another cycle — falls
  // back to running the whole graph as one island: rare, always correct.
  bool whole = false;
  for (std::uint32_t r = 0; r < n && !whole; ++r) {
    if (read_write[r] == NoNode || comp[r] == NoNode || island[comp[r]] == 0) {
      continue;
    }
    const std::uint32_t w = read_write[r];
    if (comp[w] == comp[r]) {
      continue;
    }
    if (comp_size[comp[w]] == 1) {
      --comp_size[comp[w]];
      comp[w] = comp[r];
      ++comp_size[comp[w]];
    } else {
      whole = true;
    }
  }

  Schedule s;
  s.read_modes.assign(n, TapReadMode::None);

  if (whole) {
    // Tap writes emit last so every read sees the previous sample's write
    // (z^-1) — safe while writes are sinks with unconsumed outputs.
    s.whole_graph_island = true;
    for (std::uint32_t v = 0; v < n; ++v) {
      if (def.nodes[v].kernel != &TapWriteInfo) {
        s.order.push_back(v);
      }
    }
    for (std::uint32_t v = 0; v < n; ++v) {
      if (def.nodes[v].kernel == &TapWriteInfo) {
        s.order.push_back(v);
      }
    }
    s.segments.push_back({.begin = 0, .end = n, .island = true});
    for (std::uint32_t r = 0; r < n; ++r) {
      if (read_write[r] != NoNode) {
        s.read_modes[r] = TapReadMode::Island;
      }
    }
    return s;
  }

  // Component condensation, topologically ordered by Kahn with the smallest
  // member index as priority: for a pure DAG this reproduces def order.
  std::vector<std::vector<std::uint32_t>> members(comp_count);
  for (std::uint32_t v = 0; v < n; ++v) {
    members[comp[v]].push_back(v);
  }
  std::vector<std::uint32_t> min_node(comp_count, NoNode);
  for (std::uint32_t c = 0; c < comp_count; ++c) {
    if (!members[c].empty()) {
      min_node[c] = members[c].front();
    }
  }
  std::vector<std::pair<std::uint32_t, std::uint32_t>> edges;  // dep -> user
  for (std::uint32_t v = 0; v < n; ++v) {
    for (std::uint32_t e = dep_begin[v]; e < dep_begin[v + 1]; ++e) {
      if (comp[deps[e]] != comp[v]) {
        edges.emplace_back(comp[deps[e]], comp[v]);
      }
    }
  }
  std::sort(edges.begin(), edges.end());
  edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
  std::vector<std::uint32_t> succ_begin(comp_count + 1, 0);
  for (const auto& [d, u] : edges) {
    ++succ_begin[d + 1];
  }
  for (std::uint32_t c = 0; c < comp_count; ++c) {
    succ_begin[c + 1] += succ_begin[c];
  }
  std::vector<std::uint32_t> indegree(comp_count, 0);
  for (const auto& [d, u] : edges) {
    ++indegree[u];
  }

  using Entry = std::pair<std::uint32_t, std::uint32_t>;  // (min node, comp)
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> ready;
  for (std::uint32_t c = 0; c < comp_count; ++c) {
    if (!members[c].empty() && indegree[c] == 0) {
      ready.push({min_node[c], c});
    }
  }
  s.order.reserve(n);
  while (!ready.empty()) {
    const auto [mn, c] = ready.top();
    ready.pop();
    const bool isl = island[c] != 0;
    if (isl || s.segments.empty() || s.segments.back().island) {
      s.segments.push_back({.begin = static_cast<std::uint32_t>(s.order.size()),
                            .end = 0,
                            .island = isl});
    }
    if (isl) {
      // Writes emit last within a sample so every island read sees the
      // previous sample's write (z^-1) — safe while writes are sinks.
      for (const std::uint32_t v : members[c]) {
        if (def.nodes[v].kernel != &TapWriteInfo) {
          s.order.push_back(v);
        }
      }
      for (const std::uint32_t v : members[c]) {
        if (def.nodes[v].kernel == &TapWriteInfo) {
          s.order.push_back(v);
        }
      }
    } else {
      for (const std::uint32_t v : members[c]) {
        s.order.push_back(v);
      }
    }
    s.segments.back().end = static_cast<std::uint32_t>(s.order.size());
    for (std::uint32_t e = succ_begin[c]; e < succ_begin[c + 1]; ++e) {
      const std::uint32_t u = edges[e].second;
      if (--indegree[u] == 0) {
        ready.push({min_node[u], u});
      }
    }
  }
  atm_assert(s.order.size() == n);  // the condensation is acyclic

  for (std::uint32_t r = 0; r < n; ++r) {
    if (read_write[r] == NoNode) {
      continue;
    }
    if (island[comp[r]] != 0) {
      s.read_modes[r] = TapReadMode::Island;
    } else if (comp[read_write[r]] == comp[r]) {
      s.read_modes[r] = TapReadMode::PreWrite;
    } else {
      s.read_modes[r] = TapReadMode::PostWrite;
    }
  }
  return s;
}

// Value edits reach the schedule only through Const tap delays: one that
// flips a cycle between block and sample-serial execution must swap, because
// the schedule is baked into the Graph at build (ADR 0011).
[[nodiscard]] inline bool schedule_equivalent(const GraphDef& a,
                                              const GraphDef& b) {
  const Schedule sa = compute_schedule(a);
  const Schedule sb = compute_schedule(b);
  return sa.whole_graph_island == sb.whole_graph_island &&
         sa.read_modes == sb.read_modes;
}

}  // namespace automata::detail
