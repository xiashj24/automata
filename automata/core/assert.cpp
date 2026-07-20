#include "automata/core/assert.hpp"

#include <cstdio>

// --- resumable debug-break primitive (mirrors C++26 std::breakpoint, P2514) --
#if defined(_MSC_VER)  // MSVC and clang-cl
extern void __cdecl __debugbreak(void);
#define ATM_DEBUG_BREAK() __debugbreak()
#elif defined(__has_builtin) && __has_builtin(__builtin_debugtrap)
// clang, gcc >= 12 — resumable
#define ATM_DEBUG_BREAK() __builtin_debugtrap()
#elif defined(__i386__) || defined(__x86_64__)
#define ATM_DEBUG_BREAK() __asm__ __volatile__("int3")
#elif defined(__aarch64__)
#define ATM_DEBUG_BREAK() __asm__ __volatile__("brk #0")
#else
#include <csignal>
#define ATM_DEBUG_BREAK() std::raise(SIGTRAP)
#endif

namespace automata {

void assert_failed(const char* expr, std::source_location loc) noexcept {
  // %lu + cast: line() is uint_least32_t, whose underlying type varies.
  (void)std::fprintf(stderr, "atm_assert failed: %s\n  at %s:%lu (%s)\n", expr,
                     loc.file_name(), static_cast<unsigned long>(loc.line()),
                     loc.function_name());
  ATM_DEBUG_BREAK();
}

}  // namespace automata
