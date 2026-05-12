#pragma once
#include <bitset>
#include <cstdint>
#include <cstddef>
#include <string_view>

namespace automata {

template <std::size_t N>
class Rhythm {
public:
  constexpr Rhythm() noexcept = default;

  // TODO: eulcid constructor (or factory?)

  // "x.x.xxx.x." — 'x'/'X'/'1' = hit, anything else = rest
  constexpr explicit Rhythm(std::string_view sv) noexcept {
    for (std::size_t i = 0; i < sv.size() && i < N; ++i)
      bits.set(i, sv[i] == 'x' || sv[i] == 'X' || sv[i] == '1');
  }

  // integer bit pattern: bit 0 = first step (e.g. 0b1010, 0x1A)
  constexpr explicit Rhythm(std::uint64_t val) noexcept {
    for (std::size_t i = 0; i < N; ++i)
      bits.set(i, (val >> i) & 1u);
  }

  [[nodiscard]] constexpr bool get(std::size_t i) const noexcept { return bits.test(i % N); }
  constexpr void set(std::size_t i, bool v = true) noexcept { bits.set(i, v); }
  [[nodiscard]] constexpr std::size_t steps() const noexcept { return N; }

  // boolean operators (same length, result keeps length)
  [[nodiscard]] constexpr Rhythm operator&(const Rhythm& rhs) const noexcept {
    return Rhythm(bits & rhs.bits);
  }
  [[nodiscard]] constexpr Rhythm operator|(const Rhythm& rhs) const noexcept {
    return Rhythm(bits | rhs.bits);
  }
  [[nodiscard]] constexpr Rhythm operator~() const noexcept { return Rhythm(~bits); }
  [[nodiscard]] constexpr Rhythm operator-(const Rhythm& rhs) const noexcept {
    return Rhythm(bits & ~rhs.bits);
  }
  [[nodiscard]] constexpr Rhythm operator^(const Rhythm& rhs) const noexcept {
    return Rhythm(bits ^ rhs.bits);
  }

  // dac: pack the first B steps into a uint32_t (bit 0 = LSB)
  template <std::size_t B>
  [[nodiscard]] constexpr uint32_t dac() const noexcept {
    static_assert(B <= 32, "bit depth must be <= 32");
    constexpr std::size_t len = B < N ? B : N;
    uint32_t result = 0;
    for (std::size_t i = 0; i < len; ++i)
      result |= static_cast<uint32_t>(bits.test(i)) << i;
    return result;
  }

  // rotation (wrapping shift)
  [[nodiscard]] constexpr Rhythm operator<<(std::size_t n) const noexcept {
    n %= N;
    return Rhythm((bits << n) | (bits >> (N - n)));
  }
  [[nodiscard]] constexpr Rhythm operator>>(std::size_t n) const noexcept {
    n %= N;
    return Rhythm((bits >> n) | (bits << (N - n)));
  }

private:
  std::bitset<N> bits{};

  explicit constexpr Rhythm(std::bitset<N> b) noexcept : bits(b) {}

  template <std::size_t M>
  friend class Rhythm;

  template <std::size_t A, std::size_t B>
  friend constexpr Rhythm<A + B> operator+(const Rhythm<A>&,
                                           const Rhythm<B>&) noexcept;
};

// concatenation: Rhythm<N> + Rhythm<M> -> Rhythm<N+M>
template <std::size_t N, std::size_t M>
[[nodiscard]] constexpr Rhythm<N + M> operator+(const Rhythm<N>& lhs,
                                                const Rhythm<M>& rhs) noexcept {
  Rhythm<N + M> result;
  for (std::size_t i = 0; i < N; ++i)
    result.set(i, lhs.get(i));
  for (std::size_t i = 0; i < M; ++i)
    result.set(N + i, rhs.get(i));
  return result;
}

// interleave: zip K rhythms step by step -> Rhythm<N*K>
// result[j*K + k] = inputs[k][j]
template <std::size_t N, std::size_t... Ms>
  requires(... && (Ms == N))
[[nodiscard]] constexpr Rhythm<N * (1 + sizeof...(Ms))> interleave(
    const Rhythm<N>& first,
    const Rhythm<Ms>&... rest) noexcept {
  constexpr std::size_t count = 1 + sizeof...(rest);
  Rhythm<N * count> result;
  std::size_t chan = 0;
  auto write = [&](const Rhythm<N>& input) noexcept {
    for (std::size_t j = 0; j < N; ++j)
      result.set(j * count + chan, input.get(j));
    ++chan;
  };
  write(first);
  (write(rest), ...);
  return result;
}

}  // namespace automata
