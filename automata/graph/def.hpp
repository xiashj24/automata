#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "automata/core/hash.hpp"

// The immutable plain-data graph description (ADR 0001/0002). Values —
// including each multi-output node's selector at values[0] — are patchable
// and never hashed; everything else is identity. def_hash folds only the
// output nodes' subtree hashes, so pure statement reordering or edits to
// unreachable nodes cannot force a structural swap.

namespace automata {

struct KernelInfo;

struct GraphDef {
  struct Node {
    const KernelInfo* kernel = nullptr;
    Hash type_hash = 0;
    Hash structural_hash = 0;
    std::uint32_t input_begin = 0;
    std::uint32_t input_count = 0;
    std::uint32_t value_begin = 0;
    std::uint32_t value_count = 0;
    std::uint32_t config_begin = 0;
    std::uint32_t config_count = 0;
    std::uint32_t op_begin = 0;
    std::uint32_t op_size = 0;
    std::uint8_t sink = 0;  // effectful without consumers (tap writes)
  };

  std::vector<Node> nodes;  // creation order — inputs precede their users
  std::vector<std::uint32_t> inputs;
  std::vector<float> values;
  std::vector<float> configs;
  std::vector<std::byte> op_data;  // invoker-private bytes; never hashed
  std::array<std::uint32_t, 2> outs{};
  Hash def_hash = 0;
};

}  // namespace automata
