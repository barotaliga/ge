---
type: Added
title: A demo watched like a video, with its highlights marked
---

`playdemo` shows a play bar you click or drag to seek, with pause, speeds from 0.25x to 8x, skips (`demo_skip_forward` 15 s, `demo_skip_back` 5 s) and the next and last highlight on keys and console commands (`demo_seek`, `demo_skip`, `demo_next`, `demo_prev`, `demo_highlights`). The demo is scanned once for the tracked player's highlights (multi kills from one shot, long rockets, air kills and air gibs, annihilated stacks, telefrags, streaks), marked on the bar in their colours. The engine gains `engine/replay/highlights.hpp` (any game's rules over its own events, scored, merged and ordered) and `engine/ui/play_bar.hpp` (the seek bar as a model and a view); the `se-highlights` lab pictures them.
