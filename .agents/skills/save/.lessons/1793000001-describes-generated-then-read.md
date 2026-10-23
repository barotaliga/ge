---
title: Write a big set of describes from the struct's members, then read every one
date: 2026-10-23
scope: plugins/skier/skier.hpp, games/fall_line/sim/state.hpp, engine/ai/hands.hpp
concepts: [describe, hidden-friend, generate, members, skip, shared-ptr, pointer]
---

Fall Line's run holds about sixty state types. Their describes were written by a scratch script
that listed each struct's data members in order into a hidden-friend
`template <class Io> friend void describe(Io& io, T& v)` at the struct's end, skipping raw
pointers and the members the setup rebuilds (`GameState::level`, `runs`, `lifts`, `trees`,
`spec`, `ways`, `why`; `Driver::why_`). Then every describe was read against its struct: a
derived struct describes its base first (`io(static_cast<Base&>(v))`), a member whose schema
is the game's (the tape's `schema_`) is set at creation and left out. A hidden friend needs no
include of `save/io.hpp`; a `GE_SAVE_PLAIN` mark does, so a plugin that marks a type plain
declares the `save` engine target (`ge_add_plugin(... ENGINE ... save)`).
