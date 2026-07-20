#pragma once

#include <source_location>

// atm_assert(expr) checks a runtime invariant: debug builds report and break
// on failure; NDEBUG builds compile to nothing (expr is not evaluated).
// Override ATM_ASSERT_FAIL(expr) to change the failure action. Single macro
// only, as the migration point to C++26 contract_assert (P2900).

// Macros, not functions, for short-circuit + stringized condition.
// NOLINTBEGIN(cppcoreguidelines-macro-usage)

// --- enable flag (overridable; off under NDEBUG) ----------------------------
#ifndef ATM_ENABLE_ASSERTS
#ifdef NDEBUG
#define ATM_ENABLE_ASSERTS 0
#else
#define ATM_ENABLE_ASSERTS 1
#endif
#endif

namespace automata {

// Reports to stderr and breaks into the debugger. Not [[noreturn]]: lets the
// debugger resume past the breakpoint.
void assert_failed(
    const char* expr,
    std::source_location loc = std::source_location::current()) noexcept;

}  // namespace automata

// --- overridable failure action ---------------------------------------------
#ifndef ATM_ASSERT_FAIL
#define ATM_ASSERT_FAIL(expr) ::automata::assert_failed(expr)
#endif

// --- the assert macro --------------------------------------------------------
// Ternary, not `if`: dangling-else safe, usable as a statement with `;`.
#if ATM_ENABLE_ASSERTS
#define atm_assert(expr) \
  (static_cast<bool>(expr) ? void(0) : ATM_ASSERT_FAIL(#expr))
#else
#define atm_assert(expr) ((void)0)
#endif

// NOLINTEND(cppcoreguidelines-macro-usage)
