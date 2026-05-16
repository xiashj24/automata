#pragma once

namespace automata {
namespace literals {

[[nodiscard]] constexpr float operator""_hz(long double v) noexcept {
    return static_cast<float>(v);
}
[[nodiscard]] constexpr float operator""_hz(unsigned long long v) noexcept {
    return static_cast<float>(v);
}
[[nodiscard]] constexpr float operator""_khz(long double v) noexcept {
    return static_cast<float>(v * 1000.0L);
}
[[nodiscard]] constexpr float operator""_khz(unsigned long long v) noexcept {
    return static_cast<float>(v * 1000ULL);
}
[[nodiscard]] constexpr float operator""_bpm(long double v) noexcept {
    return static_cast<float>(v);
}
[[nodiscard]] constexpr float operator""_bpm(unsigned long long v) noexcept {
    return static_cast<float>(v);
}

}  // namespace literals
}  // namespace automata
