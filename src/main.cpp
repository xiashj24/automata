#include "compiler.hpp"
#include "harness_template.hpp"
#include "ppm.hpp"
#include "renderer.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
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

// Returns true if the source defines a tracks() function.
static bool has_tracks_fn(std::string_view source) {
  const std::regex pattern{
      R"((?:constexpr\s+|inline\s+)*auto\s+tracks\s*\(\s*\))"};
  return std::regex_search(source.data(), source.data() + source.size(),
                           pattern);
}

// Finds all track_ function definitions in source order, deduplicated.
static std::vector<std::string> scan_track_fns(std::string_view source) {
  const std::regex pattern{
      R"((?:constexpr\s+|inline\s+)*auto\s+(track_[a-zA-Z_][a-zA-Z0-9_]*)\s*\(\s*\))"};
  std::vector<std::string> names;
  const auto begin = std::cregex_iterator(
      source.data(), source.data() + source.size(), pattern);
  for (auto it = begin; it != std::cregex_iterator{}; ++it) {
    std::string name = (*it)[1].str();
    if (std::ranges::find(names, name) == names.end())
      names.push_back(std::move(name));
  }
  return names;
}

static std::string build_harness(std::string_view user_code,
                                 std::string_view main_body) {
  std::string source{automata::harness_template};

  const std::string user_ph = "{USER_CODE}";
  source.replace(source.find(user_ph), user_ph.size(), user_code);

  const std::string body_ph = "{MAIN_BODY}";
  source.replace(source.find(body_ph), body_ph.size(), main_body);

  return source;
}

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    std::cerr << "Usage: automata-render <input.rhythm> [output.ppm]\n";
    return 1;
  }

  const std::string input_path = argv[1];
  const std::string output_path = [&] {
    if (argc == 3)
      return std::string{argv[2]};
    const auto dot = input_path.rfind('.');
    const std::string stem =
        dot == std::string::npos ? input_path : input_path.substr(0, dot);
    return stem + ".ppm";
  }();

  const auto user_code = read_file(input_path.c_str());
  if (!user_code) {
    std::cerr << "automata-render: cannot open '" << input_path << "'\n";
    return 1;
  }

  std::string main_body;
  if (has_tracks_fn(*user_code)) {
    main_body =
        "    std::apply([](const auto&... rhythms) {\n"
        "        (emit(rhythms), ...);\n"
        "    }, tracks());\n";
  } else {
    const auto names = scan_track_fns(*user_code);
    if (names.empty()) {
      std::cerr << "automata-render: define tracks() or track_* functions"
                   " in '"
                << input_path << "'\n";
      return 1;
    }
    for (const auto& name : names)
      main_body += "    emit(" + name + "());\n";
  }

  const std::string harness_src = build_harness(*user_code, main_body);
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

  std::ofstream ppm_file(output_path, std::ios::binary);
  if (!ppm_file) {
    std::cerr << "automata-render: cannot write '" << output_path << "'\n";
    return 1;
  }
  automata::write_ppm(ppm_file, output.pixels, output.width, output.height);

  std::cout << result.rhythm_bits_list.size() << " track(s) (" << output.width
            << "x" << output.height << " px) -> " << output_path << "\n";
  return 0;
}
