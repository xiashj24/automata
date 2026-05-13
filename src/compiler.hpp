#pragma once
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace automata {

struct HarnessResult {
  bool success;
  std::string error_output;
  std::vector<std::string> rhythm_bits_list;  // one entry per emit() call
  std::vector<std::size_t> rhythm_lengths;
};

// Writes source to a temp .cpp, compiles with clang++, runs it, and parses all
// length/bits pairs from stdout (one per emit() call). Cleans up temp files.
inline HarnessResult run_harness(std::string_view source_cpp,
                                 std::string_view include_dir) {
  namespace fs = std::filesystem;

  const fs::path tmp_dir = fs::temp_directory_path();
  const fs::path src_path = tmp_dir / "automata_harness.cpp";
  const fs::path bin_path = tmp_dir / "automata_harness";
  const fs::path err_path = tmp_dir / "automata_harness_err.txt";
  const fs::path out_path = tmp_dir / "automata_harness_out.txt";

  {
    std::ofstream src_file(src_path);
    if (!src_file)
      return {false, "Failed to write temp source file"};
    src_file << source_cpp;
  }

  std::ostringstream compile_cmd;
  compile_cmd << "clang++ -std=c++23 -O0"
              << " -I" << include_dir << " " << src_path.string() << " -o "
              << bin_path.string() << " 2>" << err_path.string();

  if (std::system(compile_cmd.str().c_str()) != 0) {
    std::ifstream err_file(err_path);
    std::string err_msg((std::istreambuf_iterator<char>(err_file)),
                        std::istreambuf_iterator<char>());
    fs::remove(src_path);
    fs::remove(err_path);
    return {false, err_msg};
  }

  std::ostringstream run_cmd;
  run_cmd << bin_path.string() << " >" << out_path.string() << " 2>"
          << err_path.string();

  if (std::system(run_cmd.str().c_str()) != 0) {
    std::ifstream err_file(err_path);
    std::string err_msg((std::istreambuf_iterator<char>(err_file)),
                        std::istreambuf_iterator<char>());
    fs::remove(src_path);
    fs::remove(bin_path);
    fs::remove(err_path);
    fs::remove(out_path);
    return {false, "Harness binary failed: " + err_msg};
  }

  // Parse stdout: repeated pairs of (length\n)(bits\n)
  std::ifstream out_file(out_path);
  std::vector<std::string> bits_list;
  std::vector<std::size_t> lengths;
  std::size_t length = 0;
  std::string bits;
  while (out_file >> length >> bits) {
    if (bits.size() != length) {
      fs::remove(src_path);
      fs::remove(bin_path);
      fs::remove(err_path);
      fs::remove(out_path);
      return {false, "Harness output malformed"};
    }
    lengths.push_back(length);
    bits_list.push_back(std::move(bits));
  }

  fs::remove(src_path);
  fs::remove(bin_path);
  fs::remove(err_path);
  fs::remove(out_path);

  if (bits_list.empty())
    return {false, "rhythm_exprs() called emit() zero times"};

  return {true, "", bits_list, lengths};
}

}  // namespace automata
