#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "automata/graph/def.hpp"

// Cross-generation node matching (ADR 0002): identity is (structural_hash,
// ordinal), ordinal being the occurrence index among equal-hash nodes in def
// order. Everything here runs on the control thread; only the finished
// TransferPlan's memcpys execute on the audio thread.

namespace automata {

class Graph;

inline constexpr std::uint32_t NoMatch = 0xffffffffu;

// matches[i] = index in `to` paired with node i of `from`, or NoMatch.
[[nodiscard]] std::vector<std::uint32_t> match_nodes(const GraphDef& from,
                                                     const GraphDef& to);

// The running def's values table with every matched node's values replaced
// by its counterpart's — the value-patch payload. Defs must hash equal.
[[nodiscard]] std::vector<float> remap_values(const GraphDef& running,
                                              const GraphDef& incoming);

struct TransferPlan {
  struct Copy {
    const std::byte* src;
    std::byte* dst;
    std::uint32_t size;
  };
  std::vector<Copy> copies;
  std::size_t total_bytes = 0;
};

// Pointers into both arenas, which never move; the copies read live state at
// swap time on the audio thread (ADR 0003).
[[nodiscard]] TransferPlan plan_transfer(const Graph& from, Graph& to);

}  // namespace automata
