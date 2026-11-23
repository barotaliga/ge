---
type: Added
title: The reference manual, generated
---

`ge manual` writes a player's reference manual (one searchable HTML page and its JSON) from the manual sources a game's programs write about their own live console: every setting with its layer, default, range and words, every command, every launch flag, and each game's own spellings, defaults and meanings, with filters by game, kind, layer and program. `writemanual <file>` on any console writes its source; Shooter's Edge's window (`se -manual <file>`) describes every game's layer in turn and the dedicated server (`--manual=<file>`) the game it serves. Both programs' flags are now tables their parsers check, so `--help` and the manual list every one. `make manual` builds it; a daily workflow publishes it to the repository's pages.
