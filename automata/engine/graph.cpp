#include "automata/engine/graph.hpp"

#include <cstring>
#include <utility>

#include "automata/core/assert.hpp"
#include "automata/graph/kernel_info.hpp"
#include "automata/graph/tap.hpp"

namespace automata {

Graph::Graph(GraphDef def) : def_(std::move(def)), values_(def_.values) {
  const std::size_t count = def_.nodes.size();
  atm_assert(count > 0);
  buffers_.resize(count * BlockSize, 0.f);

  std::vector<std::size_t> offsets(count, 0);
  std::size_t arena = 0;
  for (std::size_t i = 0; i < count; ++i) {
    const KernelInfo& k = *def_.nodes[i].kernel;
    const std::size_t align = k.state_align > 0 ? k.state_align : 1;
    arena = (arena + align - 1) & ~(align - 1);
    offsets[i] = arena;
    arena += k.state_size;
  }
  state_.resize(arena);
  state_ptrs_.resize(count, nullptr);
  for (std::size_t i = 0; i < count; ++i) {
    state_ptrs_[i] = state_.data() + offsets[i];
    def_.nodes[i].kernel->construct(state_ptrs_[i]);
  }

  // Tap reads alias their write node's delay buffer, paired by id.
  for (std::size_t i = 0; i < count; ++i) {
    if (def_.nodes[i].kernel != &detail::TapReadInfo)
      continue;
    const Hash id = tap_id(def_, def_.nodes[i]);
    std::byte* write_state = nullptr;
    for (std::size_t w = 0; w < count; ++w) {
      if (def_.nodes[w].kernel == &detail::TapWriteInfo &&
          tap_id(def_, def_.nodes[w]) == id) {
        atm_assert(write_state == nullptr);  // one write per tap
        write_state = state_ptrs_[w];
      }
    }
    atm_assert(write_state != nullptr);  // a read requires its write
    state_ptrs_[i] = write_state;
  }

  input_ptrs_.resize(def_.inputs.size());
  for (std::size_t i = 0; i < def_.inputs.size(); ++i) {
    input_ptrs_[i] = buffers_.data() + def_.inputs[i] * BlockSize;
  }
}

void Graph::process_block() noexcept {
  for (std::size_t i = 0; i < def_.nodes.size(); ++i) {
    const GraphDef::Node& node = def_.nodes[i];
    node.kernel->process(
        state_ptrs_[i], input_ptrs_.data() + node.input_begin,
        def_.op_data.data() + node.op_begin, values_.data() + node.value_begin,
        buffers_.data() + i * BlockSize, static_cast<std::uint32_t>(BlockSize));
  }
}

void Graph::apply_values(const float* values, std::uint32_t count) noexcept {
  atm_assert(count == values_.size());
  std::memcpy(values_.data(), values, count * sizeof(float));
}

}  // namespace automata
