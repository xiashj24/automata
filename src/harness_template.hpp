#pragma once
#include <string_view>

namespace automata {

// emit() serializes a Rhythm<N> to stdout as a length/bits pair.
// {USER_CODE} is replaced with the .rhythm file content.
// {MAIN_BODY} is replaced with either:
//   - std::apply over tracks() if a tracks() function is found, or
//   - individual emit(track_*()) calls discovered by name scanning.

// clang-format off
constexpr std::string_view harness_template = R"cpp(
#include <rhythm.hpp>
#include <cstddef>
#include <iostream>
#include <tuple>
using namespace automata;

template <std::size_t N>
void emit(const Rhythm<N>& rhythm) {
    std::cout << rhythm.length() << "\n";
    for (std::size_t i = 0; i < N; ++i)
        std::cout << (rhythm[i] ? '1' : '0');
    std::cout << "\n";
}

// --- USER CODE ---
{USER_CODE}
// --- END USER CODE ---

int main() {
{MAIN_BODY}
}
)cpp";
// clang-format on

}  // namespace automata
