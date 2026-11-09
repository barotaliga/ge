# Answers: the 2021 re-release's localized message keys

Sources read: id Software's quake-rerelease-qc (GPLv2), default branch as of 2026-08-31; Ironwail
(GPLv2), default branch as of 2026-09-10; vkQuake (GPLv2), default branch as of 2026-11-07. No
re-release data files (no progs.dat, no .kpf, no .bsp) were available, so nothing here was
measured against shipped data. Markers: [read] = read in the sources; [derived] = follows from
what was read; [open] = the sources do not settle it.

Where the two engines are named, "both engines" means Ironwail and vkQuake behave the same.

## 1. Which strings are keys, their shape, and the paths that carry them

- A key is a whole QC string literal that starts with `$` followed by lower-case ASCII letters,
  digits and underscores only; no spaces, no upper case, no other punctuation. Counted over every
  `$`-string in all six game codebases: 409 distinct keys, every one matching that shape; 135 of
  them are in the base campaign. [read, counted]
- Prefixes: almost all base-game keys start `qc_` (so `$qc_...`). The add-on codebases also use
  `mg3_` and `mg3_qc_` (Dawn of the Machine), `map_` (strings that come from map entities, see
  question 6) and one `m_`. The commonest base-game families by count: `qc_death_` (obituaries),
  `qc_ks_` (monster kill lines), `qc_finale_` (end-of-episode text), `qc_need_` (key doors),
  `qc_got_`, `qc_backpack_`, `qc_ff_` (team kills), `qc_suicide_`, `qc_discharge_`. [read, counted]
- Real examples a player would see raw: `$qc_item_armor` (armour pickup line), `$qc_got_item`
  (weapon/power-up pickup line), `$qc_item_health` (health pickup), `$qc_need_gold_key`,
  `$qc_need_silver_keycard`, `$qc_need_gold_runekey` (locked doors), `$qc_death_rl1`,
  `$qc_death_ax`, `$qc_suicide_bored` (obituaries), `$qc_ks_fiend` (killed by a monster),
  `$qc_found_secret` (secret found), `$qc_more_go`, `$qc_three_more`, `$qc_sequence_completed`
  (counter triggers), `$qc_entered`, `$qc_left_game`, `$qc_exited` (player join/leave/exit),
  `$qc_finale_e1` .. `$qc_finale_e4`, `$qc_finale_end`, `$qc_finale_all_runes`,
  `$qc_finale_e1_shareware` (finale text). [read]
- Print paths that carry keys in the base campaign, by count of call sites using a literal key:
  broadcast print (`bprint`) 40; private print to one player (`sprint`) 17; centre print
  (`centerprint`) 12; `WriteString` after an `SVC_FINALE` byte (intermission/finale text) 7.
  [read, counted]
- Fields that hold keys: item `netname` (about 50 assignments, e.g. `$qc_nailgun`,
  `$qc_rocket_launcher`, `$qc_shells`, `$qc_quad`), which are then printed as arguments,
  monster `killstring` (20 assignments, `$qc_ks_...`), printed as the format of the obituary,
  `message` on the secret trigger (set to `$qc_found_secret` by the game code) and on map
  entities (question 6); in Dawn of the Machine an intermission-text global set to a `map_` key.
  [read]
- Obituaries: always broadcast prints whose first argument is a key (or the attacker monster's
  `killstring`, itself a key) followed by player names. [read]
- `dprint` is also given a key in one add-on. [read]
- Not keys: the base game also carries a plain-English item-name table (strings like "Axe",
  "Shotgun", "Super Shotgun"); it is not what the pickup lines print. Achievement ids
  (e.g. `ACH_PACIFIST`, `ACH_COMPLETE_E1M7`, `ACH_FIND_DOPEFISH`) are plain strings without `$`
  sent after an `SVC_ACHIEVEMENT` byte; they are ids, not text. [read]
- The level name: the base game code never prints the world's `message` itself. [read]

## 2. Arguments

- Values are passed as extra string arguments to the same print builtin, after the key:
  e.g. the pickup line is a private print of `$qc_got_item` plus the item's `netname`; the health
  line is `$qc_item_health` plus the amount turned to a string with `ftos`; obituaries are
  `bprint` of the key plus the victim's name plus the attacker's name; the leave line is
  `$qc_left_game` plus the name plus the frag count via `ftos`. [read]
- The builtins that take a key plus arguments are declared variadic in the base game:
  `bprint(string s, ...)`, `sprint(entity client, string s, ...)`,
  `centerprint(entity client, string s, ...)`. In the current release they are bound by NAME,
  not number: each is declared as builtin number 0 with a name, and the compiled progs.dat
  carries a builtin function record whose name is `ex_bprint`, `ex_sprint` and `ex_centerprint`
  respectively (the engines bind any builtin whose function record has no code by looking that
  record's name up in their own table). [read; that progs.dat records carry those names is
  derived from how both engines bind them]
- Earlier updates of the release (before the name binding) used numbers instead: `centerprint`
  #90, `bprint` #91, `sprint` #92 for the argument-taking versions. Both engines rebind those
  numbers to the classic #73, #23, #24 implementations, which already handle the arguments.
  [read]
- `dprint` (#25), `error` (#10) and `objerror` (#11) go through the same argument handling in
  both engines. [read]
- How many: the QC VM passes at most 8 parameters to any call, so `bprint` takes the key plus at
  most 7 arguments, `sprint`/`centerprint` the entity, the key, and at most 6 arguments. The
  game code never passes more than 2 arguments after a key. [read: the 8-parameter limit is the
  VM's; counted: the call sites]
- Arguments are strings only. Numbers are converted by the game with `ftos` before the call.
  [read]
- Placeholder syntax in the localized text: `{}` or `{N}` where N is one or more decimal digits,
  N indexes the arguments after the key from 0. `{}` with no digits means index 0, not "the next
  one" (so a text using `{}` twice repeats the first argument). An index past the arguments
  supplied inserts nothing. Anything else in braces is copied literally. [read]
- The English text's own convention (`{}` vs `{0}`/`{1}`) cannot be seen without the data,
  commented-out lines in one add-on's game code use `{0}` and `{1}`, and one live call in four
  codebases prints the format `{}` with one argument (the item name). [read; the shipped file's
  usage is open]
- Is an argument that is itself a `$` key resolved: yes. Each argument is looked up in the table
  in its own right (that is how a `netname` of `$qc_nailgun` becomes words inside the pickup
  line). Consequence: a player name that happens to equal a key would also be replaced. [read,
  consequence derived]
- When the resolved format contains no placeholder (or no table is loaded), both engines fall
  back to the classic behaviour: the key's text and every argument (each resolved) are
  concatenated in order. [read]
- Output buffer: 1024 bytes; longer output is truncated, and a warning for over 255 characters
  is shown only in developer mode. [read]

## 3. Builtins beyond the classic set

Classic numbers 1..78 keep their classic meaning (with #23, #24, #73 bound by name in the
current release, above). Everything else the shipped progs can call:

| Name a mod author sees | Bound by | Arguments, return | What it does | For messages |
| --- | --- | --- | --- | --- |
| `bprint` | name `ex_bprint` (older: #91) | key, up to 7 strings | broadcast print with placeholders, see question 2 | yes |
| `sprint` | name `ex_sprint` (older: #92) | client entity, key, up to 6 strings | private print with placeholders | yes |
| `centerprint` | name `ex_centerprint` (older: #90) | client entity, key, up to 6 strings | centre print with placeholders | yes |
| `finaleFinished` | name `ex_finaleFinished` (older: #79) | none; returns float | true once the end credits/finale text has finished showing. Both engines return 0 always: the game then polls every 0.1 s forever and stays on the final text, which is the classic ending | no |
| `localsound` | name `ex_localsound` (older: #80) | client entity, sound path | plays a precached sound only that client hears; both engines send it as protocol message 56 (byte flags, then the sound index as a byte, or a short when the large-sound flag is set) and print a warning if the sound is not precached or the entity is not a client. The base campaign declares it but does not call it | no |
| `CheckPlayerEXFlags` | name `ex_CheckPlayerEXFlags` | entity; returns float bit flags | the player's weapon-switch preference: bit 1 = switch only to a newly gained weapon, bit 2 = never auto-switch. Both engines return 0 (always switch, the classic behaviour) | no |
| `walkpathtogoal` | name `ex_walkpathtogoal` | float move distance, vector goal; returns float | moves the monster along the map's navigation nodes. Results: 0 error, 1 reached goal, 2 reached path end, 3 blocked, 4 in progress. Both engines return 0, and the game then falls back to the classic `movetogoal` | no |
| `draw_point`, `draw_line`, `draw_arrow`, `draw_ray`, `draw_circle`, `draw_bounds`, `draw_worldtext`, `draw_sphere`, `draw_cylinder` | names `ex_draw_...` (older: #81..#89 in that order) | positions, colour map index, sizes, lifetime, depth-test flag; no return | debug drawing; vkQuake makes them no-ops; the base campaign never calls them | no |
| `bot_movetopoint`, `bot_followentity` | names `ex_bot_movetopoint`, `ex_bot_followentity` | bot entity plus a point or a goal entity; returns 0 error, 1 success, 2 in progress | bot movement; vkQuake makes them no-ops; the base campaign never calls them | no |
| `checkextension` | #99 | extension name; returns float | the game calls it only when the cvar `pr_checkextension` is non-zero, asking for `EX_MOVETYPE_GIB` and `EX_EXTENDED_EF`; returning 0 (or not having that cvar, so it reads 0) keeps the classic behaviour | no |

[read for all rows; the older numbers #79..#92 are read from both engines' builtin tables and
their notes on the release's first update]

Capture the Flag only: `setcolor` #401 (client, colour), and `prompt`, `promptchoice`,
`clearprompt` bound by name (`ex_prompt`, `ex_promptchoice`, `ex_clearprompt`) for an on-screen
choice menu. [read]

Name binding detail: the engines bind a name only when the function record has no code, no
parameter start and no locals. One engine's table spells the finale builtin all in lower case
while the game code spells it with a capital F; a reimplementation should accept both (compare
case-insensitively). [read; advice derived]

Protocol messages the base game writes itself with `WriteByte`/`WriteString` beyond the classic
ones: 52 `SVC_ACHIEVEMENT` followed by one string (the achievement id), 19 call sites; an engine
must consume the string (both engines ignore it or hand it to a store's achievement API).
[read]

## 4. The localization data

- Location: the English text is `localization/loc_english.txt` inside `QuakeEX.kpf`, a file at
  the root of the re-release's install folder (the folder that holds `id1`; on Steam it is the
  `rerelease` folder of the Quake install). A `.kpf` is a standard ZIP archive: both engines
  open it with a ZIP reader and extract that path from it. [read]
- Both engines also accept the same path as a loose file through the normal game search path or
  under a base directory, and prefer that over the archive. [read]
- Other languages: `localization/loc_<language>.txt` with `<language>` one of `french`,
  `german`, `italian`, `spanish` (the languages both engines know). Whether the release ships
  more files than these is open. When the chosen language's file is missing or holds no
  entries, both engines load `loc_english.txt` instead. [read]
- Encoding: UTF-8. A UTF-8 byte-order mark (EF BB BF) at the very start is skipped if present.
  [read: the engines skip it; whether the shipped file has one is open]
- Line format (as both engines parse it; the release itself publishes no specification):
  - Lines are split on LF. [read]
  - Leading spaces and tabs on a line are skipped. [read]
  - A line whose first character (after that) is `/` is a comment; the proper form is `//`. [read]
  - A line with no `=` is ignored, so blank lines are ignored. [read]
  - The key is everything before the FIRST `=`, with trailing whitespace (including CR) removed.
   Keys in the file do NOT carry the `$`: the lookup strips the `$` from the game's string
   before matching. [read]
  - The value starts after the `=`, after skipping whitespace. If it starts with `"` the value is
   quoted and ends at the next unescaped `"`; anything after the closing quote (including a CR)
   is dropped. If unquoted, it runs to the end of the line with trailing spaces and tabs removed
   (a CR is not removed, so a CRLF file with unquoted values would keep a CR). [read; the CR
   consequence derived]
  - Escapes inside the value: `\n` newline, `\t` tab, `\v`, `\b`, `\f`, `\"` and `\'` the quote
   itself. Any other backslash pair yields the second character alone (with a console warning),
   so `\\` gives one backslash. [read]
  - Keys are matched exactly and case-sensitively. [read]
  - Duplicate keys: the first occurrence in the file wins. [derived from the table's insertion
   order and lookup]
- Whether the shipped file uses quoted values, CRLF or LF, and `{}` or `{N}`, is open (no data
  file was available).

## 5. The engine side

- Where: on the server, inside the print builtins, before the message is written to the client.
  `bprint`, `sprint`, `centerprint`, `dprint`, `error`, `objerror` resolve the key and its
  arguments, then send ordinary finished text (protocol `svc_print` / `svc_centerprint`). Both
  engines also resolve the string of every `WriteString` call the same way (this is how the
  finale text after `SVC_FINALE` reaches the client as words). The client never sees a key from
  these paths. [read]
- Missing key: the original string is used unchanged, so the player sees the raw `$key`. The same
  happens for everything when no table is loaded. [read]
- Whole string only: a string is treated as a key only when it begins with `$`; the entire rest of
  the string (to its end) must match a key exactly. A `$` inside a longer string, or a key with
  text after it, is not resolved and passes through unchanged. An empty string passes through.
  [read]
- Characters: the table's text is converted from UTF-8 to the classic 8-bit character set once,
  at load: ASCII stays as is; code points that the classic font has a glyph for, e.g. bullet
  dots, arrows, boxes, the bracket glyphs, map to that glyph, choosing the plain (not
  high-bit/coloured) variant where both exist; accented Latin letters and similar are
  transliterated to one or two ASCII letters; anything else, and malformed UTF-8, becomes the
  font's box glyph (character 11). The text therefore never contains high-bit (coloured)
  letters; the file cannot ask for them. [read]
- Newlines: the engines neither add nor remove a newline. The game code's print calls carry no
  trailing newline (e.g. the armour pickup is just the key), so the line break must come from the
  text in the file (a `\n` escape at the end of the value). The backpack line is the exception
  the game builds from several prints and ends itself with a literal newline print. [read for the
  engines and the game code; that the file's values end in `\n` is derived]

## 6. Maps

- The game code compares a trigger's `message` against `$map_dopefish` on e2m3 (Scourge of
  Armagon's code does the same with `$map_not_supposed_be_here` on hip3m1), so the release's
  own maps carry `$` keys (prefix `map_`) in entity `message` fields. [read; that the shipped
  maps hold such keys is derived from those comparisons]
- How they are shown: trigger and door messages from map entities are printed by the game with
  `centerprint(other, self.message)` (trigger_once/multiple, door/key messages, the generic
  use-targets message), so they go through the same table as the game's own keys. [read]
- The world's `message` (the level name) is sent by the engine in the server info, not through a
  builtin; neither engine looks it up in the table. Whether the release's maps store the level
  name as a `$` key is open. [read; open]

## 7. Anything else

- Language setting: both engines add a saved cvar `language`, default `auto` (pick from the
  system's preferred locale: en, fr, de, it, es; otherwise English), else one of `english`,
  `french`, `german`, `italian`, `spanish`. Changing it reloads the table at once. [read]
- Episode names: the release's `mapdb.json` (in `id1`) lists the episodes, and their names are
  `$` keys (one engine's note gives `$m_quake` as an example) resolved by the same table; both
  engines do this only for their add-on/mod menus. [read]
- Menus, the scoreboard and HUD text are not localized from the table by either engine; they keep
  their own English. Whether the release's own menu text lives in the same file is open. [read,
  open]
- Key names that change with the input device: nothing in the game code or the engines resolves
  such a placeholder; whether the file contains any is open. [read; open]
- No key is expected from the engine's own built-in table; every key the game prints is looked up
  in the release's file. [read]
- Running the release's progs on a classic engine also needs: the name-bound builtins of question
  3 resolved (an unbound one aborts the program when called), protocol message 52 consumed (one
  string), and, if `localsound` is implemented, message 56. [read; derived]
