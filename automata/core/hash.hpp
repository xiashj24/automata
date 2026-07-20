#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

// 64-bit FNV-1a — the structural-identity hash (ADR 0002). Deterministic
// across runs, processes, and toolchains, which std::hash does not promise;
// hashes are compared across patch generations, so that determinism is the
// point. Collisions are ignored by design at per-graph node counts.

namespace automata {

using Hash = std::uint64_t;

// FNV-1a offset basis: the seed of every unchained hash.
inline constexpr Hash HashSeed = 0xcbf29ce484222325ull;

namespace detail {

inline constexpr Hash FnvPrime = 0x100000001b3ull;

[[nodiscard]] constexpr Hash fnv1a_step(Hash h, std::uint8_t byte) noexcept {
  return (h ^ byte) * FnvPrime;
}

}  // namespace detail

[[nodiscard]] constexpr Hash hash_bytes(std::span<const std::byte> bytes,
                                        Hash seed = HashSeed) noexcept {
  Hash h = seed;
  for (const std::byte b : bytes) {
    h = detail::fnv1a_step(h, std::to_integer<std::uint8_t>(b));
  }
  return h;
}

[[nodiscard]] constexpr Hash hash_string(std::string_view text,
                                         Hash seed = HashSeed) noexcept {
  Hash h = seed;
  for (const char c : text) {
    h = detail::fnv1a_step(h, static_cast<std::uint8_t>(c));
  }
  return h;
}

// Hashes the object representation, so float configs hash by bit pattern
// (0.f and -0.f differ). Scalars only: aggregates may carry padding.
template <typename T>
  requires(std::is_arithmetic_v<T> || std::is_enum_v<T>)
[[nodiscard]] constexpr Hash hash_value(T value,
                                        Hash seed = HashSeed) noexcept {
  const auto bytes = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
  return hash_bytes(bytes, seed);
}

// Chains h into seed; order-sensitive, as structural hashing requires.
[[nodiscard]] constexpr Hash hash_combine(Hash seed, Hash h) noexcept {
  return hash_value(h, seed);
}

}  // namespace automata
