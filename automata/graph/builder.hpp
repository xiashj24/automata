#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "automata/config.hpp"
#include "automata/core/assert.hpp"
#include "automata/core/hash.hpp"
#include "automata/graph/def.hpp"
#include "automata/graph/kernel_info.hpp"

namespace automata {

class GraphBuilder;

// A value-type handle to one node's single output. Constructing one from a
// float lifts it to a Const node in the active graph, which is what lets
// `800.0f + lfo * 600.0f` read as plain arithmetic.
class Signal {
public:
  Signal(float value);  // implicit: the scalar lift

  [[nodiscard]] std::uint32_t index() const { return index_; }

private:
  friend class GraphBuilder;
  explicit Signal(std::uint32_t index) : index_(index) {}

  std::uint32_t index_;
};

namespace detail {

// The glide every control leaf (Const, param) plays through (ADR 0002's
// ~10 ms ramp): the first value snaps, later changes ramp linearly. Living
// in transferable state, a glide survives a structural swap mid-ramp.
struct RampState {
  float current = 0.f;
  float target = 0.f;
  std::uint32_t remaining = 0;
  std::uint32_t initialized = 0;
};

inline void ramp_toward(RampState& s,
                        float value,
                        float* out,
                        std::uint32_t nframes) {
  if (s.initialized == 0) {
    s.initialized = 1;
    s.current = s.target = value;
  }
  if (value != s.target) {
    s.target = value;
    constexpr auto Ramp = static_cast<std::uint32_t>(
        DefaultValueRampSeconds * static_cast<float>(SampleRate));
    s.remaining = Ramp > 0 ? Ramp : 1;
  }
  for (std::uint32_t i = 0; i < nframes; ++i) {
    if (s.remaining > 0) {
      s.current += (s.target - s.current) / static_cast<float>(s.remaining);
      --s.remaining;
      if (s.remaining == 0) {
        s.current = s.target;
      }
    }
    out[i] = s.current;
  }
}

// The Const op. Hand-written info — Const is graph vocabulary, not a
// kernel; it plays its patchable values[0] through the shared glide.
inline void const_process(void* state,
                          const float* const*,
                          const std::byte*,
                          const float* values,
                          float* out,
                          std::uint32_t nframes) {
  ramp_toward(*static_cast<RampState*>(state), values[0], out, nframes);
}

inline const KernelInfo ConstInfo{
    .type_hash = hash_string("automata.Const"),
    .state_size = sizeof(RampState),
    .state_align = alignof(RampState),
    .output_count = 1,
    .construct = +[](void* s) { ::new (s) RampState{}; },
    .reset = +[](void* s) { *static_cast<RampState*>(s) = RampState{}; },
    .process = &const_process,
};

}  // namespace detail

class Tap;

class GraphBuilder {
public:
  [[nodiscard]] Signal add_node(const KernelInfo* kernel,
                                Hash type_hash,
                                std::span<const Signal> inputs,
                                std::span<const float> values,
                                std::span<const float> configs,
                                std::span<const std::byte> op_data,
                                bool sink = false) {
    atm_assert(kernel != nullptr);
    atm_assert(def_.nodes.size() < MaxGraphNodes);
    GraphDef::Node node;
    node.kernel = kernel;
    node.type_hash = type_hash;
    node.sink = sink ? 1 : 0;
    node.input_begin = static_cast<std::uint32_t>(def_.inputs.size());
    node.input_count = static_cast<std::uint32_t>(inputs.size());
    for (const Signal s : inputs) {
      atm_assert(s.index() < def_.nodes.size());
      def_.inputs.push_back(s.index());
    }
    node.value_begin = static_cast<std::uint32_t>(def_.values.size());
    node.value_count = static_cast<std::uint32_t>(values.size());
    def_.values.insert(def_.values.end(), values.begin(), values.end());
    node.config_begin = static_cast<std::uint32_t>(def_.configs.size());
    node.config_count = static_cast<std::uint32_t>(configs.size());
    def_.configs.insert(def_.configs.end(), configs.begin(), configs.end());
    node.op_begin = static_cast<std::uint32_t>(def_.op_data.size());
    node.op_size = static_cast<std::uint32_t>(op_data.size());
    def_.op_data.insert(def_.op_data.end(), op_data.begin(), op_data.end());
    def_.nodes.push_back(node);
    return Signal{static_cast<std::uint32_t>(def_.nodes.size() - 1)};
  }

  [[nodiscard]] Signal constant(float value) {
    const float values[1] = {value};
    return add_node(&detail::ConstInfo, detail::ConstInfo.type_hash, {}, values,
                    {}, {});
  }

  void out(Signal left, Signal right) {
    atm_assert(!has_out_);
    def_.outs = {left.index(), right.index()};
    has_out_ = true;
  }

  // build() requires an out; loaders check instead of crashing on a bad patch.
  [[nodiscard]] bool has_out() const { return has_out_; }

  // A named tap hashes by its name — identity that survives any edit; an
  // anonymous tap hashes by creation ordinal, fragile under reordering
  // (ADR 0002). Defined in graph/tap.hpp.
  [[nodiscard]] Tap tap(std::string_view name);
  [[nodiscard]] Tap tap();

  // Finalizes structural hashes and yields the def; the builder is spent.
  [[nodiscard]] GraphDef build() {
    atm_assert(has_out_);
    for (auto& node : def_.nodes) {
      Hash h = node.type_hash;
      for (std::uint32_t c = 0; c < node.config_count; ++c) {
        h = hash_combine(h, hash_value(def_.configs[node.config_begin + c]));
      }
      for (std::uint32_t i = 0; i < node.input_count; ++i) {
        const std::uint32_t input = def_.inputs[node.input_begin + i];
        h = hash_combine(h, def_.nodes[input].structural_hash);
      }
      node.structural_hash = h;
    }
    def_.def_hash = hash_combine(def_.nodes[def_.outs[0]].structural_hash,
                                 def_.nodes[def_.outs[1]].structural_hash);
    // Sinks (tap writes) are effectful even when unreachable from the outs,
    // so they participate in def identity — sorted, to stay reorder-stable.
    std::vector<Hash> sinks;
    for (const auto& node : def_.nodes) {
      if (node.sink != 0) {
        sinks.push_back(node.structural_hash);
      }
    }
    std::sort(sinks.begin(), sinks.end());
    for (const Hash s : sinks) {
      def_.def_hash = hash_combine(def_.def_hash, s);
    }
    return std::move(def_);
  }

private:
  friend class Tap;

  GraphDef def_;
  std::uint32_t tap_count_ = 0;
  bool has_out_ = false;
};

// Installs the builder that factories and Signal lifts append to for the
// duration of describe (ADR 0005). Single-threaded by contract, asserted.
class ActiveGraph {
public:
  explicit ActiveGraph(GraphBuilder& builder) {
    atm_assert(active_ == nullptr);
    active_ = &builder;
  }
  ~ActiveGraph() { active_ = nullptr; }
  ActiveGraph(const ActiveGraph&) = delete;
  ActiveGraph& operator=(const ActiveGraph&) = delete;

  [[nodiscard]] static GraphBuilder& current() {
    atm_assert(active_ != nullptr);
    return *active_;
  }

private:
  static inline GraphBuilder* active_ = nullptr;
};

inline Signal::Signal(float value)
    : index_(ActiveGraph::current().constant(value).index_) {}

// Describes a graph by running fn(builder) against a scoped active builder —
// the same shape the patch entry point uses (ADR 0005).
template <typename Fn>
[[nodiscard]] GraphDef describe(Fn&& fn) {
  GraphBuilder builder;
  {
    ActiveGraph scope(builder);
    fn(builder);
  }
  return builder.build();
}

}  // namespace automata
