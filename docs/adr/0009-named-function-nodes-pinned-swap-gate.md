# ADR 0009: Named function nodes, and pinned defs always swap

## Status

Proposed

## Context

The first attempt let a patch wrap any free function or captureless lambda
into a stateless node (`StatelessNode<Fn>`), and it earned its keep: most
one-off ideas in a live set are a line of math, not a kernel. Its identity
came from `typeid` on the closure type — tolerable there, poison here:
lambda type names shift as the file is edited, and under ADR 0002 an
unstable leaf identity re-keys every node downstream, resetting state on
every recompile.

Porting the idea exposed a latent reconciler bug that already affects ADR
0005's custom class kernels: the value-patch fast path fires on def-hash
equality alone and discards the new def. A def whose nodes point into its
own generation — custom kernels, wrapped functions — can be edited
body-only without changing its hash; the fast path then keeps the old
graph running old code, and the edit silently never lands. Wrapped
functions would hit this constantly: tweaking the math inside the lambda
is the whole point of having one.

## Decision

- **`fn(name, f, inputs...)` wraps a captureless function into a node.**
  The name is the identity — `hash(base, name)`, the tap/param rule — so a
  body edit never disturbs downstream state, and renaming is deliberately
  structural. Arity (1–3 floats) folds into the base hash and is enforced
  by overload; the function-pointer parameter is the capture firewall — a
  capturing lambda fails to convert, so state can't sneak past the hashing
  and transfer machinery. The pointer rides the op bytes, exactly where
  bound-kernel setter pointers already live.
- **A wrapped function pins its generation.** Its `KernelInfo` is a
  template-instantiated static in the defining binary, so rebinding leaves
  it foreign (ADR 0001's custom-kernel path, no new machinery): the
  generation stays mapped while any graph using the node lives — bounded
  at one generation per live graph, the accepted custom-kernel deal.
- **A pinned def always swaps, never value-patches.** When an update
  arrives with an owner token, the reconciler skips the def-hash fast path
  and issues a full swap: the def carries code pointers into its own
  generation, and only a swap makes them live. Equal structure means the
  transfer plan matches every node, so all state carries over and the edit
  is as seamless as a value patch — Const glides included, since RampState
  transfers and the new values retarget it. This fixes the custom-kernel
  editing bug for class kernels and wrapped functions alike.

## Consequences

- (+) A one-line idea is a one-line node: `fn("fold", [](float x) {
  return x - std::floor(x + 0.5f); }, in)` — no struct, no factory, no
  registration.
- (+) Custom-kernel body edits now land; before this, an edit that kept
  the def hash was silently ignored.
- (−) Patches using `fn` always pay a swap on reload, even for pure value
  edits elsewhere in the patch — state transfer makes this inaudible, but
  the value-patch fast path is off for them.
- (−) Two `fn` nodes given the same name and arity are distinct nodes but
  identical identities; a swap may transfer between the wrong pair.
  Stateless, so harmless today — a debug-build name-collision check
  becomes worthwhile if fn ever grows state.
- (−) Captureless only: anything the function needs must arrive as an
  input Signal. That is the design working — captured state would be
  invisible to hashing and transfer.
