#pragma once

#include <expected>

// Runtime-fallible control-path operations return Result<T>; per-operation
// doc comments list the codes they produce. Contract violations are
// atm_assert, expected absences are try_*/optional — never Error.

namespace automata {

enum class Error {
  FileCreateFailed,
  EncodeFailed,
  FileCopyFailed,
  LibraryLoadFailed,
  SymbolNotFound,
  AbiMismatch,
  DeviceInitFailed,
  DeviceStartFailed,
};

template <typename T>
using Result = std::expected<T, Error>;

}  // namespace automata
