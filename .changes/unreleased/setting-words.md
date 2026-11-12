---
type: Added
title: Every server setting explains itself
---

Every server setting now says what it does in one sentence and what each of its values does: `help <setting>` shows both, and a saved cfg writes them as comments. A cvar carries the explanations in `CvarSpec::values` (`engine/console/words.hpp` reads them, and `value_fits` says whether a value is one of them); Shooter's Edge's server settings, its rules and its mod's settings all have them.
