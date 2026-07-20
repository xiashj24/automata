#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "automata/graph/builder.hpp"
#include "automata/graph/kernel_info.hpp"
#include "automata/graph/kernel_traits.hpp"

// make_node<K>(audio...).control(&K::setter, sigs...).output(&K::Out::m) —
// the entire per-UGen authoring surface (ADR 0005). Audio inputs are deduced
// from process's signature; control inputs are bound setter by setter.

namespace automata {

template <detail::Kernel K, typename... S>
class NodeFactory {
public:
  NodeFactory(std::vector<Signal> inputs, detail::SetterPack<S...> setters)
      : inputs_(std::move(inputs)), setters_(setters) {}

  template <typename Setter, typename... Sig>
  [[nodiscard]] auto control(Setter setter, Sig... signals) && {
    using Traits = detail::MemFnTraits<Setter>;
    static_assert(std::is_same_v<typename Traits::Kernel, K>,
                  "control(): setter must belong to this kernel");
    static_assert(sizeof...(Sig) == Traits::Arity,
                  "control(): signal count must match setter arity");
    (inputs_.push_back(signals), ...);
    return NodeFactory<K, S..., Setter>{std::move(inputs_),
                                        detail::pack_append(setters_, setter)};
  }

  // Selects the emitted member of a multi-output kernel. The selector is a
  // value, never hashed, so lp→bp edits land as value patches (ADR 0002).
  template <typename Out>
  [[nodiscard]] Signal output(float Out::* member) && {
    using PT = detail::MemFnTraits<decltype(&K::process)>;
    static_assert(std::is_same_v<Out, typename PT::Return>,
                  "output(): member must belong to process's return type");
    Out probe{};
    const auto index = &(probe.*member) - reinterpret_cast<float*>(&probe);
    return std::move(*this).finalize(static_cast<float>(index));
  }

  operator Signal() && { return std::move(*this).finalize(0.f); }

private:
  template <detail::Kernel, typename...>
  friend class NodeFactory;

  [[nodiscard]] Signal finalize(float selector) && {
    using PT = detail::MemFnTraits<decltype(&K::process)>;
    constexpr std::size_t NumOut =
        detail::OutputTraits<typename PT::Return>::Count;
    atm_assert(inputs_.size() == PT::Arity + detail::TotalControlInputs<S...>);

    const KernelInfo* info = detail::kernel_info<K, S...>();
    std::array<std::byte, sizeof(detail::SetterPack<S...>)> op{};
    std::span<const std::byte> op_span{};
    if constexpr (sizeof...(S) > 0) {
      std::memcpy(op.data(), &setters_, sizeof(setters_));
      op_span = op;
    }
    const float values[1] = {selector};
    std::span<const float> value_span{};
    if constexpr (NumOut > 1) {
      value_span = values;
    }
    return ActiveGraph::current().add_node(info, info->type_hash, inputs_,
                                           value_span, {}, op_span);
  }

  std::vector<Signal> inputs_;
  detail::SetterPack<S...> setters_;
};

template <detail::Kernel K, typename... In>
[[nodiscard]] NodeFactory<K> make_node(In... audio_inputs) {
  using PT = detail::MemFnTraits<decltype(&K::process)>;
  static_assert(sizeof...(In) == PT::Arity,
                "make_node: audio input count must match process arity");
  std::vector<Signal> inputs;
  inputs.reserve(sizeof...(In));
  (inputs.push_back(audio_inputs), ...);
  return {std::move(inputs), {}};
}

}  // namespace automata
