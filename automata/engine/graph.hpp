#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "automata/config.hpp"
#include "automata/graph/def.hpp"

// The executable form of a GraphDef (ADR 0004): schedule in def order, one
// block buffer per node, contiguous trivially-copyable state arena.
// Construction allocates (control thread only); process_block is real-time
// safe.

namespace automata {

class Graph {
public:
  explicit Graph(GraphDef def);

  void process_block() noexcept;

  [[nodiscard]] std::span<const float> left() const noexcept {
    return node_output(def_.outs[0]);
  }
  [[nodiscard]] std::span<const float> right() const noexcept {
    return node_output(def_.outs[1]);
  }
  [[nodiscard]] std::span<const float> node_output(
      std::uint32_t node) const noexcept {
    return {buffers_.data() + node * BlockSize, BlockSize};
  }
  [[nodiscard]] const GraphDef& def() const noexcept { return def_; }

private:
  GraphDef def_;
  std::vector<float> values_;
  std::vector<float> buffers_;
  std::vector<std::size_t> state_offsets_;
  std::vector<std::byte> state_;
  std::vector<const float*> input_ptrs_;
};

}  // namespace automata
