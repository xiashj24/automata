#pragma once
#include <string_view>

namespace automata {

// The harness wraps the user's rhythm_exprs() function. emit() serializes each
// rhythm to stdout as a length/bits pair; main() calls rhythm_exprs().
// {USER_CODE} is replaced with the content of the user's expression file.

// clang-format off
constexpr std::string_view harness_template = R"cpp(
#include <rhythm.hpp>
#include <cstddef>
#include <iostream>
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
    rhythm_exprs();
}
)cpp";
// clang-format on

}  // namespace automata
