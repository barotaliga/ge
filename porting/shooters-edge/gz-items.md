# Ground Zero (game folder "rogue"): item spawnflags, answers (batch E)

Source read: the Ground Zero game library, repository github.com/yquake2/rogue, commit
9e7b3160ed27befd5c47872f8e151e5743d0a6ba (src/rogue; src/rogue-m and src/rogue-w are checkouts
of the same commit and were not read separately). Base game for comparison: github.com/yquake2/yquake2,
commit 7e75145ce43e2dcc7b76d1a7468908b4cb3d76ed. Files read: the items file, the spawn file, the
local header (flag values), the cvar setup in the save code.

Tags: [read] stated plainly by the source; [derived] worked out from several read facts.

## 1. Which item spawnflags are accepted

- The pack names two item flags in its header: value 1 "trigger spawn" and value 2 "no touch".
  Values 4 to 128 are described as reserved for editor flags; 256 and up (8 bits) are used as power
  cube id bits in coop. Values 65536 (dropped item), 131072 (dropped player item) and 262144
  (targets already used) are internal markers the game sets itself, never from a map. [read] The
  base game has the same header layout. [read]
- The item spawn routine's check differs from the base game: the pack warns and clears only when
  the flags are GREATER THAN 1; the base warns when they are anything other than 0. [read]
  So in the pack, value 1 alone is accepted silently; any other non-zero value (2, 3, 4, ... once the
  mode and skill bits are gone) prints the same message as the base game, word for word
  "<classname> at <position> has invalid spawnflags set", and sets the flags to 0, for every item
  except key_power_cube. [read] The position is printed in the engine's usual vector form
  (parenthesised, space-separated whole numbers). [read, the shared vector-to-text helper]
- Consequence: for ordinary items only "trigger spawn" (1) survives. "No touch" (2) on an ordinary
  item, alone or with 1 (value 3), triggers the warning and is cleared, so the item spawns as a plain
  item at once, visible and touchable. [derived: the check runs before any flag is read]
- key_power_cube keeps whatever flags it has (no warning for any value), as in the base game. [read]
- The check runs after the pack's g_disruptor test (when the cvar g_disruptor is 0, its default,
  ammo_disruptor and weapon_disintegrator are removed silently before any flag check). [read]

## 2. An item with "trigger spawn"

At load (ordinary items, not the power cube) [read unless marked]:
- The item spawn routine does its usual setup (precache, the deathmatch and mode removals, the
  model, the glow and rotate effects) and then, because flag 1 is set, it removes the item's
  pending think (so the normal drop to the floor two frames after spawn never runs), marks it as
  not sent to clients (invisible), makes it not solid, and gives it a use action.
- It is never linked into the world at load: no link happens on this path. [derived: neither the
  spawn routine nor the trigger-spawn setup links the entity, and the drop routine that would link
  it is cancelled] It stays at its placed origin with no size set.

When used (targeted) [read]:
- It becomes visible, its use action is removed (so a second use does nothing: it can be used only
  once), and its flags are set to 0 (except for key_power_cube, which keeps them).
- Then the drop-to-floor routine runs immediately, in the same frame (no two-frame delay): the box
  is set to 15 units each way from the origin (30 x 30 x 30 units), the model is set, it becomes a
  pickup trigger, its move type becomes "toss" (falls under gravity), and its touch is the normal
  pickup. A box trace is made from the origin straight down 128 units against solid world; the
  item is moved to where the trace stops. If the box starts inside a solid, the message
  "droptofloor: <classname> startsolid at <position>" is printed and the item is deleted.
- So it drops to the floor (snapped down up to 128 units, then falls further under gravity if
  there is no floor within 128 units, because of the toss move type). [derived from the trace and
  the move type]
- If the item belongs to a team, the team handling of the drop routine applies as for any item
  (hidden, and the team master schedules a respawn choice one frame later). [read]
- Because its flags are now 0, the drop routine's own "trigger spawn" and "no touch" branches do
  not fire. [derived]

Respawn in deathmatch: after the use its flags are 0 (not marked as dropped), so the normal pickup
routines schedule a respawn in deathmatch exactly like a placed item. [derived: pickups respawn
when the item is not marked dropped and deathmatch is on]

The base game's handling is different: there the drop happens at load and the item is then hidden
and made non-solid with a use that only shows it (no further drop); in the pack that path is used
only by key_power_cube (see 4). [read]

Targets: an item fires its targets once, on the first pickup touch, then marks them used; same as
the base game. [read]

## 3. A flag that makes an item not touchable / not dropped

- Only "no touch" (2) exists, and it reaches only key_power_cube (on any other item it is cleared
  with the warning, see 1). [read/derived]
- On a power cube with "no touch": at the end of the drop routine it is made a solid box (blocks
  movement, as a bounding box), has no touch action (cannot be picked up), and loses the rotate
  effect and the glow. It is still dropped to the floor like any item (the drop routine runs first).
  [read]
- If a "no touch" power cube is also "trigger spawn", the use makes it appear as that solid box with
  no touch. [read]
- There is no flag that prevents the drop to the floor. [read: no other item flag is tested]

## 4. Keys and the power cube

- All key_* entries (key_data_cd, key_power_cube, key_pyramid, key_data_spinner, key_pass,
  key_blue_key, key_red_key, key_commander_head, key_airstrike_target, key_nuke_container, key_nuke)
  are ordinary items spawned by the same item routine, so key_nuke and key_nuke_container follow the
  rules of 1 and 2 exactly. [read] They are all marked "stay in coop" (in coop, picking one up does
  not remove it, and players cannot drop them). [read]
- key_power_cube is its own case [read]:
  - no warning and no clearing of its flags;
  - the pack's trigger-spawn setup skips it, so it drops to the floor two frames after spawn as
    usual; at the end of that drop, flag 1 hides it, makes it not solid and gives it the base-style
    use action; when used it becomes visible and becomes a pickup trigger (or the solid untouchable
    box if flag 2 is set), linked where it already lay; the use is removed, so once only;
  - in coop, a power cube without "no touch" gets an id bit (256 shifted left by the count of cubes
    so far, i.e. 256, 512, 1024, ...) added to its flags, and the level's cube count goes up by one;
    same as the base game.

## 5. Dependence on deathmatch, coop or skill

- The flag check and the trigger-spawn behaviour themselves do not depend on deathmatch, coop or
  skill. [read]
- What does depend on mode [read]:
  - deathmatch: items removed by dmflags (no armour, no items, no health, infinite ammo, plus the
    pack's no mines, no nukes, no spheres) are freed before the trigger-spawn setup; after a use
    the item respawns like a placed one;
  - not deathmatch: the deathmatch-only items (the doppelganger, the nuke pickup, the vengeance and
    hunter spheres) are freed;
  - coop: "stay in coop" items (keys) are not removed on pickup and cannot be dropped; power cubes
    get their id bits.
- Skill matters only through the spawn-time removal flags (question 6).

## 6. The skill and mode spawnflags

- Yes. The level spawn loop tests 256, 512, 1024, 2048 and 4096, removes the entity if it is
  excluded, and otherwise clears all five bits from the flags BEFORE the entity's spawn routine runs.
  [read] So 2049 reaches the item as 1: "not deathmatch" plus "trigger spawn", accepted with no
  warning. [derived]
- The pack's exclusion rules [read]:
  - deathmatch: 2048 removes it (as base);
  - coop, when the latched cvar coop_baseq2 is 0 (its default): 4096 removes it; and the skill bits
    remove it for the current skill (256 easy, 512 medium, 1024 hard and hard-plus), except that an
    entity with all three skill bits set (256 + 512 + 1024) is kept at every skill (it is a
    coop-only entity);
  - single player, or coop with coop_baseq2 set to non-zero: the skill bits remove it for the current
    skill as in the base game; 4096 is not tested there.
  The base game tests only 2048 in deathmatch and the skill bits otherwise (coop included); it never
  tests 4096 and has no coop-only rule. Both clear all five bits afterwards. [read]
- A few map-specific fixes in the pack's spawn loop change these bits for named entities before the
  test, none of them items (a trigger in "command", a door, a trigger and a wall in "rhangar2").
  [read]
