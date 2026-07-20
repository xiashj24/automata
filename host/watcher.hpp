#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <system_error>
#include <utility>

// Poll-based change detection for the built patch library. A change fires
// only once its (mtime, size) signature holds steady for a second poll — on
// the first sighting the linker may still be writing. poll() keeps firing
// until mark_handled(), so a transient load failure retries naturally; the
// initial load flows through the same path.

namespace automata::host {

class FileWatcher {
public:
  explicit FileWatcher(std::filesystem::path path) : path_(std::move(path)) {}

  [[nodiscard]] bool poll() {
    const std::optional<Signature> sig = read_signature();
    if (!sig.has_value()) {
      return false;
    }
    if (sig == handled_) {
      return false;
    }
    if (sig != pending_) {
      pending_ = sig;  // first sighting; wait for it to settle
      return false;
    }
    return true;
  }

  void mark_handled() { handled_ = pending_; }

private:
  struct Signature {
    std::filesystem::file_time_type mtime;
    std::uintmax_t size = 0;
    bool operator==(const Signature&) const = default;
  };

  [[nodiscard]] std::optional<Signature> read_signature() const {
    std::error_code ec;
    const auto mtime = std::filesystem::last_write_time(path_, ec);
    if (ec) {
      return std::nullopt;
    }
    const auto size = std::filesystem::file_size(path_, ec);
    if (ec) {
      return std::nullopt;
    }
    return Signature{.mtime = mtime, .size = size};
  }

  std::filesystem::path path_;
  std::optional<Signature> pending_;
  std::optional<Signature> handled_;
};

}  // namespace automata::host
