---
title: The HUD status's owned bits start at the shotgun; the plugins' weapon bits at the weapon key
date: 2026-11-12
scope: games/shooters_edge/hud/status.hpp, games/shooters_edge/client/weapon_groups.cpp, games/shooters_edge/client/quick_fire.cpp
concepts: [weapon-groups, quick-weapon, owned, status, weapon-key, bits, shooter]
---

`hud::Status::owned` holds bit k - 1 for gun k (the shotgun is bit 0, the melee weapon is always
held and has no bit), while the shooter plugin's `QuickArmed::ready` and `GroupArmed::carried` hold
bit n for weapon key n (the melee weapon is bit 1). A test that writes `owned` by weapon key gives
the player the wrong guns: the launchers are `0x30`, not `0x60`. Convert through `Status::has` and
`key_of` (`quick_armed`, `group_armed`), never by hand. Number-key features run on the client before
the quick weapon (`group_moves` then `quick_moves`), so the quick weapon reads a group's pick as the
player's own weapon key and drops its way back.
