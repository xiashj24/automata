# automata C++ style

Adapted from xhal's style rulebook (the project owner's modern-C++ notes),
with the embedded constraints replaced by real-time-audio constraints. Where
an ADR governs a topic, the ADR wins.

## Files and naming

- `.hpp` / `.cpp` extensions; `#pragma once`; forward declarations where
  suitable; avoid `extern` variable exports.
- All library code lives in `namespace automata`; internal helpers in
  `automata::detail`. No `using namespace` at global scope in headers
  (patch `.cpp` files may use it inside their entry point — authoring
  ergonomics is the product there).
- Types (classes, structs, enums, aliases, concepts): `PascalCase`.
- Functions and variables: `snake_case`; private data members take a
  trailing underscore (`head_`).
- Scoped-enum values: `PascalCase` (`Quantize::NextBar`); always
  `enum class`, never plain enums.
- `inline constexpr` globals and class constants: `PascalCase`.
- No abbreviations except universal ones (min, max, freq, lfo, svf — the
  audio-DSP vocabulary counts as universal here).
- Prefer specific names over general ones; name operations after the
  problem domain, not after data movement (avoid trivial getters/setters).

## API design

- Make misuse fail to compile: strong types over primitive soup;
  `enum class` instead of `bool` parameters; `explicit` on single-argument
  constructors.
- Configuration is a struct with good defaults, filled with designated
  initializers.
- Boundaries take views: `std::span` over pointer+size, `std::string_view`
  over `const char*`.
- `[[nodiscard]]` on getters and every fallible operation.
- Rule of 0. If a destructor is unavoidable, Rule of 5 — `=default` or
  `=delete` the rest.
- Prefer default arguments over overload sets.
- No singletons: the caller constructs and owns engine objects. The one
  sanctioned exception is the scoped active-graph context during patch
  describe (ADR 0005).
- User-defined literals for physical quantities (`_hz`, `_ms`, `_db`) as
  the corresponding types land.

## Modern C++ usage

- Standard algorithms and ranges over hand-rolled loops — except inside
  per-sample DSP inner loops, where a plain indexed loop is the idiom.
- In-class member initializers instead of ctor-only default constructors;
  initialize members in declaration order.
- Don't declare a variable until you have its value; keep scopes small.
- `const` everything that isn't `constexpr`; `noexcept` where correct and
  useful; `const` member functions whenever possible.
- `switch` on enums: cases return, no `default` (keeps `-Wswitch`
  exhaustive), `std::unreachable()` for impossible paths.
- `std::to_underlying`, not `static_cast`, to convert a scoped enum to its
  underlying type.
- Generate lookup tables with `constexpr`/`consteval` functions instead of
  baking in large constant tables.
- Mutable global state: strongly avoid. Header globals are
  `inline constexpr`; a genuinely necessary mutable global is `constinit`.

## Control flow

- Braces on every `if`/`while`/`for` body — single exception: one-line
  error checks, e.g. `if (!v.has_value()) return;`.
- No cascading assignments (`a = b = 0`).
- No side-effecting unary operators inside conditions (`if (--count)`).

## Real-time constraints (ADR 0003)

- Code reachable from `Engine::render` never allocates, frees, locks,
  logs, throws, or blocks. No `std::vector`/`std::string`/`std::function`
  there — `std::array`, `RingBuffer`, `LockfreeQueue`, arenas, spans, and
  plain function pointers only.
- Control-thread code (describe, Graph build, host) may allocate freely;
  prefer fixed capacity where it is natural anyway.
- Every cross-thread structure documents its contract in its header
  comment; SPSC is the only sanctioned lock-free shape.
- Kernels are self-contained, trivially copyable objects (ADR 0005):
  state and coefficients are private members; the API is `process` and
  `reset` plus kernel-specific parameter setters, no pointers, no
  allocation; state relocation is a memcpy of the object.

## Error handling (three tiers, xhal ADR 0003 lineage)

- Impossible states are compile errors: concepts and `static_assert`.
- Contract violations are asserted (`atm_assert`), never returned; asserts
  compile out under NDEBUG.
- Runtime-fallible control-path operations return
  `Result<T> = std::expected<T, Error>`, `[[nodiscard]]`; one shared
  `Error` enum, per-operation doc comments list the codes it produces.
- Expected absences (queue empty, no message) are `try_*`/`std::optional`,
  not errors.
- Library code never throws; host tests may enable exceptions (Catch2).

## Comments

- Comments express intent — why the code is the way it is — never
  implementation details the code already shows.
- Keep comments concise: 1–2 lines. If it takes more, the explanation
  belongs in a doc/commit message, not inline.
- Don't narrate cross-file plumbing ("enabled in X, called from Y, see Z"),
  justify a change to a reviewer, or recount the bug that prompted it.
- Public API doc comments may be longer, but only to state the caller's
  contract — not the internals.

## Formatting

- `.clang-format` at the repo root is the single source of truth — run
  clang-format on every C++ file you create or edit.

## Testing

- Catch2; a `foo.test.cpp` beside every `foo.hpp` it covers, auto-globbed
  into one test target each by CMake.
- The hot-swap invariants are the executable spec (state preserved across
  matching generations, reset when structure differs, value patches ramp,
  offline render is deterministic) — they are the tests that must never be
  weakened to make a refactor pass.
