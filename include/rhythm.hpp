#pragma once
#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <cstddef>
#include <string_view>
#include <math.hpp>
#include <euclid.hpp>

namespace automata {

template <std::size_t N>
  requires(N > 0)
class Rhythm {
public:
  constexpr Rhythm() noexcept = default;

  [[nodiscard]] static constexpr Rhythm euclid(uint32_t pulses) noexcept {
    Rhythm result;
    for (std::size_t i = 0; i < N; ++i)
      result.set(i, euclid_simple(pulses, static_cast<uint32_t>(N),
                                  static_cast<uint32_t>(i)));
    return result;
  }

  // "x.x.xxx.x." — 'x'/'X'/'1' = hit, anything else = rest
  constexpr explicit Rhythm(std::string_view sv) noexcept {
    for (std::size_t i = 0; i < sv.size() && i < N; ++i)
      bits.set(i, sv[i] == 'x' || sv[i] == 'X' || sv[i] == '1');
  }

  template <std::size_t M>
  constexpr explicit Rhythm(const char (&sv)[M]) noexcept
      : Rhythm(std::string_view{sv, M - 1}) {}

  // converting from a different-length Rhythm: zero-pad or truncate (implicit
  // by design)
  template <std::size_t M>
    requires(M != N)
  // cppcheck-suppress noExplicitConstructor
  constexpr Rhythm(const Rhythm<M>& other) noexcept {
    for (std::size_t i = 0; i < N; ++i)
      bits.set(i, i < M ? other[i] : false);
  }

  // integer bit pattern: bit 0 = first step (e.g. 0b1010, 0x1A)
  constexpr explicit Rhythm(std::uint64_t val) noexcept {
    for (std::size_t i = 0; i < N; ++i)
      bits.set(i, (val >> i) & 1u);
  }

  [[nodiscard]] constexpr bool operator==(const Rhythm& rhs) const noexcept {
    return bits == rhs.bits;
  }

  [[nodiscard]] constexpr bool operator[](std::size_t i) const noexcept {
    return bits.test(i % N);
  }

  // do you really need this? should you use modulo here?
  constexpr void set(std::size_t i, bool v = true) noexcept { bits.set(i, v); }
  [[nodiscard]] constexpr std::size_t length() const noexcept { return N; }
  [[nodiscard]] constexpr std::size_t hits() const noexcept {
    return bits.count();
  }
  [[nodiscard]] constexpr std::size_t rests() const noexcept {
    return N - bits.count();
  }
  [[nodiscard]] constexpr bool none() const noexcept { return bits.none(); }

  template <std::size_t Target>
    requires(Target > 0)
  [[nodiscard]] constexpr Rhythm<Target> tile() const noexcept {
    Rhythm<Target> out;
    for (std::size_t i = 0; i < Target; ++i)
      out.set(i, (*this)[i % N]);
    return out;
  }

  template <std::size_t K>
    requires(K > 0)
  [[nodiscard]] constexpr Rhythm<N * K> stretch() const noexcept {
    Rhythm<N * K> result;
    for (std::size_t i = 0; i < N; ++i)
      result.set(i * K, (*this)[i]);
    return result;
  }

  template <std::size_t K>
    requires(K > 0 && K < N)
  [[nodiscard]] constexpr Rhythm<(N + K - 1) / K> compress() const noexcept {
    constexpr std::size_t output_size = (N + K - 1) / K;
    Rhythm<output_size> result;
    for (std::size_t i = 0; i < output_size; ++i)
      result.set(i, (*this)[i * K]);
    return result;
  }

  template <std::size_t K>
    requires(K > 0)
  [[nodiscard]] constexpr Rhythm permute() const noexcept {
    std::array<bool, N> steps{};
    for (std::size_t i = 0; i < N; ++i)
      steps[i] = (*this)[i];
    for (std::size_t k = 0; k < K; ++k)
      std::ranges::next_permutation(steps);
    Rhythm result;
    for (std::size_t i = 0; i < N; ++i)
      result.set(i, steps[i]);
    return result;
  }

  // and
  [[nodiscard]] constexpr Rhythm operator*(const Rhythm& rhs) const noexcept {
    Rhythm out;
    for (std::size_t i = 0; i < N; ++i)
      out.set(i, (*this)[i] && rhs[i]);
    return out;
  }

  // or
  [[nodiscard]] constexpr Rhythm operator+(const Rhythm& rhs) const noexcept {
    Rhythm out;
    for (std::size_t i = 0; i < N; ++i)
      out.set(i, (*this)[i] || rhs[i]);
    return out;
  }

  // not
  [[nodiscard]] constexpr Rhythm operator~() const noexcept {
    Rhythm out;
    for (std::size_t i = 0; i < N; ++i)
      out.set(i, !(*this)[i]);
    return out;
  }

  // shorthand for a * !b
  [[nodiscard]] constexpr Rhythm operator-(const Rhythm& rhs) const noexcept {
    Rhythm out;
    for (std::size_t i = 0; i < N; ++i)
      out.set(i, (*this)[i] && (!rhs[i]));
    return out;
  }

  // xor
  [[nodiscard]] constexpr Rhythm operator^(const Rhythm& rhs) const noexcept {
    Rhythm out;
    for (std::size_t i = 0; i < N; ++i)
      out.set(i, (*this)[i] ^ rhs[i]);
    return out;
  }

  // dac: pack the first B steps into a uint32_t (bit 0 = LSB)
  template <std::size_t B>
  [[nodiscard]] constexpr uint32_t dac() const noexcept {
    static_assert(B <= 32, "bit depth must be <= 32");
    constexpr std::size_t len = B < N ? B : N;
    uint32_t result = 0;
    for (std::size_t i = 0; i < len; ++i)
      result |= static_cast<uint32_t>((*this)[i]) << i;
    return result;
  }

  // rotation (wrapping shift)
  [[nodiscard]] constexpr Rhythm operator>>(std::size_t n) const noexcept {
    n %= N;
    Rhythm out;
    for (std::size_t i = 0; i < N; ++i)
      out.set((i + n) % N, (*this)[i]);
    return out;
  }

  [[nodiscard]] constexpr Rhythm operator<<(std::size_t n) const noexcept {
    n %= N;
    Rhythm out;
    for (std::size_t i = 0; i < N; ++i)
      out.set((i + N - n) % N, (*this)[i]);
    return out;
  }

private:
  std::bitset<N> bits{};
};

// CTAD: Rhythm{"x.x."} deduces Rhythm<4> (string literal char[N] ->
// Rhythm<N-1>)
template <std::size_t M>
Rhythm(const char (&)[M]) -> Rhythm<M - 1>;

// boolean ops between different-length rhythms: tile both to lcm(N, M)
template <std::size_t N, std::size_t M>
  requires(N != M)
[[nodiscard]] constexpr Rhythm<lcm(N, M)> operator*(
    const Rhythm<N>& lhs,
    const Rhythm<M>& rhs) noexcept {
  constexpr std::size_t len = lcm(N, M);
  return lhs.template tile<len>() * rhs.template tile<len>();
}
template <std::size_t N, std::size_t M>
  requires(N != M)
[[nodiscard]] constexpr Rhythm<lcm(N, M)> operator+(
    const Rhythm<N>& lhs,
    const Rhythm<M>& rhs) noexcept {
  constexpr std::size_t len = lcm(N, M);
  return lhs.template tile<len>() + rhs.template tile<len>();
}
template <std::size_t N, std::size_t M>
  requires(N != M)
[[nodiscard]] constexpr Rhythm<lcm(N, M)> operator^(
    const Rhythm<N>& lhs,
    const Rhythm<M>& rhs) noexcept {
  constexpr std::size_t len = lcm(N, M);
  return lhs.template tile<len>() ^ rhs.template tile<len>();
}
template <std::size_t N, std::size_t M>
  requires(N != M)
[[nodiscard]] constexpr Rhythm<lcm(N, M)> operator-(
    const Rhythm<N>& lhs,
    const Rhythm<M>& rhs) noexcept {
  constexpr std::size_t len = lcm(N, M);
  return lhs.template tile<len>() - rhs.template tile<len>();
}

// concatenation: Rhythm<N> | Rhythm<M> -> Rhythm<N+M>
template <std::size_t N, std::size_t M>
[[nodiscard]] constexpr Rhythm<N + M> operator|(const Rhythm<N>& lhs,
                                                const Rhythm<M>& rhs) noexcept {
  Rhythm<N + M> result;
  for (std::size_t i = 0; i < N; ++i)
    result.set(i, lhs[i]);
  for (std::size_t i = 0; i < M; ++i)
    result.set(N + i, rhs[i]);
  return result;
}

}  // namespace automata
