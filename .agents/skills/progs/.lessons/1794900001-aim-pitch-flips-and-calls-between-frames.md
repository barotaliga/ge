---
title: A direction's angles pitch the other way from a view's, and a QC call between frames goes through enter_qc
date: 2026-11-04
scope: games/shooters_edge/qc/campaign/, tests/shooters_edge_campaign_monsters_test.cpp
concepts: [qc, monsters, aim, vectoangles, makevectors, pitch, enter-qc, run-qc, mirror]
---

`vectoangles(dir)` gives a pitch that is positive UP, while `makevectors` reads a view's pitch,
positive DOWN: `makevectors(vectoangles(dir))` aims a missile up by as much as the target is
below, so a monster level with its target hits and one above it shoots at the ceiling. Turn the
pitch over first (`aim_along` in `campaign/ai.qc`). A test that wants one exact blow calls the
program's own `damage` between frames, but only between `se::enter_qc(q, sv)` and
`se::leave_qc(q)` (parameters in `pg::parm(i)`, `se::run_qc`): a bare `call_function` crashes in
the first builtin (no server bound), and setting `q.sv` by hand runs it but loses the think the
call set, because the next frame mirrors the loop's copy of `think`/`nextthink` back over it.
