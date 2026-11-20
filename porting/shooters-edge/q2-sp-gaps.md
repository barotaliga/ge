# Quake II single player: the gaps to 1-1, and what has landed

Niclas, 2026-11-20: single player must play 1-1 with the original game. Two clean-room audits list
every difference a player could notice, ranked 1 (at once on the stock campaign), 2 (sometimes)
and 3 (rare or cosmetic):

- `q2-sp-world.md`: entities, movers, triggers, targets, level flow (51: 14 / 20 / 17).
- `q2-sp-monsters.md`: monsters, AI, weapons, items (68: 11 / 36 / 21).

Work goes rank 1 first, in small PRs. Not ours: the base1 bot playthrough (Q2 conveniences
thread) and client cvars such as v_gunkick (client settings thread).

## Landed

| Date | PR | Audit rows |
| --- | --- | --- |
| 2026-11-20 | #1202 weapons on monsters | monsters 2 (direct hit no splash), 3 (BFG lasers and burst hit monsters), 4 (rail through monsters), 5 (first blow on an unaware monster doubles), 12 (splash needs a clear line to a monster), 13 in part (bullets and blasts reach the brain's screen with a direction) |
| 2026-11-20 | corpses and map hooks | monsters 1 (corpses meet shots and blasts and are thrown apart; moves pass them), 7 (deathtarget), 8 (combattarget), 9 (point_combat holds only with its Hold flag, and never a flier), 10 (parasite's drain within 256 and ±30° of pitch), 11 (the flyer bursts at death) |

| 2026-11-20 | world, first round | world 2 (target_explosion's delay holds back its blast and targets), 7 (an item's first touch fires its targets and message, taken or not), 8 ("\n" is a new line), 9 (worldspawn `gravity` in single player), 11 and 31 (a trigger with angle 0 has no way: a push stops, no facing check), 12 (target_speaker's `volume` and `attenuation`, -1 heard everywhere) |

## Next

Monsters rank 1: 6 (infighting). World rank 1: the unit's
end and cinematic, func_clock, trigger_elevator, target_laser's beam, debris, movers carrying monsters and opening for them, target_spawner, animated textures on
movers.
