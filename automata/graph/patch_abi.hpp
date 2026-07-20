#pragma once

#include <cstdint>

#include "automata/config.hpp"
#include "automata/core/hash.hpp"
#include "automata/graph/builder.hpp"
#include "automata/graph/kernel_info.hpp"

// The patch-library ABI (ADR 0001): one exported stamp the host verifies
// before trusting the library, and one describe entry point that appends to a
// host-owned builder. Host and patch must agree on toolchain, CRT, config,
// and the layout of every type that crosses the boundary; the build hash
// folds all of them so a mismatch is a refused load, not a crash.

// Exported symbol spellings — single source of truth for the AUTOMATA_PATCH
// macro and the host's symbol lookups.
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define AUTOMATA_PATCH_STAMP_SYMBOL automata_patch_stamp
#define AUTOMATA_PATCH_DESCRIBE_SYMBOL automata_patch_describe
#define ATM_PATCH_STRINGIZE_IMPL(x) #x
#define ATM_PATCH_STRINGIZE(x) ATM_PATCH_STRINGIZE_IMPL(x)

#if defined(_WIN32)
#define AUTOMATA_PATCH_EXPORT __declspec(dllexport)
#else
#define AUTOMATA_PATCH_EXPORT __attribute__((visibility("default")))
#endif
// NOLINTEND(cppcoreguidelines-macro-usage)

namespace automata {

inline constexpr char PatchStampSymbol[] =
    ATM_PATCH_STRINGIZE(AUTOMATA_PATCH_STAMP_SYMBOL);
inline constexpr char PatchDescribeSymbol[] =
    ATM_PATCH_STRINGIZE(AUTOMATA_PATCH_DESCRIBE_SYMBOL);

// Runs inside the patch library; the builder lives in the host.
using PatchDescribeFn = void (*)(GraphBuilder*);

struct PatchStamp {
  std::uint32_t magic = 0;
  std::uint32_t abi = 0;  // bumped when the describe interface changes
  Hash build = 0;         // toolchain + config + boundary-type layouts
};

inline constexpr std::uint32_t PatchMagic = 0x414d5441;  // "ATMA"
inline constexpr std::uint32_t PatchAbiVersion = 1;

[[nodiscard]] consteval PatchStamp current_patch_stamp() {
  Hash h = HashSeed;
#if defined(__VERSION__)
  h = hash_string(__VERSION__, h);
#elif defined(_MSC_FULL_VER)
  h = hash_value(_MSC_FULL_VER, h);
#endif
#if defined(_MSC_VER)
  h = hash_value(_MSC_VER, h);
#endif
#if defined(_ITERATOR_DEBUG_LEVEL)
  h = hash_value(_ITERATOR_DEBUG_LEVEL, h);
#endif
#if defined(_DLL)
  h = hash_value(1, h);  // dynamic CRT (ADR 0001 requires it on Windows)
#endif
  h = hash_value(sizeof(void*), h);

  h = hash_value(SampleRate, h);
  h = hash_value(BlockSize, h);
  h = hash_value(MaxGraphNodes, h);
  h = hash_value(TapBufferSamples, h);
  h = hash_value(DefaultValueRampSeconds, h);

  h = hash_value(sizeof(GraphDef), h);
  h = hash_value(sizeof(GraphDef::Node), h);
  h = hash_value(sizeof(GraphBuilder), h);
  h = hash_value(sizeof(KernelInfo), h);

  return {.magic = PatchMagic, .abi = PatchAbiVersion, .build = h};
}

[[nodiscard]] constexpr bool stamp_compatible(const PatchStamp& stamp) {
  constexpr PatchStamp Current = current_patch_stamp();
  return stamp.magic == Current.magic && stamp.abi == Current.abi &&
         stamp.build == Current.build;
}

}  // namespace automata
