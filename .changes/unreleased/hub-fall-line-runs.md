---
type: Added
title: A hub keeps a game's runs as input logs beside its demos, and Fall Line has a hub
---

The hub's archive keeps input logs (`ge-inputs`, `engine/play/inputs.hpp`) beside replay files. A log is stored by its SHA-256 under `r/abc/abc….inputs`, found by the Demos page's search and served at `/r/{abc}/{sha256}.inputs`; its summary says `"kind":"run"`. Its Watch page starts the game's web viewer with `--run=`. A log carries no meta words, so the game reads its row through the new `HubSpec::run_row` hook. `HubSpec::score_ms` shows a score as a time (m:ss.cc), and `hub::score_text` formats one. A hub given `--archive` and no master now starts as a hub of demos and runs. `fall_line_hub` is Fall Line's hub: it plays each run out headless and names the seed, the mode, the racers and their times at the line. The Demos page says "1 demo", not "1 demos".
