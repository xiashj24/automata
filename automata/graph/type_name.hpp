#pragma once

#include <string_view>
#include "automata/core/hash.hpp"

// Stable kernel type identity (ADR 0005): the hash of the compiler's spelled
// signature for the instantiation, so template kernels (Delayline<N>) get a
// distinct identity per capacity. Stable within one toolchain, which is all
// identity needs — generations never cross toolchains (ADR 0001).

namespace automata::detail {

template <typename T>
[[nodiscard]] consteval std::string_view raw_type_tag() {
#if defined(_MSC_VER) && !defined(__clang__)
  return __FUNCSIG__;
#else
  return __PRETTY_FUNCTION__;
#endif
}

template <typename T>
inline constexpr Hash TypeTagHash = hash_string(raw_type_tag<T>());

}  // namespace automata::detail
