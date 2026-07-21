#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "automata/config.hpp"
#include "automata/graph/def.hpp"

// The executable form of a GraphDef (ADR 0004/0011): a precomputed op table
// in schedule order — block segments run whole blocks, feedback islands run
// sample-serial — one block buffer per node, contiguous trivially-copyable
// state arena. Construction allocates (control thread only); process_block
// is real-time safe.

namespace automata {

class ControlBus;
struct Transport;

class Graph {
public:
  // With a bus, param nodes bind their slots at construction; without one
  // (tests) they play their fallbacks. With a transport, cycle nodes derive
  // their phase from it; without one they emit 0.
  explicit Graph(GraphDef def,
                 ControlBus* bus = nullptr,
                 const Transport* transport = nullptr);

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
  // One resolved entry per node, in schedule order; everything an op points
  // at is fixed at construction (arenas and tables never move).
  struct Op {
    void (*process)(void* state,
                    const float* const* ins,
                    const std::byte* op,
                    const float* values,
                    float* out,
                    std::uint32_t nframes) = nullptr;
    std::byte* state = nullptr;
    const float* const* ins = nullptr;
    const std::byte* op_bytes = nullptr;
    const float* values = nullptr;
    float* out = nullptr;
  };
  struct Segment {
    std::uint32_t begin = 0;  // range into ops_
    std::uint32_t end = 0;
    std::uint32_t inputs_begin = 0;  // range into island_inputs_
    std::uint32_t inputs_end = 0;
    std::uint8_t island = 0;
  };

  GraphDef def_;
  std::vector<float> values_;
  std::vector<float> buffers_;
  std::vector<std::byte*> state_ptrs_;
  std::vector<std::byte> state_;
  std::vector<const float*> input_ptrs_;
  std::vector<Op> ops_;
  std::vector<Segment> segments_;
  // Island ops read through this scratch copy of input_ptrs_, re-pointed to
  // the current sample before each per-sample pass.
  std::vector<const float*> island_ptrs_;
  std::vector<std::uint32_t> island_inputs_;
};

}  // namespace automata
