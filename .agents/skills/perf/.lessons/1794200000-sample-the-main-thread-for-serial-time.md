---
title: Sample the main thread's wall time to find what runs alone while the pool waits
date: 2026-10-30
scope: plugins/mountain/mapgen
concepts: [profiling, sampling, wall-time, serial, parallel_bands, frame-pointer, callgrind, a-b, early-out, lower-bound]
---

Once the bakes ran on threads, callgrind's instruction counts stopped ranking the map build's
wall time: they weigh a band on a worker the same as a walk on the caller. A timer aimed at the
main thread alone (`timer_create` with `SIGEV_THREAD_ID`, every 0.5 ms; the handler walks the
frame-pointer chain from the interrupted `rbp`, since `backtrace()` crashed unwinding mid-
prologue; a `-g -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer` copy of the build;
`addr2line -f -C -i` on the unique addresses) splits each stage into time inside
`parallel_bands` and time alone: 450 of 750 ms was alone. The wins were the pure parts of that:
candidates gathered in order, walked side by side, weighed in the old order; a per-cell table
both searches read; a running minimum instead of a rescan. A pruned search over options stays
exact when its floor sums the terms read so far in the cost's own order (rounding is monotone,
so the floor never rounds above the full cost) and a tie goes to the lower index; a test against
the long way, with a mutation that overestimates the floor failing it, holds that. A pass that
shares one answer object across calls (a captured `NetHit`) must get its own before it goes on
threads.
