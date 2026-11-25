# Held weapons: the weapon in the first person

The weapon in the player's own hand: which set is drawn, where it sits in the view, how it is lit
by the room, how it switches, kicks and fires, and how a recording or a joined server drives it.
The weapons OTHER players carry are [`models.md`](models.md); the weapons' rules (damage, refire)
are each game's [`rules.md`](rules.md).

## Shared

| Layer | File | What it gives |
| --- | --- | --- |
| Engine | `engine/viewmodel/held.hpp` | The held model: its frames, skin, colours and grip mount |
| Engine | `engine/viewmodel/place.hpp` | Its place in the view: offset, bob, sway, kick, dip, put-away, breath |
| Engine | `engine/viewmodel/light.hpp` | Lit by the world: the probe, point lights, the floor under it |
| Engine | `engine/viewmodel/arm.hpp`, `raster.hpp`, `stage.hpp` | The arm's joints and their human ranges; the CPU reference raster; the labs' stage |
| Engine | `engine/render/viewmodel.hpp` | The GPU pass |
| Classic enhance | `plugins/classicenhance/weapons.hpp` | The pack's held weapons as view models: the form, the skin, the floor's light (`ce_weapons`) |
| Shell | `games/shooters_edge/view/hand.hpp` | The hand: the switch, the shot's kick and flash, the cvars; a classic row placed at the eye where its files author it (`Hand::at_eye`) |
| Shell | `games/shooters_edge/client/held.hpp` | Which set (`load_held_set`: the loaded game's own, else Shooter's Edge's), which weapon, a shot, the look, the stride; driven from a recording or a joined server (`step_demo_held`) |
| Shell | `games/shooters_edge/view/weapons.hpp`, `content/models/view/` | Shooter's Edge's OWN set, built in Blender (`held-weapons` skill), for its own content only |

## Each game's

| | Quake | Quake II | Next: the third | Next: the sector family |
| --- | --- | --- | --- | --- |
| Files | one model a weapon, authored at the eye, the firing frames a `fire` clip | a folder a weapon with its own model | its own weapon models (the hand and the weapon apart, joined by a tag) | weapon sprites drawn on the screen, not in the world |
| Adapter | `held_weapons` in `client/qw_adapter.cpp` | `held_weapons` in `client/q2_adapter.cpp` | its own | its own: a 2D path (`docs/porting-sprite-games.md`) |
| Firing | the mod's `attack_finished` moved past the server's time | the rules' hand stepping onto a firing frame | its own frames | its own frames |

## Adding a game

1. `held_weapons` in the game's adapter returns its own set. A missing file draws no weapon and the
   status line names it: never a stand-in from another game or from Shooter's Edge's set.
2. Place each one where its files author it (`Hand::at_eye`, a classic row with no offset).
3. Say when it fires from what the server or recording sends, as `held.hpp` does for both games.
4. Run classic enhance's weapons stage (`ce_weapons`) on the set and check each weapon.

## Brought and left behind

| Benefit, now every game's | From | Layer |
| --- | --- | --- |
| The weapon lit by the room it is in | ours | `engine/viewmodel/light.hpp` |
| Bob, sway, kick, landing dip and put-away as one model | the engine's camera and view model | `place.hpp`, `cl_landdip` (off in Quake) |
| Quick weapon and weapon groups over any game's weapons | Quake's later clients | `features.md` |

| Drawback left behind | How |
| --- | --- |
| A weapon unlit or flat-lit in a dark room | the room's light on it |
| One game's weapon standing in for another's missing one | never: no weapon and a named missing file |

## Gaps

The third game's tagged hand and weapon; the sector family's screen sprites.

## Labs and tests

`se-held`, `se-view`, `gpu-viewmodel`, `se-viewmodel`, `classic-weapons`, `se-weapon-stage`;
`tests/shooters_edge_listen_test.cpp`.

## Lessons

- A classic game is played with its own things alone: Shooter's Edge's weapons are for its own
  content and never a stand-in.
- Test the adapter with only that game's packs: Quake II once asked for Quake's palette
  and showed no weapon at all.
