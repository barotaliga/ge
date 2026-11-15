---
type: Added
title: A map's intent drafted from its items, and map-intent plays built maps
---

`ge::shooter::draft_intent` (`plugins/shooter/intent_draft.hpp`) drafts an `.intent` for a map nobody wrote a plan for. Each room round an armour, a power or a key weapon becomes one area: the red armour's room a camp, the yellow's a hold, a power's or the mega health's a power, the launcher's or the lightning gun's a fight. The room is found by flooding the map's open cells on a cut at the item's height. `--intent=auto` in the `map-overview` and `map-intent` labs uses it and attaches the draft as `<map>.intent` to edit. `map-intent` takes `--game=`, `--map=` and `--intent=` to play bots on a built map from a game folder instead of compiling a plan.
