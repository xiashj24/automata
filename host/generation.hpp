#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

#include "automata/core/error.hpp"
#include "automata/graph/patch_abi.hpp"

// DLL generations (ADR 0001): each load is a shadow copy of the built patch
// library, mapped for as long as anything references it. The pool holds one
// reference per resident generation; collect() unloads whatever nobody else
// still references. Declare the pool before the Engine in the host so every
// generation outlives the Graphs built from it.

namespace automata::host {

class Generation {
public:
  ~Generation();  // unmaps the module and deletes its shadow copy
  Generation(const Generation&) = delete;
  Generation& operator=(const Generation&) = delete;

  [[nodiscard]] PatchDescribeFn describe() const { return describe_; }
  [[nodiscard]] std::uint64_t id() const { return id_; }

private:
  friend class GenerationPool;
  Generation() = default;

  void* module_ = nullptr;
  std::filesystem::path shadow_;
  PatchDescribeFn describe_ = nullptr;
  std::uint64_t id_ = 0;
};

using GenerationRef = std::shared_ptr<Generation>;

class GenerationPool {
public:
  GenerationPool();   // shadow copies live in a per-process temp directory
  ~GenerationPool();  // unloads every generation still resident
  GenerationPool(const GenerationPool&) = delete;
  GenerationPool& operator=(const GenerationPool&) = delete;

  // Shadow-copies and maps the library, then verifies its ABI stamp.
  // Errors: FileCopyFailed, LibraryLoadFailed, SymbolNotFound, AbiMismatch.
  [[nodiscard]] Result<GenerationRef> load(const std::filesystem::path& dll);

  // Unloads generations only the pool still references (control thread).
  void collect();

  [[nodiscard]] std::size_t resident() const { return resident_.size(); }

private:
  std::filesystem::path shadow_dir_;
  std::uint64_t counter_ = 0;
  std::vector<GenerationRef> resident_;
};

}  // namespace automata::host
