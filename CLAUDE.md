# automata: live-coding audio synthesis engine (C++23)

Patches are C++ compiled to a shared library and hot-reloaded; DSP state
survives edits. Core design: docs/architecture.md.

Design decisions are recorded in docs/adr/ — read the relevant ADR before
changing an interface, and add a new one when settling an open decision.
The founding set: 0001 patch boundary & generation lifetime, 0002 identity
& reconciliation, 0003 threads & real-time discipline, 0004 execution &
time base, 0005 kernel/UGen authoring.

## Non-negotiable properties

- State-preserving hot-swap: a live edit never resets DSP memory that
  structurally survived it.
- Real-time safety by construction: nothing reachable from
  `Engine::render` allocates, frees, locks, logs, or blocks.
- Adding a UGen is almost free: a kernel class + a one-line factory.
- Bounded hot-reload footprint: generations are reclaimed, never
  accumulated.
- Offline rendering shares the live render path exactly.

## Designing a UGen or engine change

- Adding a UGen follows the /new-ugen skill
  (.claude/skills/new-ugen/SKILL.md): prior-art survey, shape choice,
  unit and cached-setter idioms, probe registration, tests, live smoke.
- Survey the reference submodules in reference/ for prior art first
  (SuperCollider, Csound, Faust libraries, Gamma, DaisySP, Soundpipe,
  signalsmith, sst-*, …) and synthesize approaches before writing code.
  Reference code is never linked or copied wholesale — kernels are written
  dependency-free in the project's own convention (ADR 0005).

## Style

- docs/style.md is the C++ style and API-design rulebook — follow it for
  every C++ file you create or edit.
- .clang-format at the repo root is the single source of truth — run
  clang-format on every C++ file you create or edit.

## Code comments

- Comments express intent — why the code is the way it is — never
  implementation details the code already shows.
- Keep comments concise: 1–2 lines; longer explanations belong in docs or
  commit messages, not inline.
- Don't narrate cross-file plumbing ("enabled in X, called from Y, see Z"),
  justify a change to a reviewer, or recount the bug that prompted it.
- Public API doc comments may be longer, but only to state the caller's
  contract — not the internals.
