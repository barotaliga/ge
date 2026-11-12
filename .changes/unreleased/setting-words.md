---
type: Added
title: Every setting explains itself
---

Every server and client setting now says what it does in one sentence and what each of its values does: `help <setting>` shows both, and a saved cfg writes them as comments. A cvar carries the explanations in `CvarSpec::values` (`engine/console/words.hpp` reads them, and `value_fits` says whether a value is one of them); Shooter's Edge's server settings, its rules, its mod's settings and every player setting (the HUD, the picture, the classic enhancements, the controls, the weather, frame pacing) all have them.
