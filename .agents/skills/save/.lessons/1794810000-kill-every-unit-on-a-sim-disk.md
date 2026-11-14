---
title: Prove a crash-safe write by killing a simulated disk at every unit, and read the index by chunk
date: 2026-11-13
scope: engine/save/disk.hpp, engine/save/store.cpp, engine/lab/labs/save_slots_lab.cpp
concepts: [store, slot, index, sim-disk, kill, crash, bak, tmp, recovery, chunk-reader]
---

A claim that "a crash at any byte leaves the old save or the new one" is held only by trying
every byte: `SimDisk` spends one unit a byte written and one a rename, remove or flush, and dies
when its `budget` runs out, leaving a partial file. Measure the write's cost once on a
`restarted()` copy, then loop `k = 0..cost`: kill at `k`, reopen on `restarted()` (what the
next launch sees), and check the slot and its neighbour. The recovery that makes it pass lives in
`Store::open`, not in `write`: remove every `.tmp`; a `.bak` with no slot means the kill came
between the two renames (put it back); a `.bak` beside a slot that opens means the kill came
before the `.bak` was removed (drop it). Reading a thousand-slot index with `FileReader::get` by
tag and a linear `find` per slot is quadratic: walk `r.chunks()` with `r.reader(c)` into a map.
