#include "host/generation.hpp"

#include <string>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace automata::host {

namespace {

[[nodiscard]] void* open_library(const std::filesystem::path& path) {
#if defined(_WIN32)
  return ::LoadLibraryW(path.c_str());
#else
  return ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

[[nodiscard]] void* find_symbol(void* module, const char* name) {
#if defined(_WIN32)
  return reinterpret_cast<void*>(
      ::GetProcAddress(static_cast<HMODULE>(module), name));
#else
  return ::dlsym(module, name);
#endif
}

void close_library(void* module) {
#if defined(_WIN32)
  ::FreeLibrary(static_cast<HMODULE>(module));
#else
  ::dlclose(module);
#endif
}

[[nodiscard]] unsigned long current_pid() {
#if defined(_WIN32)
  return ::GetCurrentProcessId();
#else
  return static_cast<unsigned long>(::getpid());
#endif
}

}  // namespace

Generation::~Generation() {
  if (module_ != nullptr) {
    close_library(module_);
  }
  std::error_code ec;
  std::filesystem::remove(shadow_, ec);  // best-effort
}

GenerationPool::GenerationPool() {
  shadow_dir_ = std::filesystem::temp_directory_path() /
                ("automata-shadow-" + std::to_string(current_pid()));
  std::filesystem::create_directories(shadow_dir_);
}

GenerationPool::~GenerationPool() {
  resident_.clear();
  std::error_code ec;
  std::filesystem::remove_all(shadow_dir_, ec);
}

Result<GenerationRef> GenerationPool::load(const std::filesystem::path& dll) {
  ++counter_;
  const std::filesystem::path shadow =
      shadow_dir_ / (std::to_string(counter_) + "-" + dll.filename().string());

  std::error_code ec;
  std::filesystem::copy_file(
      dll, shadow, std::filesystem::copy_options::overwrite_existing, ec);
  if (ec) {
    return std::unexpected(Error::FileCopyFailed);
  }

  GenerationRef gen{new Generation};
  gen->shadow_ = shadow;
  gen->id_ = counter_;
  gen->module_ = open_library(shadow);
  if (gen->module_ == nullptr) {
    return std::unexpected(Error::LibraryLoadFailed);
  }

  const auto* stamp = static_cast<const PatchStamp*>(
      find_symbol(gen->module_, PatchStampSymbol));
  void* describe = find_symbol(gen->module_, PatchDescribeSymbol);
  if (stamp == nullptr || describe == nullptr) {
    return std::unexpected(Error::SymbolNotFound);
  }
  if (!stamp_compatible(*stamp)) {
    return std::unexpected(Error::AbiMismatch);
  }
  gen->describe_ = reinterpret_cast<PatchDescribeFn>(describe);

  resident_.push_back(gen);
  return gen;
}

void GenerationPool::collect() {
  std::erase_if(resident_,
                [](const GenerationRef& gen) { return gen.use_count() == 1; });
}

}  // namespace automata::host
