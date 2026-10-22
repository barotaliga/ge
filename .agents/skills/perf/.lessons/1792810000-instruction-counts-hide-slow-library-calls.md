---
title: Instruction counts hide a slow library call; sample wall time too
date: 2026-10-22
scope: engine/core
concepts: [callgrind, sampling, sigprof, fmod, remainder, to-int32, hash2, hypot, division, wall-time, a-b]
---

Callgrind ranks by instructions, and `fmod` runs few instructions that take tens of cycles, so
the remainder behind every `angle_diff` and the 32-bit wrap behind every `hash2` never showed in
its tables. A sampling profile of wall time did: an `LD_PRELOAD`ed SIGPROF handler that records
the interrupted PC (and whether it is the main thread), dumped with `/proc/self/maps` at exit and
symbolised with `addr2line -a -f -i`, grouped by the innermost and the outermost inlined frame.
The exact shortcuts that followed: a dividend already inside the divisor is fmod's answer as it
is; a value within int64 truncates exactly as an int64 and its low 32 bits are the wrap; the
larger side of a `hypot` over itself is exactly 1, so only the smaller is divided; and a
power-of-two pitch's inverse is exact, so a product by it is the quotient's bits. Each needs a
test holding it to the long way over many values. Time changes as A/B runs of the two binaries
back to back, several times over: in the container one run wanders by ±5%, more than most
single changes buy. Two ideas that measured as no gain and were dropped: answering "does any run
cover this point?" from one exhaustive walk (the first question's early stop is worth more), and
reading the woods' clumps ahead under guessed ids (a clump that grows nothing shifts every id
after it, and 18% of them do).
