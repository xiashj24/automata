#include "automata/engine/graph.hpp"

#include <cstring>
#include <utility>

#include "automata/core/assert.hpp"
#include "automata/core/control_bus.hpp"
#include "automata/core/transport.hpp"
#include "automata/graph/clock.hpp"
#include "automata/graph/kernel_info.hpp"
#include "automata/graph/param.hpp"
#include "automata/graph/schedule.hpp"
#include "automata/graph/tap.hpp"

namespace automata {

Graph::Graph(GraphDef def, ControlBus* bus, const Transport* transport)
    : def_(std::move(def)), values_(def_.values) {
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

  // Params bind their slot by name; unwired (no bus, or a full bus) they
  // play their patchable fallback.
  if (bus != nullptr) {
    for (std::size_t i = 0; i < count; ++i) {
      if (def_.nodes[i].kernel != &detail::ParamInfo)
        continue;
      auto& s = *reinterpret_cast<detail::ParamState*>(state_ptrs_[i]);
      s.slot = bus->channel(param_name(def_, def_.nodes[i]));
    }
  }

  // Cycles bind the transport they re-derive phase from; unbound they
  // emit 0.
  if (transport != nullptr) {
    for (std::size_t i = 0; i < count; ++i) {
      if (def_.nodes[i].kernel != &detail::ClockInfo)
        continue;
      auto& s = *reinterpret_cast<detail::ClockState*>(state_ptrs_[i]);
      s.transport = transport;
    }
  }

  input_ptrs_.resize(def_.inputs.size());
  for (std::size_t i = 0; i < def_.inputs.size(); ++i) {
    input_ptrs_[i] = buffers_.data() + def_.inputs[i] * BlockSize;
  }

  // Schedule and op table (ADR 0011). island_ptrs_ must reach its final
  // size before any op keeps a pointer into it.
  const detail::Schedule sched = detail::compute_schedule(def_);
  island_ptrs_ = input_ptrs_;
  ops_.resize(count);
  segments_.reserve(sched.segments.size());
  for (const detail::Schedule::Segment& seg : sched.segments) {
    Segment segment{
        .begin = seg.begin,
        .end = seg.end,
        .inputs_begin = static_cast<std::uint32_t>(island_inputs_.size()),
        .inputs_end = 0,
        .island = seg.island ? std::uint8_t{1} : std::uint8_t{0}};
    for (std::uint32_t k = seg.begin; k < seg.end; ++k) {
      const std::uint32_t v = sched.order[k];
      const GraphDef::Node& node = def_.nodes[v];
      Op& op = ops_[k];
      op.process = node.kernel->process;
      if (node.kernel == &detail::TapReadInfo) {
        switch (sched.read_modes[v]) {
          case detail::TapReadMode::PreWrite:
            op.process = &detail::tap_read_process;
            break;
          case detail::TapReadMode::PostWrite:
            op.process = &detail::tap_read_post_write_process;
            break;
          case detail::TapReadMode::Island:
            op.process = &detail::tap_read_island_process;
            break;
          case detail::TapReadMode::None:
            atm_assert(false);  // every read gets a scheduled mode
            break;
        }
      }
      op.state = state_ptrs_[v];
      op.ins =
          (seg.island ? island_ptrs_ : input_ptrs_).data() + node.input_begin;
      op.op_bytes = def_.op_data.data() + node.op_begin;
      op.values = values_.data() + node.value_begin;
      op.out = buffers_.data() + v * BlockSize;
      if (seg.island) {
        for (std::uint32_t j = 0; j < node.input_count; ++j) {
          island_inputs_.push_back(node.input_begin + j);
        }
      }
    }
    segment.inputs_end = static_cast<std::uint32_t>(island_inputs_.size());
    segments_.push_back(segment);
  }
}

void Graph::process_block() noexcept {
  for (const Segment& seg : segments_) {
    if (seg.island == 0) {
      for (std::uint32_t k = seg.begin; k < seg.end; ++k) {
        const Op& op = ops_[k];
        op.process(op.state, op.ins, op.op_bytes, op.values, op.out,
                   static_cast<std::uint32_t>(BlockSize));
      }
      continue;
    }
    for (std::uint32_t i = 0; i < BlockSize; ++i) {
      for (std::uint32_t j = seg.inputs_begin; j < seg.inputs_end; ++j) {
        const std::uint32_t p = island_inputs_[j];
        island_ptrs_[p] = input_ptrs_[p] + i;
      }
      for (std::uint32_t k = seg.begin; k < seg.end; ++k) {
        const Op& op = ops_[k];
        op.process(op.state, op.ins, op.op_bytes, op.values, op.out + i, 1);
      }
    }
  }
}

void Graph::apply_values(const float* values, std::uint32_t count) noexcept {
  atm_assert(count == values_.size());
  std::memcpy(values_.data(), values, count * sizeof(float));
}

}  // namespace automata
