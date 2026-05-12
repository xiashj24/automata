#pragma once

#include <concepts>
#include <numeric>

namespace automata {

template <std::integral T>
[[nodiscard]] constexpr T gcd(T a, T b) noexcept {
  while (b != 0) {
    T remainder = a % b;
    a = b;
    b = remainder;
  }
  return a;
}

template <std::integral T>
[[nodiscard]] constexpr T lcm(T a, T b) noexcept {
  if (a == 0 || b == 0)
    return 0;
  return (a / gcd(a, b)) * b;
}

}  // namespace automata
