#include "automata/engine/graph.hpp"

#include <utility>

#include "automata/core/assert.hpp"
#include "automata/graph/kernel_info.hpp"

namespace automata {

Graph::Graph(GraphDef def) : def_(std::move(def)), values_(def_.values) {
  const std::size_t count = def_.nodes.size();
  atm_assert(count > 0);
  buffers_.resize(count * BlockSize, 0.f);

  state_offsets_.resize(count, 0);
  std::size_t arena = 0;
  for (std::size_t i = 0; i < count; ++i) {
    const KernelInfo& k = *def_.nodes[i].kernel;
    const std::size_t align = k.state_align > 0 ? k.state_align : 1;
    arena = (arena + align - 1) & ~(align - 1);
    state_offsets_[i] = arena;
    arena += k.state_size;
  }
  state_.resize(arena);
  for (std::size_t i = 0; i < count; ++i) {
    def_.nodes[i].kernel->construct(state_.data() + state_offsets_[i]);
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
        state_.data() + state_offsets_[i],
        input_ptrs_.data() + node.input_begin,
        def_.op_data.data() + node.op_begin, values_.data() + node.value_begin,
        buffers_.data() + i * BlockSize, static_cast<std::uint32_t>(BlockSize));
  }
}

}  // namespace automata
