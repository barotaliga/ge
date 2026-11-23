# The console and cfg files

Every setting a player, a server's owner or a developer may change without a rebuild is a
**cvar** (a console variable), set from text: a cfg file, the drop-down console, the command
line's `+set`, or the server. A cfg is a plain text file of commands, one a line (or several
split by `;`), that a player writes, keeps and shares. The language is the engine's
(`engine/console`, `ge::console`), so every game on the engine reads the same cfgs. The roadmap
is `docs/goals.md` § M21; the procedure for adding a setting is the `console` skill.

## Names

A cvar's name is short, lower case and starts with the prefix of whoever owns it. After the
prefix comes the thing in one word, abbreviated when it is long: `r_shadowsize`, not
`r_shadow_map_size_texels`.

| Prefix | Owner | Examples |
| --- | --- | --- |
| `sv_` | the server's rules, decided for everyone | `sv_seed`, `sv_hour`, `sv_weather`, `sv_maxclients`, `sv_cheats` |
| `cl_` | the client: the camera, prediction, what it asks of the server | `cl_view`, `cl_fov`, `cl_updaterate` |
| `r_` | the picture | `r_bloom`, `r_taa`, `r_scale`, `r_shadowsize` |
| `s_` | the sound | `s_wind`, `s_effects`, `s_mute` |
| `vid_` | the window and the display | `vid_width`, `vid_height`, `vid_vsync` |
| `in_` | input devices | `in_joystick` |
| `m_` | the mouse | `m_pitch`, `m_yaw` |
| `net_` | the network's plumbing, mostly for developers | `net_fakeloss`, `net_fakelag` |
| `con_` | the drop-down console | `con_height`, `con_speed`, `con_chat` |
| `hud_` | the HUD | `hud_scale` |
| `scr_` | the screen as a whole: its overlays and the screenshots taken of it | `scr_levelstats`, `scr_format`, `scr_quality` |
| none | a few names everyone knows | `name`, `rate`, `sensitivity`, `volume` |

`ge cvar ls <prefix>` lists a game's cvars with a prefix; a new name matches its neighbours'
style.

### Synonyms

A setting other software already knows by another name (a classic game's `maxclients` for
`sv_maxclients`) gets that name as a SYNONYM, never as a second cvar that could disagree:
`Cvars::add_synonym(name, target)` makes `find`, `set` and the console's query and set by the
synonym reach the target. `add_synonym(name, target, part)` names one word of a setting that
holds several, space-separated (`flood_persecond` for the second word of `sv_floodprot`'s
`"N S T"`): setting it replaces that word and sets the whole, and querying it shows the word and
the setting it is part of. A synonym is refused when its name is a cvar of its own, the target
does not exist, or the name already stands for another setting. A server game lists its names
in `ServerGame::names` (`docs/servers.md`).

### Layers: one name, one meaning

A program registers its cvars in LAYERS: the engine's first, then a genre plugin's, then the
game's (`CvarLayer guard(cvars, "shooter")`; a registry starts in `engine`). Each cvar keeps the
layer that registered it. When a later layer registers a name an earlier one has, its spec says
what the two are (`CvarSpec::overlap`):

| `overlap` | Meaning | What happens |
| --- | --- | --- |
| `same` | One setting: the same effect, units and direction | The later layer's default, help, values and range replace the earlier's; both fields are written; a value the player set stays. Another `kind` is refused as a clash |
| `own` | The game's own setting under that name: another meaning | The earlier setting is set aside, unreachable while the layer lasts; the name means the game's |
| `none` (the default) | Not declared | A CLASH: nothing is merged. The earlier cvar stays, the later field is never written, and `Cvars::clashes()` records it (`clash_text` says what to do) |

`Cvars::drop_layer(layer)` takes a layer away, as when a window loads another game: the layer's
own cvars are retired (hidden from every lookup, `cvarlist` and the catalog; registering the name
again revives it at the same address) and what it replaced or set aside comes back with its value.
A command has synonyms too (`Console::add_command_synonym(name, target)`, registered in the
cvars' current layer); `Console::drop_layer` takes a layer's cvars, setting synonyms and command
synonyms away together. When the layer of a `same` row is dropped, a value the player set stays (one setting) and an unset
one goes back to the earlier layer's default. The same layer registering a name twice gets the
first, as it always did. Clashes are never printed to a player; a test fails on them. The rule
for placing a setting and declaring a name, and another game's spelling of one of ours as a
synonym, is `docs/porting-games.md` § One name, one meaning.

**Saving one part** (`Console::save_text(SaveScope)`, `cfg.hpp`): the binds or not, and only the
settings of the layers a filter accepts, each with the value it has there (a set-aside `own`
definition's kept value; a `same` setting in the layer that first registered it). A series of
games keeps one config of every layer but the loaded game's, and each game a file of its own
layer (Shooter's Edge: `docs/porting-games.md` § One config for the series).

### Scopes: a game's own value

A series of games keeps one value per setting, and each game may keep its OWN value over it
(`Cvars::set_scope`, `cvar.hpp` § SCOPES). The scope is the loaded game's name (Shooter's Edge:
the game's folder, `client/cvar_layers.hpp` `game_scope`); `game` stands for the loaded one.

| Line | What it does |
| --- | --- |
| `<game>:<name> <value>` | That game's own value: shown while it is loaded, the common one kept for the rest. |
| `<game>:<name>` | Prints that game's own value, or says it has none. |
| `unset <game>:<name>` | Takes that game's own value away; the common one shows again. |
| `<name> <value>` typed | The common value; the loaded game's own value of it is given up. |
| `<spelling> <value>` in a config | A game's own spelling (a synonym of its layer) read from a file is that game's own value, whatever order the file has it in; our name in the same file sets the common value under it. |

So a config may say `gamma 1` for every game and `vid_gamma 0.9` for the game that spells it so,
in either order. Only reading a file works this way (a line's `Origin` names a file); a value
typed later is the player's and replaces the game's own. Saved, each game's own value is a
`seta <game>:<name> <value>` line after the common ones; a split save sends the loaded game's
lines to its file and the others' to the series' (`SaveScope::scopes`).

**A named save is a pair.** Under a loaded game, `cfg_save <file>` writes the common values (and
other games' own) to `<file>.cfg` and the loaded game's own values to `<file>.<game>.cfg`;
`exec <file>` and `cfg_load <file>` read both (`console.hpp` `game_file`).

### A setting's words

Every cvar says what it does in ONE SENTENCE (`help`: a capital first, a full stop last, no other
sentence inside) and what each value does (`values`), in one grammar `engine/console/words.hpp`
reads: entries `<pattern>: <meaning>` separated by `; `, a pattern a literal (`0`), a range
(`1..32`), `""` (the empty value), `N` (any number), `TEXT` (any text) or several of them
(`N N`), the default's meaning marked `(the default)`. `help <name>` prints the values under the
sentence, a saved cfg writes them as comments, and `value_fits` is what a cfg check asks of each
line. `words_fault` names what is wrong with a cvar's words; a game's test runs it over every
cvar it has (Shooter's Edge's server: `classic_game_every_server_setting_explains_itself`).

### A cfg checked and tidied

`engine/console/cfg_tidy.hpp` reads a cfg without running it, against a console's settings:
`check_cfg` finds a setting the game lacks, a value that fits none of a setting's values (above)
and a setting set twice; `format_cfg` writes the same commands back one a line, a setting as
`name "value"`, with `comments` each setting's help above it (its sentence, the values it names
with what each does, and its default: "(Default: 1).") and with `pretty` the settings in
sections by their prefix (`cfg_section`) in one fixed order, by name, each opened by a blank line
and its header (`// SEATS - Who may join, how many, and who is refused`), one a line with the values in one column and each setting's help beside it in
another, then binds by key, aliases and the other commands in
the order written; with `defaults` (pretty only) every setting of the given flags the cfg leaves
alone joins its section at its default, so the file lists everything it could set. Formatting a
formatted cfg changes nothing. A player's CLI runs both
(`settings check`, `settings format`, `server check`, `server format`: `docs/gamecli.md`).

## Tiers

Each setting has one of three homes, set by its flags (`engine/console/cvar.hpp`):

1. **The player's** (`kArchive`). The player chooses it: the picture, the sound, the camera, the
   controls. `cfg_save` writes it to `config.cfg`, and any cfg the player runs may change it.
2. **The server's** (`kServer`, usually with `kServerInfo`). The server decides it for everyone:
   the map, the rules, the tick rate. Only the server's own console or `server.cfg` (or a listen
   game's host) sets it. With `kServerInfo` every client gets its value in the server info and
   follows it. A client's own `set` of it is refused, with the reason.
3. **Code's.** Everything else a game reads stays a constant in the code. Not every number is a
   setting, and a setting nobody will tune is noise in `cvarlist`.

The other flags:

- `kCheat`: changes only while the server's `sv_cheats` is 1.
- `kLatch`: a new value waits until the game applies it (a restart or a new map), for what is
  read once when something is made (the window, a pipeline).
- `kReadOnly`: only code sets it.
- `kUserInfo`: rides in the user info a client sends the server (`name`).

LOCKS AND RULESETS (`engine/console/ruleset.hpp`). A competition's rules are data: a RULESET holds
some settings at one value, keeps others inside a range and refuses some commands (always, or only
while a match is played). The player picks one by name (`ruleset smackdown`; `rulesets` lists the
game's book, `rulesets <name>` says what one holds), and it changes only while not connected. A
held setting refuses every set with the ruleset's words ("cl_hud is locked by the smackdown
ruleset."), a set into a range is clamped into it, and the value asked for is kept as the
player's: `cfg_save` writes it and leaving the ruleset gives it back. A server's stuffed text is
never refused by a ruleset. The classic games' books are `plugins/classic/rulesets.hpp`.
A ruleset may also keep scripts off TEAM MACROS (`add_macro(..., MacroKind::team)`: the `$` words
for what a player holds and where they are): a line that expanded one runs only the ruleset's
team commands (`say_team`, `if`, the canned reports), and what an allowed command runs (`if`'s
branch) is held the same way, so `if $health < 50 then impulse 7` does nothing; any other command
or setting answers "impulse cannot be used with team macros under the smackdown ruleset." It may
keep TRIGGERS QUIET while a match is played (`msg_trigger`, `re_trigger` and a game's event
triggers ask `Console::triggers_quiet`; sound triggers keep quiet at all times), and cap the
`wait`s in a row a script may spread over frames (`Console::limit_waits`).

A server's stuffed text may run only the commands marked as safe to stuff (`echo`, `set`,
`wait`, `say`, `say_team`, `say_near`) and set only the server's cvars. It never touches a player's files,
binds or archived cvars. Over a network the session carries the server info, the user info and
the stuffed text (`docs/networking.md` § Configs over the session).

`cvarlist` prints the flags as letters: `A` archive, `U` user info, `S` server info, `V` server,
`C` cheat, `L` latch, `R` read only, `*` created by a cfg (no code reads it).

## Startup

A game reads, in order:

1. `default.cfg`: the game's shipped defaults and binds.
2. `config.cfg`: what `cfg_save` last wrote.
3. `autoexec.cfg`: the player's own file, which the game never writes.
4. The command line's commands: `+set r_bloom 0 +exec race.cfg`. A `+` starts each command and
   the first needs none, so `coopgame 2 expert` and `set r_bloom 0 +exec race.cfg` work too.

A missing file is skipped. A dedicated server reads `server.cfg` in place of the first three. A
latched value set during startup is applied once it is over.

**A session's own config.** Every game window's `-config <file>` (`engine/play/session_flags.hpp`,
`Console::set_config_file`) puts that file in
place of `config.cfg`: it is read at startup, every save of the settings (`cfg_save`, the menu
closing, quitting) writes it, and `autoexec.cfg` is skipped. The player's own files are neither read
nor written, so a measurement, a test or an agent's run cannot change them. A file that is not there
yet starts from the defaults and is made by the first save.

## The language

- **Commands** are words split by blanks; `"..."` groups words with blanks into one; `//`
  starts a comment; `;` ends a command (outside quotes). A line is at most 4096 characters.
- **Lookup.** The first word is looked up as a command, then an alias, then a cvar. A cvar's
  name alone prints it; with a value it sets it (`r_bloom 0.1`).
- **Expansion.** `$name` and `${name}` become a macro's or a cvar's value (a macro wins); `$$`
  is a `$`. An alias's body gets its arguments as `$1`…`$9`, `$*` (each quoted as needed), `$@`
  (joined by blanks), `${N-M}` and `${N-}` (a run of them) and `$0` (its own name); a body with a
  `%` in it also takes `%1`…`%9`, `%0` (all of them) and `%%` (a `%`), the form older teamplay
  configs write. `$qt` is a double quote and `$sc` a semicolon.
- **The buffer.** Commands are queued and run once a frame. `exec` and an alias put theirs at
  the front, so they run before what followed them. `wait` leaves the rest for the next frame.
  An alias or a trigger that calls itself is stopped after 64 nested calls in one frame, with a
  problem naming it; a loop that waits is a loop on purpose and runs on.
- **Problems.** Every refusal is printed with the file and line it came from and the next
  action; an unknown name gets the nearest known one ("did you mean 'r_bloom'?").

## Binds

`bind [-<mode>] <key> [<command>]` sets what a key runs.

- **Keys** are named in lower case:
  - `a`…`z` and `0`…`9`, `f1`…`f12`;
  - `space`, `enter`, `tab`, `escape`, `backspace`;
  - `up`, `down`, `left`, `right`;
  - `shift`, `ctrl`, `alt`;
  - `ins`, `del`, `home`, `end`, `pgup`, `pgdn`;
  - the keypad: `kp_0`…`kp_9`, `kp_enter`, `kp_plus`, `kp_minus`, `kp_star`, `kp_slash`,
    `kp_del`;
  - the punctuation keys by their character (`[` `]` `,` `.` `/` `\` `'` `-` `=` `` ` ``), and
    `semicolon`;
  - the mouse: `mouse1`…`mouse8`, `mwheelup`, `mwheeldown`;
  - the gamepad: `pad_a`, `pad_b`, `pad_x`, `pad_y`, `pad_lb`, `pad_rb`, `pad_lt`, `pad_rt`,
    `pad_back`, `pad_start`, `pad_ls`, `pad_rs`, `pad_up`, `pad_down`, `pad_left`, `pad_right`.
  - older spellings in players' configs are read as the key they name: `uparrow` and the other
    arrows written out, `lctrl` and `rctrl` (both `ctrl`, as are the alt and shift pairs),
    `kp_home` (`kp_7`) and the other keypad keys by their arrows, `~` (`` ` ``), `esc`.
- **Modifiers** come before the key: `ctrl+s`, `alt+shift+f1` (kept in the order ctrl, alt,
  shift). A key held with modifiers runs its modified bind if it has one, else its own.
- **Modes**: `game` (the default), `console`, `menu` and `demo` each keep their own table.
- **Buttons.** A bind that starts with `+` runs that command when the key goes down and the same
  command with `-` when it comes up: `bind space +jump`, or `bind v +voice_prox` to talk to the
  players near you while the key is held. Any other bind runs on the press. A game registers
  its held actions as buttons (`Console::add_button`).
- **Only buttons take a `+`.** Every other command is typed bare, in the console and in a cfg
  (`coopgame 2 expert`, `map e1m2`). A `+` typed on a plain command (or alias) is dropped, so
  `+coopgame 2 expert` runs `coopgame 2 expert`, and the `-coopgame` a key bound to it sends on
  its release runs nothing.

The backquote key (under the tilde) is bound to `toggleconsole` in the game and console modes
by default.

## Built-in commands

### Variables

| Command | What it does |
| --- | --- |
| `set <name> <value>` | Sets a cvar. A name no code reads is created (marked `*`). |
| `seta <name> <value>` | The same, and archives it, so `cfg_save` writes it. |
| `reset <name>` | Puts a cvar back to its default. |
| `cvarlist [<prefix>]` | Every cvar, its value, flags and help; `[meta]` marks a meta-cvar, `[by <meta>]` a value one set. |
| `meta <name> <level> "<name> <value>; ..." ...` | Declares a META-CVAR: each level of `<name>` sets the cvars in its quotes, unless a cfg, the console or the command line set them first or later (explicit beats implied); a level that leaves one out puts it back to what it was under the meta. A level may include another meta's level by naming it (`comp 1`); a loop of includes is refused, named. `meta <name>` alone shows it. Code declares one with `Cvars::add_meta` (docs/servers.md § Meta-cvars). |
| `setu <name> <value>` / `sets <name> <value>` | Sets a cvar (every word after the name its value), created when nothing reads it, and makes it part of the user info (`setu`) or the server info (`sets`), the other mark taken away. |
| `resetall` | Every cvar back to its default, but the read-only ones and `game`. |
| `userinfo` | "User info settings:" and each user-info cvar with a value, its name padded to 20. |
| `setinfo [<key> <value>]` | Shows or sets a key of this player's user info. |
| `serverinfo [<key> <value>]` | Shows or sets a key of the server's info. |
| `help <name>` | What a cvar, command or alias is: its value, default, range and help; a meta-cvar's levels, and the meta behind a value it set. |

### Aliases and files

| Command | What it does |
| --- | --- |
| `alias <name> "<commands>"` | A new command that runs the commands. With no body, prints it. |
| `tempalias <name> "<commands>"` | The same, but `cfg_save` does not write it. |
| `unalias <name>` / `unaliasall` | Removes one alias, or every alias. |
| `aliaslist` | Every alias and its body. |
| `exec <file>` | Runs a cfg file's commands (`.cfg` is added when there is no extension). |
| `cfg_save [<file>]` | Writes every bind, alias and archived cvar (default `config.cfg`); a named file under a loaded game also writes `<file>.<game>.cfg` (§ Scopes). |
| `writeconfig [-a] [-b] [-c] [-m] <file>` | A cfg of the player's own: binds (`-b`) and archived cvars (`-c`) by default, aliases with `-a`, every changed cvar with `-m` in place of the archived; `// generated by writeconfig` on top, `.cfg` added. |
| `cfg_load [<file>]` | Runs a saved config (default `config.cfg`), and its `<file>.<game>.cfg` when there is one. |
| `unset <name>` | Takes away a setting `set` made (a setting the game registered stays: `reset` sets it back). |
| `unset <game>:<name>` | Takes a game's own value of a setting away (§ Scopes). |
| `cmdlist [<prefix>]` | Every command and what it does. |
| `cvarlist_re`, `cmdlist_re`, `cvar_reset_re`, `toggle_re`, `unset_re`, `unalias_re` `<pattern>` | The list, reset, flip, removal or alias removal over every name a regular expression matches (anywhere in the name, without case: `^cl_` for a prefix). `unset_re` takes plain names only, never a game's own value. A pattern the console cannot read changes nothing. |
| `echo <words>` | Prints the words with one blank between each. |
| `_echo [-e] [-n] [-c <colour>] [-l <level>] <text>` | Prints the text as written, its spacing kept; `-e` reads backslash escapes (`\n` splits the line), `-h` the options. |
| `text <words>` | Runs the words as the next command, before the rest of the buffer. |
| `complete <name>` | A command the game knows (sent to the server when typed, completed with Tab); refused for a cvar's name. Typed with no server, "Can't "<name>", not connected". |
| `writemanual <file>` | Every setting, command and launch flag of the program written for the reference manual (§ The reference manual). |
| `lasterror` | The last refusal printed again. |
| `wait` | Leaves the rest of the buffer for the next frame. |

### Scripting

| Command | What it does |
| --- | --- |
| `if <a> [<op> <b>] [then] <commands> [else <commands>]` | Runs one branch. The ops are `==` `=` `!=` `<>` `<` `<=` `>` `>=`, `isin` `!isin` (a substring of b), `isini` `!isini` (the same in any case), `eq` `ne` (text, any case) and `=~` `!~` (b a regular expression found in a). With no op, `a` is tested as true (not empty, not 0). `then` may be left out after `a op b`, as old configs write it. |
| `if (<expression>) then <commands> [else <commands>]` | Runs one branch on an expression (below). |
| `if_exists <cvar\|alias\|trigger\|command> <name> <commands> [<else>]` | Runs the first commands when the name exists, else the second. |
| `eval <expression>` | Prints the expression's value and its kind: `Result: 3.5 (number)`. |
| `set_eval <name> <expression>` | An expression's value into a cvar. |
| `set_calc <name> <a> <op> <b>` | A calculation into a cvar. The ops are `+` `-` `*` `/` `%`, `div` (whole numbers), `and` `or` `xor` (bits) and the comparisons; a division by zero stores 999, which old configs test for. Also `set_calc <name> strlen <s>`, `substr <s> <from> [<count>]`, `strstr <s> <find>` (or `pos`), `int <a>`, `tobrown <s>` and `towhite <s>` (the font's coloured and plain halves). |
| `inc <name> [<by>]` / `dec <name> [<by>]` | Adds to or takes from a cvar (by 1). |
| `mul <name> [<by>]` / `div <name> [<by>]` | Multiplies or divides a cvar (by 1); dividing by 0 keeps it. |
| `toggle <name> [<v1> <v2> ...]` | Flips a cvar between 0 and 1; with values, sets the one after its own (matched without case, wrapping); a value not in the list is refused, "can't cycle". |
| `cycle <name> <v1> <v2> ...` | Sets a cvar to the value after its own in the list. |
| `msg_trigger <alias> <match> [-l <level>]` | A game line of that print level (0 low, 1 medium, 2 high, the default; 3 chat; `t` the team's chat) holding `match` runs the alias; `$triggermatch` is the line (a chat line's words after its speaker), until the next map. Alone, the list. A line that asks or answers a version query fires none. |
| `alias_in <alias> <cvar> [<options>]` / `cvar_in <cvar1> <cvar2> [<options>]` | A cvar's value put at the start of an alias's text or another cvar's value; options, a sum: 1 at the end, 2 not when it is there already, 4 say so, 8 make the alias or cvar. |
| `alias_out <alias> <cvar> [<options>]` / `cvar_out <cvar1> <cvar2> [<options>]` | The first place the value is found taken out; options 1 only test, 2 quietly. |
| `set_alias_str <cvar> <alias>` / `set_bind_str <cvar> <key>` | A cvar set to an alias's text or a key's bind. |
| `hash <text>` | The text's hash, a whole number (letters in either case alike). |
| `msg_trigger_remove <alias>` | Stops a trigger. |
| `re_trigger <name> <pattern>` | A game line a regular expression finds runs the alias `<name>` at once; `re_trigger_options`, `_delete`, `_enable`, `_disable`, `_match` and `re_trigger_match_0..9` (`console/re_triggers.hpp`). |

**Clock macros** (`engine/console/clock_macros.hpp`, a game adds them with its clock):
`$com_date` and `$com_time` by `com_date_format` (`%Y-%m-%d`) and `com_time_format` (`%H:%M`),
the date letters of the C library; `$com_uptime` (`MM.SS`, `H:MM.SS`, `D+H:MM.SS`),
`$com_uptime_long` ("1 day, 2 hours, 5 secs") and `$random` (a digit from the console's seed);
the older clients' fixed forms `$time` (14:05), `$date` (22.11.26), `$timestamp` (20261122-1405),
`$dateiso` (2026-11-22_14-05) and `$rand` (0 to 1, six decimals);
`level_clock` (`$cl_timer`, "4:05", "1:4:05") and `tenths_clock` (`$cl_demopos`, "3:07.4") are the
forms a game's own clocks print in. A joined server's macros (Shooter's Edge,
`net/qw_remote_macros.hpp`): `$cl_ping` (also `$ping` and `$latency`), `$cl_lag` ("1.25%"),
`$serverip`, `$connectiontype` (disconnected, spectator or player), and the last whole second's
`$cl_pps`, `$cl_mps`, `$net_uprate`, `$net_dnrate` (`engine/net/rates.hpp` over the transport's
counts). **The files and the network** (`engine/vfs/search_commands.hpp`, `engine/net/net_commands.hpp`):
`path`, `dir <folder> [ext]`, `fdir <pattern>`, `whereis <file> [all]`, `dns <name>` and `net_stats`.
**Draw strings** (`engine/hud/draw_strings.hpp`): `draw <name> <x> <y> [colour]` keeps a cvar's or a
macro's value on the screen (a negative place counts from the right or bottom), `draw` alone
lists them, `undraw <name>|all` takes them off.

**Expressions** (`engine/console/expr.hpp`): numbers (an integer stays one through `+ - *`; `/`
gives a fraction), text in `"..."` or `'...'` or a bare word, `%name` for a cvar's value (`$name`
is expanded first, as on any line), `+ - * /` (`+` joins text), the comparisons above, `and` `&&`
`or` `||` and brackets. A comparison gives 1 or 0. A fault (an unclosed bracket, a division by
zero) is a problem naming it.

**The corpus.** Players' configs as advanced as the ones they carry (weapon scripts, team
reports and triggers, a HUD layout kept as cvars, an old config with no `then` and upper-case
keys, computed scripts) are run from startup through a script of keys and game lines and held
golden with the config they save: `tests/golden/console_configs/`, `tests/console_configs_test.cpp`.

### Binds

| Command | What it does |
| --- | --- |
| `bind [-<mode>] <key> [<command>]` | Sets what a key runs; with no command, prints it. |
| `unbind [-<mode>] <key>` | The key runs nothing. |
| `unbindall` | Every key in every mode runs nothing. |
| `bindlist [-<mode>]` | Every bind. |

### The drop-down console

| Command | What it does |
| --- | --- |
| `toggleconsole` | Opens or closes the console. |
| `clear` | Empties the scrollback. |

### Talk: chat and voice, for a multiplayer game

A single-player game has no chat. A multiplayer game opts in with `console/talk.hpp`'s
`add_talk` and a `TalkSpec` naming the routes it has (a team, the players near, one player); the
lines go to the view's chat sender, which the game points at its network, and the game reads the
voice keys each frame (`talk_voice`, the narrowest route held winning). Keys are bound only where
the player bound nothing.

| Command | Key | What it does |
| --- | --- | --- |
| `say <text>` | | A chat line to everyone. |
| `say_team <text>` | | A chat line to your team. |
| `say_near <text>` | | A chat line to the players near you (the game's server decides how near). |
| `whisper <nick> <text>` | | A chat line to one player. |
| `messagemode` | T | The chat line opened for everyone: Enter says it, Escape drops it. |
| `messagemode2` | Y | The chat line for your team. |
| `messagemode3` | U | The chat line for the players near you. |
| `+voice` | V | Talk to everyone while held. |
| `+voice_team` | B | Talk to your team while held. |
| `+voice_near` | N | Talk to the players near you while held. |

### Ignore lists: chat a player stops reading

`console/chat_ignore.hpp` (`add_chat_ignore`): `ignore <name or number>`, `ignore_id`, `unignore`,
`unignore_id`, `ignorelist`, `ignore_team`, `unignore_team`, `unignoreall`, `unignoreall_team`, and
the settings `ignore_mode` (an ignored player's team chat too), `ignore_opponents` (0..2),
`ignore_spec` (0..2), `ignore_flood` (0..2) with `ignore_flood_duration`, and `ignore_tv`. The game
reads each incoming chat line into a `ChatLine` (who said it and how) and keeps a `ChatRoster` of the
players; `chat_ignored` says whether the line goes, and `chat_new_level` clears the players' flags
on a new map. A dropped line reaches nothing: not the console, the chat sound or a trigger.

The filters: `ignoretext <words>` drops a chat line that is wholly those words (`*` any run, `?`
one character, `\` the next as written; case counts; 3 characters at least), `ignorenick <name>`
a line said by that name (`name: `, `(name): `, after a leading `[...] `), whoever has it;
`unignoretext` and `unignorenick` take one away (`all`: every one), and either alone lists them
with the lines each dropped. They are kept until quit and need no roster.

## The drop-down console

The backquote key (under the tilde) slides the console down from the top of the screen over the
game. It holds everything that happened, each line coloured by its kind: the console's own
output, the commands typed (after `]`), chat to everyone, to the team, to the players nearby and whispers, the server's
messages and the game's events. Closed, the newest few lines show at the top of the screen for
a few seconds (the notify lines).

While a game loads, the game can hold the console over the whole screen (`set_full`, as the
classic games start); let go, it slides up at `con_speed` to where it was.

The input line edits in place (left, right, home, end, backspace, delete). Page Up and Page
Down scroll the lines. Up and Down walk the history while no list of suggestions is showing.

**Autocomplete.** While the first word is typed (after a `/` or `\`, or bare while `con_chat` is not 1),
every command, alias and cvar it starts is listed under the input line: the one submitted most
often from this console first, then the shortest, then by name, at most `con_suggest` of them.
Each row shows a cvar's value after `=`, an alias's body after `:` or a command's help. The rest
of the top name is drawn dim after the cursor, and Tab takes it: `/sv_dea` and Tab gives
`/sv_deathmatch `, ready for `1`. Up and Down move through the list (the hint follows the
selected row), Tab takes the selected one, and Enter on it fills in a cvar's name for its value
or runs a command or an alias. A blank after the word ends the list, unless the word is a setting
of a few values.

**A setting's few values.** A cvar that takes at most eight values (literal words in its values
text, a whole-number range of an integer cvar spelled out, a boolean's 0 and 1; `value_choices` in
`console/words.hpp`) lists them in its row, `r_look = modern  (classic, modern)`. Typed with a blank
after it, `/r_look `, the list becomes its values in their own order, each with what it does and the
one it has now marked `(now)`; the typed text narrows them, Tab takes one and Enter runs the line.

**A game's spellings.** A game's own name for one of ours (a cvar synonym or a command synonym, §
Layers) is listed too, with the setting's value and its few values, and says whose it is: `gl_look
= modern  (classic, modern)  alias of r_look`. Our name says the game's: `r_look = modern  alias
gl_look`.

Enter submits the line:

- a line starting with `/` or `\` is a command, with that character dropped: `/r_bloom 0`,
  `/say hi`;
- a line starting with `//` is a note: shown in the scrollback, never run or said;
- any other line is a chat message to everyone while `con_chat` is 1 (a multiplayer game sets
  it), a command while it is 0 (single player has nobody to chat to), with 2 a command when
  its first word names a command, alias or cvar (in any case), else a chat message, and with 3
  a message to the team. A game with its own setting for this (`ConsoleView::game_chat`, in its
  order: 0 command, 1 everyone, 2 the team; Quake II's `con_auto_chat`) rules over `con_chat`.

`clearnotify` lets the notify lines so far go. `remotemode [address [password]]` sends every line
typed without a slash to the game's `rcon` command (setting `rcon_address` and `rcon_password`
when given and the game has them), the prompt `#` in place of `]`; `remotemode` again ends it. A
game joining a server shows what is coming on the console's FOOT (`set_foot`): a bar and the file's
share and size, or "Loading <map>...".

`condump [name]` writes the scrollback to `<folder>/<name>.txt` (`condump.txt` with no name), the
folder being the game's (`set_dump_folder`); a name with a folder in it is refused.

Its cvars:

| Cvar | Default | What it is |
| --- | --- | --- |
| `con_height` | 0.5 | the share of the screen it covers when down |
| `con_speed` | 3 | how fast it slides, screens a second (0: at once) |
| `con_lines` | 1024 | the lines of scrollback kept |
| `con_notify` | 1 | 0: no notify lines at all |
| `con_clearnotify` | 1 | 1: opening or closing the console lets the notify lines so far go (the scrollback keeps them) |
| `con_notifylines` | 4 | the newest lines shown at the top while it is closed (0: none) |
| `con_notifytime` | 4 | how long a notify line stays, s |
| `con_chat` | 0 | a line typed without a `/`: 0 a command, 1 said to everyone, 2 a command when its first word is known, else said, 3 said to the team |
| `con_clock` | 0 | 1: the local time in the panel's lower right corner |
| `con_scale` | 0 | the text's size against the console's own (0: 1) |
| `con_suggest` | 8 | the suggestions listed under the input line (0: none) |
| `con_bgtop` | 10141c | the panel's colour at its top, `rrggbb` |
| `con_bgbottom` | 10141c | the panel's colour at its bottom (the same as the top: a flat fill) |
| `con_bgalpha` | 0.85 | how much of the game the panel hides, 0 to 1 |
| `con_bgimage` | (none) | a picture drawn over the gradient, a path in the game's files |
| `con_font` | mono | the charset the console writes in (`docs/charsets.md`: `terminal`, `mono`, ...); empty for the UI font, and a name no charset has draws in the UI font too |

The view (`engine/console/view.hpp`) is a model that holds no pixels. `ui::draw_console`
(`engine/ui/console.hpp`) turns it into the 2D layer's draw list, which the game draws over its
frame and the `console` lab draws on the CPU (`make lab LAB=console`). A game sets the look (the
sizes, each kind's colour, the list's colours) in a `ConsoleStyle`.

The keyboard reaches it through `ui::feed_console` (`engine/ui/console_keys.hpp`): each frame's
key events go to the binds by their bind names (`console::usage_key`, held modifiers spelled
before the key), in the `console` table while the view is down and the `game` table while it is
up, so the key under the tilde opens and closes it; while it is down the input line also takes
the edit keys (with a held key's repeats) and the typed text, and `ConsoleKeys::held` tells the
game to read no keys that frame. Fall Line's window does exactly this: the tilde drops the
console over the run, the skier coasts on with no hands while it is down, Escape closes it
(rather than quitting), and the 2D pass draws it after post. The `world` lab's `console` still
(`--console=1`, the default) photographs it over the game's own frame.

## Tooling

```sh
ge cvar ls [<prefix>]            # every cvar of the game: value, default, kind, range, flags, help
ge cvar get <name>...            # the named ones (an unknown name: the nearest known)
ge cfg check <file>... [--server]   # every problem at file:line, and each name nothing reads
ge cfg exec <file>... [--cmd=...]   # the same, then what changed and what it printed
ge cfg dump [--out=my.cfg]       # every player setting, commented: a cfg to start from
```

`--server` checks a cfg as a dedicated server's, where the server's cvars may be set.

## The reference manual

The player's manual of every setting, console command and launch flag is generated from the
programs' own registrations, never kept by hand, so it cannot fall behind the code
(`engine/console/manual.hpp`, `manual_page.hpp`; row M21f):

1. **Each program describes its live console** (`describe`): every setting with its layer, kind,
   default, range, flags, help and values; every setting synonym and command synonym (a game's
   own spelling of ours); every command with the layer it was added in; and the flags the program
   documented (`Console::set_manual`'s `FlagDoc`s, the same table its parser refuses unknown
   flags by). A program with games writes one PASS per game: Shooter's Edge's window loads each
   game's own layer in turn as a game switch does (`se -manual <file>`, client/manual.hpp; the
   Quake pass is QuakeWorld's too), the dedicated server writes the game it serves
   (`sesv [-classic q2] --manual=<file>`); any console writes its one pass with `writemanual`.
2. **`ge manual <source>...`** merges the passes into rows: a name's COMMON definition (the layer
   under the games'), each game's own where its layer met the name (`same`: the game's default and
   words; `own`: the game's own meaning under the same name), its spellings per game, the games it
   is in (none named: all) and the programs that have it.
3. **The page** (`user-manual.html`, one file with no outside script, and `manual.json` beside
   it) searches names, spellings and help, filters by game, kind, layer and program, shows the
   picked game's own default and meaning in place, and links every row (`#setting-gamma`).

`make manual` runs all three into `build/manual/` (`tools/manual.sh`). The window needs one game to
start: Quake's shareware release, fetched once and checked by the classicenhance tool
(`GE_MANUAL_GAME` names another source); every other game's own layer is loaded without its
files. The `manual` workflow
publishes it to the repository's pages once a day. A new setting, command or flag needs nothing
more than its registration, its help and, for a flag, its row in the program's flag table.

