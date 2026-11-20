---
title: A Quake II monster's volley is data in its Attack row; frame numbers from the notes rarely match the pose table's
date: 2026-11-20
scope: games/shooters_edge/server/q2_monster_kinds.hpp
concepts: [monster, attack, volley, every, loop, shots, again, again-skill, windup, pain-hold, frames]
---

Most of a monster's fire pattern fits `roster::Attack` without new code: shots `every` s on a
`loop`, `shots` a loop, `again` chance of another loop (`again_skill` its least skill,
`again_loop` the later loops' length), `windup` to the first shot, `lead` -0.2 to aim behind. A
blow can loop too (the flyer's two slashes). Write new rows with designated initializers: the
positional ones stop being readable past the sixth field. Turn a note's "fires at frame N" into
a windup as (N - the run's first frame) x 0.1 s only when the note's run starts where
`q2_monster_poses.hpp`'s does; the notes count frames by the original's frame list, and several
runs (the tank's machine gun, the berserker's spike) are not in the pose table at those numbers.
A pain's hold is set by `pose_monster` from the pain run it picks (0.1 s a frame); the 0.3 s in
`monster_damage` only covers a kind with no pain frames.
