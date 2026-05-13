#include "compiler.hpp"
#include "harness_template.hpp"
#include "ppm.hpp"
#include "renderer.hpp"

#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifndef AUTOMATA_INCLUDE_DIR
#error "AUTOMATA_INCLUDE_DIR must be defined by CMake"
#endif

static std::optional<std::string> read_file(const char* path) {
  std::ifstream file(path);
  if (!file)
    return std::nullopt;
  return std::string{std::istreambuf_iterator<char>(file),
                     std::istreambuf_iterator<char>()};
}

static std::string build_harness(std::string_view user_code) {
  const std::string placeholder = "{USER_CODE}";
  std::string source{automata::harness_template};
  const auto pos = source.find(placeholder);
  source.replace(pos, placeholder.size(), user_code);
  return source;
}

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: automata-render <input.cpp> <output.ppm>\n";
    return 1;
  }

  const auto user_code = read_file(argv[1]);
  if (!user_code) {
    std::cerr << "automata-render: cannot open '" << argv[1] << "'\n";
    return 1;
  }

  const std::string harness_src = build_harness(*user_code);
  const automata::HarnessResult result =
      automata::run_harness(harness_src, AUTOMATA_INCLUDE_DIR);

  if (!result.success) {
    std::cerr << "automata-render: compile/run error:\n"
              << result.error_output << "\n";
    return 1;
  }

  const automata::RenderConfig cfg;
  const automata::RenderOutput output =
      automata::render(result.rhythm_bits_list, cfg);

  std::ofstream ppm_file(argv[2], std::ios::binary);
  if (!ppm_file) {
    std::cerr << "automata-render: cannot write '" << argv[2] << "'\n";
    return 1;
  }
  automata::write_ppm(ppm_file, output.pixels, output.width, output.height);

  std::cout << result.rhythm_bits_list.size() << " rhythm(s) (" << output.width
            << "x" << output.height << " px) -> " << argv[2] << "\n";
  return 0;
}
