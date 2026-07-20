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

class ControlBus;

class Graph {
public:
  // With a bus, param nodes bind their slots at construction; without one
  // (offline, tests) they play their fallbacks.
  explicit Graph(GraphDef def, ControlBus* bus = nullptr);

  void process_block() noexcept;

  // Overwrites the values table in place (audio thread, bounded memcpy).
  void apply_values(const float* values, std::uint32_t count) noexcept;

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

  // State locations for TransferPlan building (control thread); the arenas
  // never move after construction.
  [[nodiscard]] std::byte* state_ptr(std::uint32_t node) noexcept {
    return state_ptrs_[node];
  }
  [[nodiscard]] const std::byte* state_ptr(std::uint32_t node) const noexcept {
    return state_ptrs_[node];
  }

private:
  GraphDef def_;
  std::vector<float> values_;
  std::vector<float> buffers_;
  std::vector<std::byte*> state_ptrs_;
  std::vector<std::byte> state_;
  std::vector<const float*> input_ptrs_;
};

}  // namespace automata
