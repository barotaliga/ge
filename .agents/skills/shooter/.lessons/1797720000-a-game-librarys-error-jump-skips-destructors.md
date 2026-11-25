---
title: A hosted game library's error leaves by a long jump, so nothing with a destructor may be alive across it
date: 2026-11-15
scope: games/shooters_edge/server/q2_native_calls.cpp
concepts: [native-game-library, longjmp, setjmp, leak, asan, error-jump]
---

The library's `error` call never returns: the host jumps back to the guard around the call into
the library. A jump skips every destructor between, so a `std::string` built for the message (a
local, or a temporary in the call's full expression such as `fail(h, "x" + name)`) leaks, and
LeakSanitizer fails the test that makes the game stop. Set `error_text` in its own statement,
let the scope close, then call `leave(h)`, which jumps with nothing alive in its frame; keep the
formatted text out of locals of the frame that jumps.
