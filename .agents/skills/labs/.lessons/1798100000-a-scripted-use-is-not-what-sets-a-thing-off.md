---
title: A thing a program fires by its value answers no scripted use; watch who uses whom
date: 2026-11-24
scope: games/shooters_edge/labs/campaign_things.hpp
concepts: [things-lab, use-targets, counter, oncount, trace-hooks, scripted-look, false-idle]
---

The things lab first called each named thing's `use` with the player as the user, and many classes
looked idle: an oncount answers only its counter's value, a decoy trigger only the decoy's touch, a
path only a monster's. A plain use proves nothing for them either way. The lab now installs the VM's
`enter` hook for the look: a thing's own `use` entered from inside the program (depth 2 on) was SET
OFF, and the thing its caller frame began as FIRED it. That settled the counters and oncounts on
every map with no change to how they are used. Two more traps from the same work: a zero-length
`trace_move` is not a free-room test (sweep the box a unit instead), and a point thing that is not
solid has no `absmin`/`absmax`, so take its middle from `origin + (mins + maxs) / 2`. A player's
health topped above its maximum reads as a hurt every frame (the program brings it down), so keep
the top-up at the normal maximum.
