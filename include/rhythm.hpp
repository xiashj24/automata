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
  constexpr explicit Rhythm(std::string_view s) noexcept {
    for (std::size_t i = 0; i < s.size() && i < N; ++i)
      bits.set(i, s[i] == 'x' || s[i] == 'X' || s[i] == '1');
  }

  // integer bit pattern: bit 0 = first step (e.g. 0b1010, 0x1A)
  constexpr explicit Rhythm(std::uint64_t val) noexcept {
    for (std::size_t i = 0; i < N; ++i)
      bits.set(i, (val >> i) & 1u);
  }

  constexpr bool get(std::size_t i) const noexcept { return bits.test(i % N); }
  constexpr void set(std::size_t i, bool v = true) noexcept { bits.set(i, v); }
  constexpr std::size_t steps() const noexcept { return N; }

  // boolean operators (same length, result keeps length)
  constexpr Rhythm operator&(const Rhythm& o) const noexcept {
    return {bits & o.bits};
  }
  constexpr Rhythm operator|(const Rhythm& o) const noexcept {
    return {bits | o.bits};
  }
  constexpr Rhythm operator~() const noexcept { return {~bits}; }
  constexpr Rhythm operator-(const Rhythm& o) const noexcept {
    return {bits & ~o.bits};
  }
  constexpr Rhythm operator^(const Rhythm& o) const noexcept {
    return {bits ^ o.bits};
  }

  // dac: pack the first B steps into a uint32_t (bit 0 = LSB)
  template <std::size_t B>
  constexpr uint32_t dac() const noexcept {
    static_assert(B <= 32, "bit depth must be <= 32");
    constexpr std::size_t len = B < N ? B : N;
    uint32_t result = 0;
    for (std::size_t i = 0; i < len; ++i)
      result |= static_cast<uint32_t>(bits.test(i)) << i;
    return result;
  }

  // rotation (wrapping shift)
  constexpr Rhythm operator<<(std::size_t n) const noexcept {
    n %= N;
    return {(bits << n) | (bits >> (N - n))};
  }
  constexpr Rhythm operator>>(std::size_t n) const noexcept {
    n %= N;
    return {(bits >> n) | (bits << (N - n))};
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
constexpr Rhythm<N + M> operator+(const Rhythm<N>& a,
                                  const Rhythm<M>& b) noexcept {
  Rhythm<N + M> r;
  for (std::size_t i = 0; i < N; ++i)
    r.set(i, a.get(i));
  for (std::size_t i = 0; i < M; ++i)
    r.set(N + i, b.get(i));
  return r;
}

// interleave: zip K rhythms step by step -> Rhythm<N*K>
// result[j*K + k] = inputs[k][j]
template <std::size_t N, std::size_t... Ms>
  requires(... && (Ms == N))
constexpr Rhythm<N * (1 + sizeof...(Ms))> interleave(
    const Rhythm<N>& first,
    const Rhythm<Ms>&... rest) noexcept {
  constexpr std::size_t K = 1 + sizeof...(rest);
  Rhythm<N * K> r;
  std::size_t k = 0;
  auto write = [&](const Rhythm<N>& x) noexcept {
    for (std::size_t j = 0; j < N; ++j)
      r.set(j * K + k, x.get(j));
    ++k;
  };
  write(first);
  (write(rest), ...);
  return r;
}

}  // namespace automata
