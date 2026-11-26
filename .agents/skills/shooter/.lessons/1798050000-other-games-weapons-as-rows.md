---
title: Another game's weapons on Quake II's rules are rows of its own table, gated by the body, never renamed files
date: 2026-11-26
scope: games/shooters_edge/server
concepts: [remix, cr-items, sv-wear, q1-arms, weapon-row, quake-row, held-loop, look-names]
---

- **A worn game's weapons are `SqWeaponRow`s with `quake` set**, built from that game's one data
  table (`sq_quake_row` over `profiles::kQ1Weapons`), and the shot numbers both rules read live
  in a plain header in `plugins/classic` (`q1_arms.hpp`). `sq_holds` needs `row.quake ==
  body.quake`, so Quake II's own play never sees them and nothing else needs a guard; the
  weapon table's lookups (`sq_weapon_named`, `sq_weapon_of_item`, the impulses, the cycle) take
  a `quake` flag from `Q2Server::quake_arms`. Every caller that maps an item or a name to a
  weapon must pass it (`commands_refuse_item`, the drops), or it finds Quake II's row.
- **A held weapon with no wind-up is `Q2Held::loop`** in the shared hand: fire frames 1..2,
  one shot a frame while the button is held and ammunition lasts.
- **The hits travel as the worn game's look names (`q1_*`)**; the listen client strips the prefix
  and draws the worn game's own effect. The Quake II wire drops a name it does not know, so a
  remote client needs our named-effect extension before it sees them.
- **A row added to the weapon table shifts nothing for the old rows** but every table indexed by
  `kSqWeaponCount` (the bar's `kWorn`, `kSqAimNames`, the server's model arrays) must grow with
  it, and the joined bar must skip the worn rows on Quake II's keys.
