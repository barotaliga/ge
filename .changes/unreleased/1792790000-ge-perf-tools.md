---
type: Added
title: ge bench, profile, mem and leaks
---

A game is measured through `ge`: `ge bench` times each step of the pilot's runs against its budget (with `--tag` and `--compare` for a before and after), `ge profile` says where the step's time goes zone by zone (`--render` adds the frame's GPU and CPU ms per pass from the game's `profile` lab), `ge mem` counts what the run allocates and names the zone of a hot path that still does, and `ge leaks` reports every block never freed at its line; the new `ge::prof` target gives games the `GE_ZONE` timing zones and `ge::prof_alloc` the allocation counts, and `FrameStats` keeps each pass's CPU ms beside its GPU ms.
