#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

#include "automata/core/hash.hpp"
#include "automata/graph/kernel_traits.hpp"
#include "automata/graph/type_name.hpp"

// One KernelInfo per (kernel, bound-setter shape): the function-pointer op
// table a Graph executes. Vocabulary kernels get host-owned statics; a patch
// registering its own kernel gets infos inside its library, which is what
// pins the generation (ADR 0001).

namespace automata {

struct KernelInfo {
  Hash type_hash = 0;
  std::uint32_t state_size = 0;
  std::uint32_t state_align = 1;
  std::uint32_t output_count = 1;
  void (*construct)(void* state) = nullptr;
  void (*reset)(void* state) = nullptr;
  // ins: one block pointer per input, audio inputs then control inputs;
  // values: the node's slice (selector at [0] when output_count > 1).
  void (*process)(void* state,
                  const float* const* ins,
                  const std::byte* op,
                  const float* values,
                  float* out,
                  std::uint32_t nframes) = nullptr;
};

namespace detail {

template <typename K>
concept Kernel = std::is_class_v<K> && std::is_trivially_copyable_v<K> &&
                 std::is_default_constructible_v<K> && requires {
                   &K::process;
                   &K::reset;
                 };

template <std::size_t B, typename K, typename... S>
void run_setter(K& k,
                const SetterPack<S...>& setters,
                const float* const* cins,
                std::uint32_t i) {
  using Setter = std::tuple_element_t<B, std::tuple<S...>>;
  constexpr auto Offsets = control_offsets<S...>();
  const auto setter = pack_get<B>(setters);
  [&]<std::size_t... J>(std::index_sequence<J...>) {
    (k.*setter)(cins[Offsets[B] + J][i]...);
  }(std::make_index_sequence<MemFnTraits<Setter>::Arity>{});
}

// Per-sample: bound setters, then process — audio-rate modulation of any
// bound parameter is correct by default (ADR 0005).
template <Kernel K, typename... S>
void process_block_for(void* state,
                       const float* const* ins,
                       [[maybe_unused]] const std::byte* op,
                       [[maybe_unused]] const float* values,
                       float* out,
                       std::uint32_t nframes) {
  using PT = MemFnTraits<decltype(&K::process)>;
  using R = typename PT::Return;
  constexpr std::size_t NumAudio = PT::Arity;
  constexpr std::size_t NumOut = OutputTraits<R>::Count;

  K& k = *static_cast<K*>(state);
  SetterPack<S...> setters{};
  if constexpr (sizeof...(S) > 0) {
    std::memcpy(&setters, op, sizeof(setters));
  }
  [[maybe_unused]] std::uint32_t sel = 0;
  if constexpr (NumOut > 1) {
    sel = static_cast<std::uint32_t>(values[0]);
  }

  for (std::uint32_t i = 0; i < nframes; ++i) {
    [&]<std::size_t... B>(std::index_sequence<B...>) {
      (run_setter<B>(k, setters, ins + NumAudio, i), ...);
    }(std::index_sequence_for<S...>{});

    const auto call = [&]<std::size_t... A>(std::index_sequence<A...>) {
      return k.process(ins[A][i]...);
    };
    if constexpr (NumOut == 1) {
      out[i] = call(std::make_index_sequence<NumAudio>{});
    } else {
      const R r = call(std::make_index_sequence<NumAudio>{});
      out[i] = reinterpret_cast<const float*>(&r)[sel];
    }
  }
}

// Binding shape (setter arities, in order) folds into type identity: nodes
// executing different op shapes must never pair across generations.
template <Kernel K, typename... S>
[[nodiscard]] consteval Hash binding_type_hash() {
  Hash h = TypeTagHash<K>;
  ((h = hash_combine(h, MemFnTraits<S>::Arity)), ...);
  return h;
}

template <Kernel K, typename... S>
[[nodiscard]] const KernelInfo* kernel_info() {
  using PT = MemFnTraits<decltype(&K::process)>;
  static_assert(PT::AllFloatParams, "process parameters must be float");
  static_assert((MemFnTraits<S>::AllFloatParams && ...),
                "setter parameters must be float");
  static const KernelInfo info{
      .type_hash = binding_type_hash<K, S...>(),
      .state_size = sizeof(K),
      .state_align = alignof(K),
      .output_count =
          static_cast<std::uint32_t>(OutputTraits<typename PT::Return>::Count),
      .construct = +[](void* s) { ::new (s) K(); },
      .reset = +[](void* s) { static_cast<K*>(s)->reset(); },
      .process = &process_block_for<K, S...>,
  };
  return &info;
}

}  // namespace detail

}  // namespace automata
