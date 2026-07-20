#pragma once

#include <array>
#include <cstddef>
#include <type_traits>

// Signature sniffing for kernel process/setter member functions (ADR 0005).
// MSVC deduces noexcept into the member-pointer type, hence every trait
// carries a dual specialization.

namespace automata::detail {

template <typename F>
struct MemFnTraits;

template <typename K, typename R, typename... A>
struct MemFnTraits<R (K::*)(A...)> {
  using Kernel = K;
  using Return = R;
  static constexpr std::size_t Arity = sizeof...(A);
  static constexpr bool AllFloatParams = (std::is_same_v<A, float> && ...);
};

template <typename K, typename R, typename... A>
struct MemFnTraits<R (K::*)(A...) noexcept> : MemFnTraits<R (K::*)(A...)> {};

// Output shape of process: float, or a standard-layout all-float aggregate.
// Size and alignment stand in for "all members float" — enforceable without
// reflection, and wrong aggregates fail loudly here.
template <typename R>
struct OutputTraits {
  static_assert(std::is_standard_layout_v<R> &&
                    std::is_trivially_copyable_v<R> &&
                    alignof(R) == alignof(float) &&
                    sizeof(R) % sizeof(float) == 0 && sizeof(R) > sizeof(float),
                "process must return float or an all-float aggregate");
  static constexpr std::size_t Count = sizeof(R) / sizeof(float);
};

template <>
struct OutputTraits<float> {
  static constexpr std::size_t Count = 1;
};

// SetterPack: aggregate of member-function pointers — guaranteed trivially
// copyable, unlike std::tuple — so bound setters can travel through a
// GraphDef's op bytes by memcpy.
template <typename... S>
struct SetterPack {};

template <typename S0, typename... Rest>
struct SetterPack<S0, Rest...> {
  S0 head;
  SetterPack<Rest...> tail;
};

template <std::size_t I, typename S0, typename... Rest>
[[nodiscard]] constexpr auto pack_get(const SetterPack<S0, Rest...>& pack) {
  if constexpr (I == 0) {
    return pack.head;
  } else {
    return pack_get<I - 1>(pack.tail);
  }
}

template <typename N>
[[nodiscard]] constexpr SetterPack<N> pack_append(const SetterPack<>&, N next) {
  return {next, {}};
}

template <typename S0, typename... Rest, typename N>
[[nodiscard]] constexpr auto pack_append(const SetterPack<S0, Rest...>& pack,
                                         N next) {
  return SetterPack<S0, Rest..., N>{pack.head, pack_append(pack.tail, next)};
}

// Per-binding offsets into the node's control-input list.
template <typename... S>
[[nodiscard]] consteval auto control_offsets() {
  std::array<std::size_t, sizeof...(S)> offsets{};
  std::size_t acc = 0;
  std::size_t i = 0;
  ((offsets[i++] = acc, acc += MemFnTraits<S>::Arity), ...);
  return offsets;
}

template <typename... S>
inline constexpr std::size_t TotalControlInputs =
    (std::size_t{0} + ... + MemFnTraits<S>::Arity);

}  // namespace automata::detail
