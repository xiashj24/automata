#pragma once

#include "automata/graph/builder.hpp"
#include "automata/graph/patch_abi.hpp"
#include "automata/graph/tap.hpp"
#include "automata/ugens/ugens.hpp"

// The single header a patch includes (ADR 0001). AUTOMATA_PATCH(g) defines
// the patch body — plain describe code against the builder g — and exports
// the entry point plus the ABI stamp the host verifies before trusting the
// library. It appears exactly once per hot-reloadable library, as its root;
// sub-patches are ordinary functions taking and returning Signals.

// The ActiveGraph scope is installed here, inside the library, so factories
// and scalar lifts compiled into the patch resolve to the same context.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define AUTOMATA_PATCH(g)                                               \
  static void automata_patch_body(::automata::GraphBuilder&);           \
  extern "C" AUTOMATA_PATCH_EXPORT const ::automata::PatchStamp         \
      AUTOMATA_PATCH_STAMP_SYMBOL = ::automata::current_patch_stamp();  \
  extern "C" AUTOMATA_PATCH_EXPORT void AUTOMATA_PATCH_DESCRIBE_SYMBOL( \
      ::automata::GraphBuilder* builder) {                              \
    const ::automata::ActiveGraph scope(*builder);                      \
    automata_patch_body(*builder);                                      \
  }                                                                     \
  static void automata_patch_body(::automata::GraphBuilder& g)
