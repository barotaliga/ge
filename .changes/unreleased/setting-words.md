---
type: Added
title: Every setting explains itself, and configs are checked and formatted
---

Every server and client setting now says what it does in one sentence and what each of its values does: `help <setting>` shows both, and a saved cfg writes them as comments. A cvar carries the explanations in `CvarSpec::values` (`engine/console/words.hpp` reads them, and `value_fits` says whether a value is one of them); Shooter's Edge's server settings, its rules, its mod's settings and every player setting (the HUD, the picture, the classic enhancements, the controls, the weather, frame pacing) all have them.

A player's CLI checks and tidies a config without running it: `settings check` and `server check` list each line that sets a setting the game lacks, a value the setting does not take or a setting set twice; `settings format` and `server format` write it back tidy, `--comments` putting each setting's explanation above it and `--pretty` grouping the settings into sections in a fixed order with their values aligned (`--write` in place). The cvar catalog (`cvars.txt`) gains each setting's values as an eighth field; a seven-field catalog still reads.
