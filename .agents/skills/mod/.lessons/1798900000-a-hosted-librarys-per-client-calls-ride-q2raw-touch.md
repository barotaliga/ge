---
title: A hosted game library's per-client calls ride Q2Raw::touch, and its fixture's statics outlive a load
date: 2026-11-22
scope: games/shooters_edge/server/q2_native.cpp
concepts: [game-library, getgameapiex, extended-game-interface, per-client, fixture, q2raw]
---

The frames of a hosted Quake II game library are written per slot by `q2_frame_raw` from one
shared `Q2Raw`, so a call the game answers per client (which entities a client sees, the state it
sees them in) cannot be made while the view is taken: set `Q2Raw::touch` (a plain function and a
context, null when the game has no such call) and `q2_frame_raw` asks it for each entity of each
slot's frame. Keep it null for a game without the calls, so the common path pays one compare.

The test library (`tests/q2_native_fixture_game.cpp`) stays loaded between tests in one process
(closing a shared library need not unload it), so every static the extended interface sets
(`gix`, flags) is reset in the stock entry point, and the extended table is offered only when a
setting asks for it (`fixture_ex 1`): otherwise its `CanSave` would replace the dead player's
check other tests hold.
