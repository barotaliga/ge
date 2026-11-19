# Weapon choice and the switch rules (clean-room note)

WORKING AREA: deleted when porting ends. A reader's answers on the first game's modern client's weapon command, preselection, hiding and score keys, and the common deathmatch mod's w_switch and b_switch. Built from in `games/shooters_edge/client/weapon_choice.hpp` (round 2 of `client-gaps.md`).


Read: ezQuake at commit 5eb5920 (GPL-2.0) and KTX at its default branch, commit b1b23a9 (GPL-2.0).
Tags: [read] = stated by the source as read; [derived] = follows from facts read, not run,
[measured] = none in this document (nothing was run). Weapon numbers below are the player-facing
numbers 1 axe, 2 shotgun, 3 super shotgun, 4 nailgun, 5 super nailgun, 6 grenade launcher,
7 rocket launcher, 8 thunderbolt (see answer 6).

## 1. What counts as owned, ammo needs, the axe, nothing owned

- A weapon in the list counts as available only when the player holds the weapon (its bit in the
  items stat) AND has at least this much of its ammo [read]:
  - 1 axe: the axe item bit only, no ammo [read]. The axe is therefore not unconditionally
   available: if the server's items stat lacks the axe bit, 1 is skipped like any other unowned
   weapon [read]. Whether a server ever clears the axe bit is outside the client and not settled
   here [open].
  - 2 shotgun: 1 shell or more [read].
  - 3 super shotgun: 2 shells or more [read].
  - 4 nailgun: 1 nail or more [read].
  - 5 super nailgun: 2 nails or more [read].
  - 6 grenade launcher: 1 rocket or more [read].
  - 7 rocket launcher: 1 rocket or more [read].
  - 8 thunderbolt: 1 cell or more [read].
- The list is read in the order given; the first available entry wins [read]. At most 10 numbers
  are remembered; further arguments are ignored [read]. Entries below 1 or above 8 are never
  available (skipped) [read].
- When nothing in the list is available, the result is the FIRST number of the list as typed,
  unchanged [read]. So `weapon 7 8` with neither weapon usable still sends impulse 7 (in mode 0)
  [derived], and the server's own refusal applies (the classic "no weapon" / "not enough ammo"
  reply is the server's business, not the client's) [derived]. Nothing is sent only when the
  first number is 0 or not a number (it reads as 0) [derived].
- `impulse` with one number sends that number raw, with no ownership check and no preselection
  [read]. With two or more numbers it remembers the list and sends the best available at once,
  or the first number when none is available, regardless of cl_weaponpreselect [read].
- Related player-visible commands [read]: `+fire <n> [n ...]` (and `+fire_ar`, an anti-rollover
  variant meant for binding to several keys) set the list, pick the best available at once and
  press attack in one go; the trailing argument is taken as a key code when it is 32 or more.
  Releasing a `+fire_ar` key while another is still held reselects from the list remembered for
  the key still held.

## 2. Where the stats come from; dead and spectating

- The check reads the client's copy of the server's stats (items, shells, nails, rockets, cells)
  at the moment the command runs, or the moment attack is pressed [read]. Those stats are the
  values the server last sent (the server sends a stat when it changes); the command does not
  poll anything each frame [read for the reading; derived for the update model].
- The display paths (status-bar ammo and gun elements, the view model with r_viewpreselgun, the
  weapon-number chat macro) recompute the preselected weapon every time they draw [read], without
  changing the remembered list [read].
- The `weapon`, `impulse` and `+fire` commands make no check for death, spectator mode or demo
  playback: they compute the choice from whatever stats the client holds and queue the impulse
  for the next movement command as usual [read]. What the server then does with an impulse from
  a dead player or a spectator is the server's matter [derived]. Whose stats a tracking
  spectator holds is not settled by the parts read [open].
- The preselected weapon is shown (HUD, view model) only while health is above 0, not in demo
  playback and not as a spectator [read].

## 3. Preselection modes

- With any nonzero cl_weaponpreselect (1, 2, 3 or 4, including 3 and 4 outside deathmatch 1),
  pressing attack first computes the best available weapon from the remembered list and, if
  that is nonzero, queues it as the pending impulse, then marks attack as pressed [read]. Both
  go into the same outgoing movement command: the next command built carries the attack button
  and the weapon impulse together [read]. Nothing is sent a frame earlier [derived].
- Server side (KTX), within one player frame the impulse is handled before the attack button is
  examined, so the switch is applied before the fire check in that same frame [read]. Whether the
  change itself delays the first shot is not settled by the parts read [open].
- Mode 0: a `weapon` command sends the best available at once [read].
- Mode 1: a `weapon` command only updates the remembered list; nothing is sent until the next
  attack press [read].
- Mode 2: a `weapon` command sends at once if attack is currently held or was pressed within the
  current input frame, and only if the result is nonzero; otherwise it only preselects (sent on
  the next attack press) [read].
- Modes 3 and 4: the client parses the server info key `deathmatch` as an integer whenever the
  server info arrives [read]. Mode 3 behaves as mode 1 when that value is exactly 1, otherwise as
  mode 0; mode 4 behaves as mode 2 when it is exactly 1, otherwise as mode 0 [read]. (The attack
  press preselection above still applies in modes 3 and 4 for every deathmatch value [read].)
- Only one impulse is pending per outgoing command; a later one in the same frame replaces an
  earlier one [read]. The pending impulse is cleared once copied into a command [read].
- `weapon 10` and `weapon 12` are special [read]: 10 selects the next available weapon after the
  current preselected one, counting upward and wrapping from 8 to 1; 12 does the same downward,
  wrapping from 1 to 8; if none is found the result is 1. The result replaces only the first
  entry of the remembered list. Sending is then governed by the mode as above.

## 4. cl_weaponforgetorder and the remembered list

- The client never switches by itself when a weapon runs dry or a new weapon is picked up: no
  path reacts to stat changes by sending an impulse [read]. Choices are made only when a
  weapon/impulse/+fire command runs, when attack is pressed with preselection on, when a
  `+fire_ar` key is released over another, and when attack is released with weapon hiding on
  (answer 5) [read].
- With preselection on, though, each attack press re-picks from the remembered list, so after
  the first choice runs out of ammo the next attack press selects the next available entry
  [derived]. Server-side out-of-ammo switching is the mod's own (KTX switches to its own best
  weapon, see answer 8) [read].
- With cl_weaponforgetorder 0 (default "0") the whole list (up to 10 entries) stays as typed
  until the next weapon/impulse/+fire command replaces it [read].
- With cl_weaponforgetorder 1, every time a choice is made (not for display) the list is
  rewritten to: the chosen weapon, then shotgun, then axe; or the chosen weapon then axe when the
  chosen weapon is the shotgun or cl_weaponhide_axe is 1 [read].
- The list is reset to "shotgun, then axe" whenever the client's game state is wiped, which
  happens on each connection and each map change (on every new server data message) [read]. It
  is not reset on death [read].
- Release has a server-side weapon-selection protocol extension (setting
  `cl_pext_serversideweapon`, default 0) and a flag set that includes "reset on death"; in the
  client read here no code sends those flags [read], so it is out of play [derived].

## 5. cl_weaponhide

- Applies when cl_weaponhide is 1, or when it is 2 and the server info `deathmatch` is exactly 1
  [read]. Default "0"; cl_weaponhide_axe default "0" [read].
- On releasing attack (only on the release that leaves attack fully up, i.e. the last of several
  keys holding it) the pending impulse is set to [read]:
  - 1 (axe) when cl_weaponhide_axe is 1,
  - otherwise 2 when the player has the shotgun item and at least 1 shell, else 1.
- It is sent regardless of the weapon that was fired or the weapon held: no check that the
  current weapon was already the shotgun or axe [read]. The axe impulse is sent without checking
  the axe item [read].
- No delay: it goes into the next movement command built [read]. It applies to releasing
  `+attack`, `+fire` and the last `+fire_ar` key [read].

## 6. Weapon numbering

- Yes: the client maps 1 axe, 2 shotgun, 3 super shotgun, 4 nailgun, 5 super nailgun, 6 grenade
  launcher, 7 rocket launcher, 8 thunderbolt, the classic impulse numbering [read].
- Numbers above 8 (and below 1) in a list are never chosen as available [read]. They can still be
  sent when they are the first number and nothing else is available, or when given to `impulse`
  alone [derived]. `weapon 10` and `weapon 12` as the first argument have the cycling meaning in
  answer 3 [read].

## 7. r_viewpreselgun

- Default "0" [read]. When 1, and the preselection display applies (cl_weaponpreselect nonzero,
  alive, not demo playback, not spectator), and the gun is not in a firing animation (weapon
  frame 0), the first-person weapon model drawn is the preselected weapon instead of the
  server's current weapon [read]. It changes only which view model is drawn; it sends nothing
  and changes no list [read]. (The ammo and gun HUD elements follow the preselection whenever
  preselection is on, independently of r_viewpreselgun [read].)

## 8. KTX: w_switch and b_switch

Values are read from the player's user info at the moment of each pickup, as an integer by the
usual leading-digits rule (non-numeric text reads as 0) [read].

- Threshold: a picked-up weapon is eligible when its weapon number is less than or equal to the
  value (inclusive) [read]. w_switch governs map weapons and ammo boxes; b_switch governs
  backpacks [read].
- Empty, absent, 0 or non-numeric (so "none" too): if the user info key `w_rank` reads as a
  nonzero integer, the threshold becomes the mod's best-weapon result under w_rank expressed as
  the weapon's item bit value, not its number (1 shotgun, 2 super shotgun, 4 nailgun, 8 super
  nailgun, 16 grenade launcher, 32 rocket launcher, 64 thunderbolt, 4096 axe) [read; the bit
  values are the format's standard item bits]. That is a quirk: with a best weapon of thunderbolt
  or better bit the threshold admits everything, with shotgun it admits nothing [derived].
  Without w_rank, the threshold is 8 [read].
- 8 or more: every weapon is eligible [read]. 1 and 2, and negative values: in effect never,
  because no pickup has number 1 and the shotgun never outranks anything (below) [derived].
- Eligibility is not enough. The switch happens only when ALL hold [read]:
  - the player is not a bot,
  - weapon number at or below the threshold,
  - if the player is in liquid, the new weapon is not the thunderbolt,
  - the player is not holding the grappling hook with attack down,
  - the new weapon ranks strictly better than the current one, by the fixed rank thunderbolt,
   then rocket launcher, then super nailgun, then grenade launcher, then super shotgun, then
   nailgun, then everything else (shotgun, axe, others) tied last.
  Note the grenade launcher ranks below the super nailgun [read].
- Map weapon pickup: switch considered every time the weapon is actually picked up [read]. In
  deathmatch 2, 3 and 5, in coop, or with the server setting k_nosweep, a weapon the player
  already holds cannot be picked up, so nothing happens [read]. Pickups only work while a match
  is running and the mod considers play ready [read].
- Backpack: the new weapon is the backpack's weapon item; a backpack with no weapon never causes
  a switch [derived]. Uses b_switch with the same 0/w_rank/8 fallback [read].
- Ammo box: if, before the ammo was added, the current weapon was the mod's best weapon, the mod
  considers switching to its best weapon after the ammo, under w_switch [read]. Without w_rank
  that best-weapon rule is thunderbolt (only when the player is out of water or no deeper than the feet in it, and with at
  least 1 cell), super nailgun (2 nails), super shotgun (2 shells), nailgun (1 nail), shotgun
  (1 shell), else axe; it never names the rocket or grenade launcher [read]. With w_rank set, its
  digits are tried in order with the same ammo minimums as the client (answer 1), falling back
  to the axe [read].
- Other values: no named words are recognised; anything non-numeric is 0 [read].
- The stock deathmatch progs was not read; whether it reads the same keys is open [open].
- KTX's `info <key> <value>` command (as `cmd info`) sets any user info key server side for the
  player, so w_switch and b_switch can be set through it too [derived].

## 9. How ezQuake sends them

- ezQuake registers `w_switch` and `b_switch` as user-info settings, default empty [read].
  Setting the setting (config, console, menu) mirrors its value into the user info and, when
  connected, sends a `setinfo` to the server at once [read]. An empty value removes the key from
  the user info, so by default the server sees no key at all [read].
- `setinfo w_switch <v>` also works: it edits the user info directly and forwards to the server,
  but does not change the setting's own value [read].
- The options menu has "Gun Autoswitch" (under Weapon Handling): it shows "on" when either value
  is 0/empty or above 2; toggling from on sets both to 1, from off sets both to 8 [read].

## 10. +showscores and +showteamscores

- Both are local view toggles; pressing sets a flag and forces a redraw, releasing clears it
  [read].
- +showteamscores in a team game draws a team summary first (per team: ping low, average and
  high, the team name, total frags, player count) and then the same player scoreboard below it
  [read]. Without teamplay it draws the plain scoreboard [read]. The player rows are grouped by
  team in both, when teamplay is on, more than 2 players are present and the setting
  `scr_scoreboard_teamsort` is 1 (its default) [read]; teams are ordered by team frags, players
  within a team by frags then name, spectators last [read].
- While dead (not spectating) in a team game the team view is shown unless +showscores is held
  [read].
- Holding either sends nothing of its own, but while the player scoreboard is drawn, connected
  and not playing a demo, the client sends the string command `pings` to the server at most once
  every 2 seconds to refresh ping times [read].
