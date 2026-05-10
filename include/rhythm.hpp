#pragma once
#include <bitset>
#include <cstdint>
#include <cstddef>
#include <string_view>

namespace automata {

template <std::size_t N>
class Rhythm {
public:
  constexpr Rhythm() = default;

  // "x.x.xxx.x." — 'x'/'X'/'1' = hit, anything else = rest
  constexpr explicit Rhythm(std::string_view s) {
    for (std::size_t i = 0; i < s.size() && i < N; ++i)
      bits[i] = (s[i] == 'x' || s[i] == 'X' || s[i] == '1');
  }

  // integer bit pattern: bit 0 = first step (e.g. 0b1010, 0x1A)
  constexpr explicit Rhythm(std::uint64_t val) {
    for (std::size_t i = 0; i < N; ++i)
      bits[i] = (val >> i) & 1u;
  }

  constexpr bool operator[](std::size_t i) const { return bits[i]; }
  constexpr void set(std::size_t i, bool v = true) { bits[i] = v; }
  constexpr std::size_t steps() const { return N; }

  // boolean operators (same length, result keeps length)
  constexpr Rhythm operator*(const Rhythm& o) const { return {bits & o.bits}; }
  constexpr Rhythm operator+(const Rhythm& o) const { return {bits | o.bits}; }
  constexpr Rhythm operator!() const { return {~bits}; }
  constexpr Rhythm operator-(const Rhythm& o) const { return {bits & ~o.bits}; }
  constexpr Rhythm operator^(const Rhythm& o) const { return {bits ^ o.bits}; }

  // dac: pack the first B steps into a uint32_t (bit 0 = LSB)
  template <std::size_t B>
  constexpr uint32_t dac() const {
    static_assert(B <= 32, "bit depth must be <= 32");
    constexpr std::size_t steps = B < N ? B : N;
    uint32_t result = 0;
    for (std::size_t i = 0; i < steps; ++i)
      result |= static_cast<uint32_t>(bits[i]) << i;
    return result;
  }

  // rotation (wrapping shift)
  constexpr Rhythm operator<<(std::size_t n) const {
    n %= N;
    return {(bits << n) | (bits >> (N - n))};
  }
  constexpr Rhythm operator>>(std::size_t n) const {
    n %= N;
    return {(bits >> n) | (bits << (N - n))};
  }

private:
  std::bitset<N> bits{};

  explicit constexpr Rhythm(std::bitset<N> b) : bits(b) {}

  template <std::size_t M>
  friend class Rhythm;

  template <std::size_t A, std::size_t B>
  friend constexpr Rhythm<A + B> cat(const Rhythm<A>&, const Rhythm<B>&);
};

// concatenation: Rhythm<N> ++ Rhythm<M> -> Rhythm<N+M>
template <std::size_t N, std::size_t M>
constexpr Rhythm<N + M> cat(const Rhythm<N>& a, const Rhythm<M>& b) {
  Rhythm<N + M> r;
  for (std::size_t i = 0; i < N; ++i) {
    r.set(i, a[i]);
  }

  for (std::size_t i = 0; i < M; ++i) {
    r.set(N + i, b[i]);
  }

  return r;
}

}  // namespace automata
