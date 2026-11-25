# Teamplay sets, the message filter, the regex triggers, map and sky groups, network settings (round 11 and 12 reader)

WORKING AREA: deleted when porting ends. Clean-room answers (M18k4 rounds 19 and 20): a reader read the
first game's modern client and Quake II's chosen fork and wrote these facts with no code; the
implementer built from them alone.

Read: ezQuake release tag 3.6.9 (commit b2d448f2c5ac4be4ec40c03dcc9a018c1f837deb, the newest numbered
release tag) and actionquake/q2pro at commit 6400f7e97309554b2b22fe4b2a1ec4803be7ee50 (2026-11-16).
Every fact below is [read] from that source unless marked otherwise. No code, no internal names;
quoted text is what a player sees printed. `<x>` marks a value filled in at print time.

## A. The item sets: tp_took, tp_pickup, tp_point

### 1. Syntax (all three commands share one parser)
- **No argument**: prints the set's member names on one line, separated by single spaces, then a
  newline. Names are printed in the fixed vocabulary order (question 2), each member singly (groups
  are not folded back). If the set is EMPTY, an empty line is printed (the word "none" is prepared
  but never printed: a quirk). When `tp_pointpriorities` is 1 the listing (for all three commands)
  is ordered by priority number instead (lowest first; members never given a number come first).
- **A list of names**, the first one NOT starting with `+` or `-`: the set is emptied first, then
  every argument is applied in turn.
- **First argument starting with `+` or `-`**: the set is kept and the arguments are applied to it.
  `+name` adds, `-name` removes. A sign is sticky: an unsigned argument after `-x` keeps removing
  (the remove/add mode only changes when another `+` or `-` appears). So `tp_took -quad rl` removes both.
- **`none`** alone (case-insensitive): empties the set (and wipes all priority numbers).
- **`default`**: a name standing for the set's default contents (so `tp_took default` replaces the
  set with the default; `tp_took +default` adds the defaults).
- **`all`**: every flag. For `tp_took` and `tp_pickup` the player-and-building names
  (teammate, enemy, eyes, sentry, disp) are always stripped from whatever is added, so `all` there
  means everything except those five.
- **`clear`**: NOT a keyword; it is an unknown name (see 4).
- Names and group words are matched case-insensitively.
- Config saving writes each set as one line: `tp_pickup    ` / `tp_took     ` / `tp_point    `
  (name padded with spaces) followed by `none`, `all`, `default` when the set equals exactly that,
  otherwise the group words that are wholly present (in the order powerups, weapons, armor, ammo,
  players, runes) and then the remaining single names, each followed by a space.
- Config reset runs `tp_took default`, `tp_pickup default`, `tp_point default`.

### 2. Vocabulary
Single names, in their fixed order (32 of them):
quad, pent, ring, suit, ra, ya, ga, mh, health, lg, rl, gl, sng, ng, ssg, pack, cells, rockets,
nails, shells, flag, teammate, enemy, eyes, sentry, disp, quaded, pented, rune1, rune2, rune3, rune4.
(The table also holds resistance, strength, haste and regeneration after those, but the lookup only
searches the first 32, so those four words are NOT accepted: they behave as unknown names.)

Group words:
- `powerups` = quad pent ring suit
- `weapons` = lg rl gl sng ssg (note: NOT ng, and not the shotgun or axe)
- `armor` = ra ya ga
- `ammo` = cells rockets nails shells
- `players` = teammate enemy eyes
- `runes` = rune1 rune2 rune3 rune4
- `default` = the command's default set; `all` = everything.
There are no `health`, `flag`, `items` group words beyond the single names above.

### 3. Defaults
- `tp_pickup` default: powerups, armor, weapons, mh, pack, rockets, cells, flag, runes.
  (Spelled out: quad pent ring suit ra ya ga mh lg rl gl sng ssg pack cells rockets flag rune1 rune2
  rune3 rune4.)
- `tp_took` default: exactly the same set as tp_pickup.
- `tp_point` default: the same plus players (teammate enemy eyes), sentry and disp.
- Not in any default: health, ng, nails, shells, quaded, pented.

### 4. What each set changes
- A pickup is detected from the pickup sounds and the status changes (powerups, health, mega,
  weapons, armour by type, backpack, ammo, runes, the flag by the carrier effect or keys in TF).
- **tp_pickup OR tp_took**: if the picked item is in EITHER set, it becomes the "last took" item:
  its name (the `tp_name_*` word) and the location where it was taken are recorded with the time.
  That record is what the took macros read (`$took`, `$tookloc`, the "name at place" form used by
  `%i`, and the "last took or pointed" form), and what the tp_msgtook message uses. An item in
  neither set leaves the previous record alone.
- **tp_took only**: if the item is also in tp_took, the `f_took` trigger alias runs, but only when
  triggers may fire: not a spectator, the ruleset does not restrict triggers, and either
  `tp_forceTriggers` is 1 or teamplay is on and at least one other non-spectator player shares your
  team. (`tp_triggers 0` also silences it; demo or spectator with `cl_restrictions 1` too.)
- So tp_pickup widens what counts as "took" for the macros without firing f_took.
- **Deathmatch-mode rule for took/pickup**: when the game is not Team Fortress and deathmatch is
  4, ammo and weapons are removed from both sets for that check.
- **tp_point**: only items (by their model) and players in the set can be pointed at. Armour is
  checked per colour (the armour's skin picks ga/ya/ra). Players: a player with the ring (eyes model)
  needs `eyes`; a teammate needs `teammate`; and in all cases `enemy` must be in the set or no player
  at all is pointed at (the checks are a chain, so without `enemy` neither teammates nor eyes are
  chosen: a quirk). Dead players and gibbed heads are never pointed at; nor are spectators or yourself.
- **Deathmatch-mode rule for point** (not TF, deathmatch 1 to 4): deathmatch 4 removes ammo;
  any deathmatch other than 1 (so 2, 3, 4) removes weapons.
- **Unknown names** are silently ignored: no message. But if the first argument is unsigned, the set
  is still emptied first, so `tp_took bogus` leaves an empty set.

### 5. tp_tookpriorities and tp_pointpriorities
- There is NO `tp_tookpriorities` setting in this version (only tp_pointpriorities is registered).
- `tp_pointpriorities`, default 0 (marked as buggy by its authors).
- Priority numbers come from the order of arguments: each named flag gets the argument's position
  (1 for the first argument, 2 for the second...); with a leading `+` the numbers continue after the
  count of flags already in the set. A group word gives all its members the same number. Removing a
  flag sets its number to 0. `none` and an unsigned first argument wipe all numbers. NOTE: there is
  one table of numbers shared by all three commands, so `tp_took` and `tp_pickup` overwrite the
  point priorities too (a quirk).
- With tp_pointpriorities 1:
  - The acceptance cone is narrower: an item is rejected when its sideways miss from the view line
    is more than 0.55 times its distance along the view (about 29 degrees) instead of 1.7 (about 60
    degrees). The 300-unit miss cap and the 10-unit minimum distance stay.
  - Rank is no longer by distance: an item's rank is its priority number, LOWER wins; a player's
    rank is always 1. The first visible candidate with the best rank wins (ties keep the earlier one;
    items are examined before players, in entity order).
  - Bug: the number looked up is off by one, it reads the priority of the NEXT flag in the
    vocabulary order (quad reads pent's number; the armour model reads ya's number). Flags never
    numbered rank 0 and so beat everything.

## B. filter

### 6. Syntax
- No argument: `No filters defined` when empty; otherwise the tags on one line, each with its `#`,
  separated by single spaces, e.g. `#att #def`, then a newline.
- `filter clear` (case-insensitive) or `filter ""` (one empty argument): empties the list.
- `filter #a #b ...`: REPLACES the whole list (it is emptied first). At most 8 tags; arguments past
  the 8th are ignored silently.
- The `#` is required. An argument without it prints `A filter must start with "#"` and stops;
  an argument containing a space prints `A filter may not contain spaces` and stops. Tags accepted
  before the bad one stay (the list was already emptied).
- Each tag is stored without its `#` and cut to 4 characters. `filter #` stores an empty tag.
- Matching is case-insensitive.
- Config saving writes `filter       ` followed by `clear`, or by each `#tag ` (tag then space).
  Config reset runs `filter clear`.

### 7. What it does
- Only TEAM messages are filtered (those recognised as `(name): text` from a teammate, or your own).
  Normal chat, spectator chat and server prints are never filtered. It applies both to team
  messages arriving from the server and to the local echo of your own team message when the client
  prints it itself.
- With an empty list nothing is filtered.
- The tag is the last word of the message: the text must end in a newline; scanning back from just
  before the newline, a `#` must appear within the last 5 characters with no space between it and
  the newline. So the tag is 1 to 4 characters long. Messages whose last character before the
  newline is `#`, whose last word has no `#`, whose "tag" is longer than 4 characters, or where the
  `#` is the very first character of the message text, count as UNTAGGED and are always shown.
- A tagged team message whose tag equals one in the list is shown, with the tag stripped (and the
  single space before it removed too, so the line ends where the text ended).
- A tagged team message whose tag is not in the list is hidden (not printed, no chat sound, and it
  never reaches the message triggers or re_triggers, since those run after the filter).
- Separately, a team message containing `#inlay#` anywhere is always hidden.
- Nothing adds a tag to your own outgoing messages: the filter list is not used when sending. A
  player tags a message by typing it (or via their bound message text).

## C. The regular-expression triggers

### 8. Commands and their printing
- `re_trigger` (no argument): `no regexp_triggers defined` when there are none; otherwise
  `List of re_triggers:` then one line per trigger `<name> : "<regex>" : <times matched>`, then
  `------------` and `<listed>/<total> re_triggers`.
- `re_trigger <pattern>` where the single argument looks like a regex (contains any of
  `\ " ( ) [ ] { } . * + ? ^ $ |` and does not start with `+` or `-`): the same list limited to
  triggers whose names the pattern finds (a case-sensitive unanchored search).
- `re_trigger <name>`: shows one trigger:
  `<name>: "<regex>"`, then `  options: mask=<n> interval=<number>` followed, as they apply, by
  ` final`, ` remove`, ` nolog`, ` disabled`, ` noaction`, then `  matched <n> times`.
  Unknown: `re_trigger "<name>" not found`.
- `re_trigger <name> <regex>`: creates the trigger (or replaces its regex, keeping its options,
  counter and place in the order). On a bad regex it prints `Invalid regexp: ` followed by a garbled
  value (the error is printed wrongly) and DELETES the trigger, even one that existed (and if that
  trigger was not the newest, the newer ones are lost from the list too: a bug).
- More than two arguments: `re_trigger <trigger name> <regexp>`.
- `re_trigger_options <name> <opt>...`: fewer than two arguments prints
  `re_trigger_options <trigger name> <option1> <option2>`; unknown trigger `re_trigger "<name>" not found`.
- `re_trigger_delete`, `re_trigger_enable`, `re_trigger_disable`: any number of arguments, each an
  exact name or a name pattern (as above). Silent when nothing matches; print nothing with no
  argument. A broken pattern prints `Invalid regexp: <message> <offset>`; patterns nested more than
  four deep print `Error: Regexp commands nested too deep`.
- `re_trigger_match <name> <text>`: see 13; wrong count prints `re_trigger_match <trigger name> <string>`.
- Names are exact and case-sensitive. The match variables `re_trigger_match_0` to
  `re_trigger_match_9` are read-only settings.
- These commands are all allowed inside trigger actions (see 11).

### 9. re_trigger_options
Exact lower-case words, applied left to right; an unknown word prints
```
re_trigger_options: invalid option.
valid options:
  final
  notfinal
  remove
  noremove
  mask <trigger_mask>
  interval <min_interval>)
  enable
  disable
  noaction
  action
  nolog
  log
```
(the stray `)` is in the text) and processing continues with the next word.
- `final` / `notfinal`: after this trigger matches, try no later triggers. Default notfinal.
- `remove` / `noremove`: do not show the matched line. Default noremove.
- `mask <n>`: which kinds of line it sees (bits, below). Only the low 8 bits are kept. Default 31.
- `interval <seconds>`: minimum real time between firings; 0 (default) means no limit. While inside
  the interval the trigger is skipped entirely (no match, no counter).
- `enable` / `disable`: default enabled.
- `noaction` / `action`: match (fill the variables, count, remove, nolog) but run no alias. Default action.
- `nolog` / `log`: the matched line is not written to the console log. Default log.
- Mask bits: 1 low server prints, 2 medium, 4 high, 8 chat (normal and team chat both), 16 centre
  prints, 32 the output of the `echo` command, 64 every console print made by the client itself.
  Default 31 = the five server kinds; echo and the client's own prints only when asked.
There is no colour option and no game-only option.

### 10. Which lines are matched
- Server prints (bits 1, 2, 4, 8 by the print's level): the client collects print text until a
  newline arrives and matches the collected text INCLUDING its trailing newline (a chunk may hold
  more than one line). Chat lines include the speaker's prefix (`name: ` or `(name): `). Text is
  matched raw as received: Quake's coloured (high-bit) characters and any colour codes are not
  stripped or converted. Matching happens before the line is shown, and after ignore lists, the
  team filter and flood protection (lines those hide are never matched). Your own chat is seen as
  it comes back from the server.
- Centre prints (16): the whole centre-print text as received, matched before it is shown.
- echo (32): the echoed text after macro expansion and fun-character conversion, without the newline.
- Client prints (64): every message the client prints, as formatted (usually with its newline),
  whatever piece of text one print call carries; the echo command's own output and the
  showpackets/showdrop lines are excluded.

### 11. On a match
- The trigger's counter goes up and its last-fired time is set.
- The captures are copied into `re_trigger_match_0` (the whole match) to `re_trigger_match_<k>`,
  for as many groups as matched, up to 10. Variables beyond that are NOT cleared: they keep earlier
  values.
- Unless noaction: if an alias with exactly the trigger's name exists, it runs at once (its name,
  then a `wait`, are put at the front of the restricted trigger command buffer, which is executed
  right then, before the line is shown). Otherwise `re_trigger "<name>" has no matching alias`.
- Commands the trigger's alias may run are limited; any other prints
  `"<command>" cannot be used in message triggers`. Allowed: play, playvol, stopsound,
  stopsound_script, set, echo, say, say_team, alias, unalias, msg_trigger, inc, bind, unbind,
  record, easyrecord, stop, if, if_exists, wait, log, match_forcestart, dns, addserver, connect,
  join, observe, tcl_proc, tcl_exec, tcl_eval, exec, set_ex, set_alias_str, set_bind_str, unset,
  unset_re, toggle, toggle_re, set_calc, rcon, user, users, unalias_re, the six re_trigger commands,
  the hud262 commands, alias_in, alias_out, cvar_in, cvar_out; and impulse only for Team Fortress's
  listed impulses with a rate limit.
- The line is still shown unless a matching trigger had `remove` (then it is hidden: for a
  centre print, nothing is centre-printed). `nolog` keeps it out of the log.
- Later triggers still see the same line unless the matching one had `final`.
- Order: newest trigger first (each new trigger goes on the front of the list; redefining keeps
  its place).
- Regex flavour: PCRE2 with no options: case-sensitive, unanchored search, `^` at the start of the
  text, `$` at the end or just before a final newline.

### 12. Limits
- `tp_msgtriggers 0` (default 1) turns all re_triggers off (and the older msg_trigger ones).
- While playing (not spectating, not a demo) they are off when the ruleset restricts triggers
  (smackdown, qcon, thunderdome, smackdrive; not mtfl, not the default) or when the server's fpd
  value has the "no timers" bit (2) or the "no sound triggers" bit (4).
- In demo playback or spectating they are off only when `cl_restrictions` is 1 (default 0).
- Built-in guards that always run first on server and centre prints: a high-level line starting
  `Location :` or `Angles   :` (the showloc output) or the centre print
  `Enemies are using your dispenser!` disables re_triggers for that line and keeps it out of the
  log (so they cannot be used as cheats). Another built-in watcher remembers the last IP:port or
  host:port seen in prints (for the last-IP feature); it does not affect user triggers.
- No cap on firings per frame and no recursion guard beyond the `interval` option; the action runs
  synchronously, so an action whose `echo` matches a trigger with mask 32 fires again (the client's
  own print path skips echo output for mask 64 only).
- They are independent of `tp_triggers` and the f_ triggers (`tp_triggers` gates f_ triggers only).

### 13. re_trigger_match <name> <text>
- Looks the trigger up by exact name only (no patterns); unknown prints `re_trigger "<name>" not found`.
- Runs the regex on the given text. It IGNORES the disabled state, the interval, `tp_msgtriggers`,
  rulesets and restrictions. On a match it counts, sets the time, fills the match variables, and,
  unless noaction, runs the alias's body (the alias text itself, then `wait`) in the restricted
  buffer immediately; no alias prints `re_trigger "<name>" has no matching alias`. No match: silent.
  remove/nolog/final mean nothing here; the text is not printed.

## D. mapgroup and skygroup

### 14. mapgroup syntax
- No argument: `No map groups defined` when there are no user groups; otherwise one line per user
  group: a control byte of value 2 (the console's alternate-colour mark) then `<group>: ` and each
  member followed by a space, then newline. Built-in groups are not listed.
- `mapgroup clear`: deletes every user group.
- `mapgroup <group>`: shows that group in the same form (built-in ones too), or
  `No map group named "<group>"`.
- `mapgroup <group> <map> <map>...`: creates the group, or (if it exists and the first map has no
  sign) empties and refills it. `+map` adds, `-map` removes, sticky as in tp_took. A group left empty
  is deleted.
- `mapgroup <group> clear`: deletes it; missing prints `"<group>" is not a map group name`.
- Bad group name (empty, a control or non-ASCII byte, any of `? * : < > "`, or `../` / `..\`):
  `Error: <group> is not a valid map group name`.
- Built-in group: `Cannot modify system group "<group>"`.
- A map may belong to one group only: adding it to another prints (only when `cl_warncmd` or
  `developer` is set) `Warning: "<map>" is already a member of group "<other>"...ignoring`.
  A repeat within the same group is ignored silently.
- Names compared case-insensitively. At most 36 maps per group; names up to 63 characters; no limit
  on the number of groups. Members are bare map names (no `maps/`, no `.bsp`).
- Config saving writes `mapgroup <group> <maps> ` per user group, or `mapgroup clear`. Config reset
  runs `mapgroup clear` and `skygroup clear`.

### 15. What a group shares
The group's NAME is the shared file stem; no member is "the" map. For the map being played, if it is
a member of a group whose name differs from the map's name:
- Place file: `<map>.loc` is tried first; if it fails, `<group>.loc` (user groups only).
- Coloured light file: `maps/lits/<map>.lit`, `maps/<map>.lit`, `lits/<map>.lit` first, then
  `maps/<group>.lit`, `lits/<group>.lit` (user groups only).
- World textures: `textures/<map>/<texture>` first, then `textures/<group>/<texture>` (built-in
  groups too), then the usual shared texture paths.
- Not shared: `$mapname` and every macro, the match tools' names (demos, screenshots), and configs
  (this client reads no per-map config at all).

### 16. Built-in groups
One: `exmy`, holding `start` and `e1m1`..`e1m8`, `e2m1`..`e2m8`, `e3m1`..`e3m8`, `e4m1`..`e4m8`
(33 maps). It is a system group: hidden from the listing, not removed by `clear`, cannot be changed,
not saved, and only its texture fallback applies (no loc or lit fallback).

### 17. skygroup
- Same commands, limits and listing as mapgroup, with the words changed: `No sky groups defined`,
  `No sky group named "<group>"`, `Error: <group> is not a valid sky group name`,
  `Cannot modify system group "<group>"`, the same "already a member" warning. `skygroup <g> clear`
  on a missing group is silent (the "is not a sky group name" message only appears for the literal
  name `exmx`: a quirk).
- No built-in sky groups (start-up clears a non-existent one).
- Effect: whenever a sky is set (map load, a change of `r_skyname`, and after every skygroup
  command), if the current map is a member of a sky group whose name differs from the map's, the
  skybox loaded is the one named after the GROUP, replacing whatever was asked. On map load the
  asked sky is `r_skyname` when set, else the map's own worldspawn sky; the group replaces either,
  and even forces a skybox when neither asks for one. A group name containing a `.` loads nothing.
  A skybox that fails to load leaves no skybox (and a failed `r_skyname` change is refused).

## F. Network settings

### 18. cl_timeout
- ezQuake: default 60, seconds; a value of 0 or below means 60. Counted from the last packet
  received from the server, while connected and not playing a demo. On expiry prints a blank line
  then `Server connection timed out.` and ends the game (disconnects).
- Q2PRO: default 120, seconds (clamped 0 to 2,073,600); 0 disables. Counted from the last packet
  received; checked once per client frame and only fires after more than 5 consecutive frames over
  the limit. Never for a local (loopback) game. On expiry the client disconnects with the error
  `Server connection timed out.` Separately, after a network error report plus 5 seconds of
  silence: `Server connection was reset.`

### 19. showdrop and showpackets
ezQuake (client cvars, default 0; the server half has sv_showpackets / sv_showdrop):
- `showpackets 1` both directions, `2` sent only, `3` received only.
  Sent: `<t> --> s=<outgoing seq>(<1 if reliable data sent else 0>) a=<last incoming seq>(<incoming reliable bit>) <bytes> [c]`
  Received: `<t> <-- s=<seq>(<reliable bit>) a=<acked seq>(<acked reliable bit>) <bytes> [c]`
  `<t>` is the demo packet clock in milliseconds with one decimal. ` [c]` marks the client side
  (` [s]` the server side in a combined build).
- `showdrop 1`: `<address>:Out of order packet <seq> at <last incoming seq>` for stale or duplicate
  packets, and `<address>:Dropped <n> packets at <seq>` when a gap is seen.
Q2PRO (all of these exist only in debug builds; printed in the developer colour, green, regardless
of the `developer` setting):
- `showpackets` (default 0): `send <bytes, width 4> : s=<outgoing seq> ack=<incoming seq> rack=<incoming reliable bit>`
  plus ` reliable=<n>` when reliable data goes, newline; fragments add
  ` fragment_offset=<n> more_fragments=<0/1>`. Receiving: `recv <bytes> : s=<seq> ack=<ack> rack=<reliable ack>`
  plus ` reliable=<n>` (and the fragment fields).
- `showdrop` (default 0): `<address>: out of order packet <seq> at <incoming>`,
  `<address>: dropped <n> packets at <seq>`, `<address>: message too short`,
  `<address>: out of order fragment at <seq>`, `<address>: dropped fragment(s) at <seq>`,
  `<address>: oversize fragment at <seq>`.
- `cl_showpackets` (client, default 0): after each sent move packet prints `<bytes> ` (1: with
  `(<commands>)`, 2: with `(<milliseconds>)`, 3: the changed command fields then ` | `).
- `cl_shownet` (default 0): 1 prints each received message's size `<bytes> `; 2 prints a line of
  dashes then one line per server command `<offset, width 3>:<command name>` ending
  `<offset>:END OF MESSAGE`; 3 adds entity deltas (`<offset>:delta:<n>`, `remove`, `baseline`),
  `playerinfo`, `packetentities` and the strings carried; 4 adds unchanged entities.

### 20. rcon_password and rcon_address
- ezQuake: if connected (from the connection handshake on), rcon always goes to the current server
  and rcon_address is ignored. Not connected: rcon_address is used (port 27500 if none given); if it
  is empty: `You must either be connected or set 'rcon_address' to issue rcon commands`.
  An empty rcon_password prints nothing: the command goes without a password. With `cl_crypt_rcon`
  1 (default) the password is not sent in clear: a SHA-1 digest over the word rcon, the password,
  the client's time in hex and the arguments, plus that time, is sent instead (with an empty
  password the first argument is taken as the password). With cl_crypt_rcon 0 the packet is
  `rcon <password> <args>` with each word followed by a space.
- Q2PRO: no argument: `Usage: rcon <command>`. Empty rcon_password:
  `You must set 'rcon_password' before issuing an rcon command.` Connected: to the current server.
  Not connected and rcon_address empty:
  `You must either be connected, or set the 'rcon_address' cvar to issue rcon commands.`
  An unparsable address: `Bad address: <address>`; default port 27910. Sends
  `rcon "<password>" <rest of the line>` out of band. Both settings are marked private. The server
  browser fills rcon_address with a chosen server's address.

### 21. cl_solid_players, cl_nolerp_on_entity, cl_earlypackets (ezQuake; Q2PRO has none of them)
- `cl_solid_players` default 1: in prediction (your own move and the predicted moves of other
  players) other players present in the frame block as player-sized boxes; 0 lets prediction pass
  through them. Which players are solid comes from the server's solid bit, or, on servers without
  it, every living non-spectator.
- `cl_nolerp_on_entity` default 0: when 1, and `cl_independentPhysics` is on (default 1), while you
  stand on a moving entity (a lift or platform) smoothing is switched off as with `cl_nolerp 1`:
  your view is not interpolated between physics frames and other entities (not monsters, not in
  MVD playback) are drawn at their latest received place.
- `cl_earlypackets` default 1 (menu label "Early Packets"): outside demo playback, the client reads
  server packets as early as possible in every frame (not only on physics frames) and sets up the
  other players' prediction whenever a packet has just arrived; on a teleport it also stops the
  view from being smoothed from the previous position. Seen as lower latency for other players'
  positions.
- Q2PRO's related `cl_nolerp` (default 0): 1 turns off entity and player smoothing; 2 and 3 are
  developer options (always treat entities as new / always smooth from the last received frame).
