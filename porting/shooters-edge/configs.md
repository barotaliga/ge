# The console language, specified

A specification in our words of the console language players' configs use, the classic core
and what today's clients added (README.md, the rules), for M18k. `docs/shooters-edge.md`
§ Configs lists the features; this note states the exact behaviour of each, with a small
fixture per feature: an input, and what the console prints or what state results. The engine's
own console (`docs/console.md`) is what implements it; this note is the behaviour it must
reproduce for players' existing configs to work.

**Where each fact came from.** A research session read the modern client's command buffer,
tokenizer, cvar and key modules, its teamplay macros, triggers and location files, its config
manager and its expression parser (2026-10-30), with the **modern client's behaviour and
defaults as the baseline**; the classic core (lines, `;`, quotes, `alias`, `bind`, `exec`,
`wait`, `+`/`-` commands) is the same in the 1996 release. The fixtures are derived from the
rules as read, not yet run against the modern client: each is marked with what to confirm when
`se-configs` (or a scripted run of the modern client) can check it; the ones most likely to be
wrong are marked **verify**.

## Lexing

1. **Lines.** A buffer is cut into command lines at a newline, or at `;` outside double quotes.
   A carriage return is dropped. A line is at most 1023 characters; the rest is lost.
2. **Comments.** `//` outside quotes starts a comment: the rest of that line (up to the newline,
   so a `;` after `//` does not start a new command) is ignored.
3. **Tokens.** Whitespace (space, tab, CR) separates tokens. A token that starts with `"` runs
   to the next `"` (or the line's end) and may hold spaces and `;`; the quotes are not part of
   it. A quote inside a word does not start a quoted token: `a"b` is one token `a"b`. A token is
   at most 1023 characters; at most 80 tokens a line. `//` at the start of a token ends the line.
4. **Curly braces** (only when `cl_curlybraces` is 1, default 0): `{` … `}` group like quotes and
   nest; a backslash before the newline joins the next line.
5. **Arguments.** `argv(0)` is the command name; "the rest of the line" (`args`) is the raw text
   after the first token, quotes kept; commands that take "the rest" (alias bodies, bind
   commands, cvar assignment) join `argv(1..)` with single spaces, which drops the quotes.

Fixtures:

```
> echo a;echo b                       prints: a / b
> echo "a;b"                          prints: a;b
> echo a // b;echo c                  prints: a            (the ; is inside the comment)
> echo "x    y"   z                   prints: x    y z
> echo a"b c"                         prints: a"b c"       (verify: two tokens a"b and c")
```

## The command buffer

1. **Buffers.** Text waits in a buffer until the frame runs it. The modern client keeps several,
   run each frame in this order: the main buffer (keys, the console, configs; 256 KB), the safe
   buffer (message triggers; 2 KB), the formatted-comms buffer (lines that used a teamplay macro;
   2 KB), the server buffer (a local server's own; 256 KB) and the demo buffer (8 KB). Text the
   server stuffs goes to its own buffer (8 KB), run with the main buffer's rules plus § What a
   server may stuff. A buffer that would overflow drops the new text and prints an overflow.
2. **Append versus insert.** Typing, a key's bind and a stuffed command append at the end.
   `exec` and an alias **insert** their text at the front, so it runs before what was already
   waiting: a script runs to its end before the line after the `exec`.
3. **A stuffed line runs only when complete** (it ends in a newline); a half-arrived line waits
   for the rest.
4. **`wait`** stops the buffer for this frame; the rest runs next frame. The modern client limits
   a chain of waits only under one tournament ruleset (10); otherwise 32768.
5. **Runaway guard.** If running a line made the buffer longer more than 1000 times in one frame
   (an alias calling itself), the buffer is cleared with "A recursive alias has caused an infinite
   loop". There is no other depth limit.
6. **Lookup order** for a line's first token: a command, then a cvar, then an alias, then a
   legacy name (an old command renamed to a new one), else "Unknown command" (printed only when
   `cl_warncmd` is 1, default 1). Names are case-insensitive.
7. **Forwarding.** A client command registered without a function (`kill`, `god`, `give`,
   `noclip`, `fly`), and `cmd <text>`, go to the server as a string command (the command name
   lowercased). `cmd` with no server prints "Can't "cmd", not connected".
8. **`exec <file>`** loads the file from the game's search path, adding `.cfg` when the name has
   no extension; it refuses files whose extension is in `cl_allow_downloads` (default `bsp, lmp,
   loc, mdl, mvd, pcx, spr, wad, wav`), so a downloaded file cannot be run. It prints
   "execing <path>" when `cl_warnexec` is 1 (default) and "couldn't exec <name>" on failure.
9. **Command line.** `+set name value` on the command line is applied before the configs run;
   every other `+command args` (up to the next `+` or `-` word) is appended after them.

Fixtures:

```
> alias a "echo 1;wait;echo 2"; a; echo 3
  frame 1 prints: 1        frame 2 prints: 2 / 3        (alias text is inserted before "echo 3")
> exec x.cfg                  (x.cfg holds "echo in")   then "echo after" on the same line
  prints: in / after
> alias loop "loop"; loop     prints the recursive-alias warning; the buffer is empty after
> KILL                        sent to the server as "kill"
```

## Cvars

1. **A cvar** is a name and a string value; its number is the string read as a float (0 when it
   is not a number). Names are case-insensitive. Each has a default (shown by the bare name with
   `cvar_viewdefault`, default 1).
2. **Reading**: a line that is just the name prints `"name" is "value"` (with the default shown
   too under `cvar_viewdefault`).
3. **Assigning**: `name value…` sets it to the rest of the line joined with single spaces. Only
   for an existing cvar; an unknown name is an unknown command.
4. **`set name value`** takes exactly two arguments (quote a value with spaces): it sets an
   existing cvar, or creates a user cvar (kept, and saved in the config's user section); naming
   a command prints "is a command". `unset name` deletes a user cvar.
5. **`toggle name…`**: each named cvar becomes "0" if its number is non-zero, else "1".
6. **`inc name [delta]`**: adds `delta` (default 1). There is no `dec`: use `inc name -1`.
7. **Number formatting** when a command writes a number (inc, toggle, set_calc): `%f`, then
   trailing zeros and a trailing point removed: 1 → "1", 0.5 → "0.5", 1/3 → "0.333333".
8. **Flags** that change behaviour: user info (mirrored into the user info string; `setinfo`
   for keys with no cvar), server info (a local server's), read-only, set-at-start-only, latched
   (takes effect on a video or sound restart), "not from server" (a stuffed line may not change
   it), "user created" and "mod created" (made by `set` from the player or from the server).
   There is no archive flag in the modern client: the config writer saves every cvar whose value
   differs from its default (§ Saving).
9. **Not in the modern client**: `seta`, `dec`, `cycle`. The M18k list names them; decision for
   us: accept `seta` as `set` (other configs use it) and `cycle` / `dec` as ours, documented as
   additions.
10. **Info strings**: `setinfo key value` sets a user info key (sent to the server when
    connected, `protocol.md` § 9); `setinfo` alone lists them. `serverinfo` asks the server to
    print its info.

Fixtures:

```
> sensitivity 3.5 ; sensitivity        prints: "sensitivity" is "3.5"
> set myvar "a b" ; myvar              prints: "myvar" is "a b"
> set myvar a b                        prints: Usage: set <cvar> <value>
> toggle cl_nopred ; cl_nopred         prints: "cl_nopred" is "1"
> inc fov 2.5   (fov was 90)           fov is "92.5"
> inc fov -92.5                        fov is "0"
> nosuchvar 1                          prints: Unknown command "nosuchvar"
```

## Aliases

1. **`alias name text…`** defines (or replaces) `name` as the rest of the line joined by single
   spaces; `alias name` prints it; names are at most 31 characters, case-insensitive.
   `unalias name`, `unaliasall`, `aliaslist [pattern]`, `viewalias name`.
2. **Running** an alias inserts its text at the front of the buffer, followed by a newline.
3. **Arguments** use `%`: `%1` … `%9` are the call's arguments, `%0` all of them (the raw rest of
   the line). Any `%` in the body turns substitution on; then `%%` gives a single `%` and any other
   `%x` is left as it is. A missing argument is the empty string. (`$1`…`$9` and `$*` are not the
   modern client's: `$` is cvar and macro expansion, § Expansion.)
4. **Pass-through**: an alias whose body has no `%`, is a single word naming a cvar or a command
   that does not start with `+` or `-`, and is called with arguments, gets the arguments appended:
   `alias s sensitivity` then `s 4` sets sensitivity to 4.
5. **`+` and `-` aliases**: a key bound to `+name` runs `+name <key number>` on press and
   `-name <key number>` on release (§ Binds), so `alias +zoom "fov 40"` / `alias -zoom "fov 90"`
   make a held zoom. The key number is an argument the alias ignores unless it uses `%1`.
6. **`tempalias`** defines an alias that is not saved in the config.
7. **Server aliases**: an alias defined by a stuffed line is marked as the server's; it runs in
   the server's buffer under its rules, and the server's aliases are deleted on disconnect.

Fixtures:

```
> alias hi "echo hello %1 and %2" ; hi a b          prints: hello a and b
> alias all "echo [%0]" ; all x "y z"               prints: [x "y z"]   (verify the quotes)
> alias pct "echo 100%%" ; pct                      prints: 100%
> alias s sensitivity ; s 4 ; sensitivity           prints: "sensitivity" is "4"
> alias +z "fov 40" ; alias -z "fov 90" ; bind f +z  holding f: fov 40; releasing: fov 90
```

## Expansion

1. **When**: every line is expanded just before it runs (after it is cut from the buffer, before
   it is tokenized). Text inside double quotes is not expanded. So `alias a "echo $x"` keeps `$x`
   and expands it each time `a` runs, while `alias a echo $x` expands it once, at definition.
2. **What**: `$` followed by a name. The name is the longest run of characters above space that
   is not `$`; of its prefixes, the longest that names a cvar and the longest that names a macro
   are found, and the longer wins; its value replaces it and the unused tail of the run stays as
   text. When nothing matches, the `$` and the run are kept as typed. Cvar names match without
   regard to case.
3. **Macros** are built-in read-only values: `$health`, `$armor`, `$armortype`, `$shells`,
   `$nails`, `$rockets`, `$cells`, `$ammo`, `$weapon`, `$weaponnum`, `$weapons`, `$bestweapon`,
   `$bestammo`, `$powerups`, `$location`, `$deathloc`, `$took`, `$tookloc`, `$point`,
   `$pointloc`, `$need`, `$time`, `$date`, `$latency`/`$ping`, `$qt` (a double quote),
   `$lastip`, `$gamedir`, `$triggermatch`, `$team1`, `$team2`, and more. Teamplay macros (health,
   armour, ammo, weapons, powerups, locations) switch the rest of the line to the formatted-comms
   buffer's rules: only `say`, `say_team`, `echo`, `if`, `wait` and the `tp_msg…` commands may use
   them.
4. **A literal `$`**: put it in quotes, or use `$$` in chat text (§ Team messaging, the fun
   characters); outside quotes `$$x` expands `x` after a literal `$` (verify).
5. **Output length**: an expanded line is cut at 1023 characters.

Fixtures:

```
> set n 5 ; echo $n                    prints: 5
> echo "$n"                            prints: $n
> echo $nope                           prints: $nope
> set ab 1 ; set abc 2 ; echo $abcd    prints: 2d        (longest cvar prefix wins)
> alias e echo $n ; set n 6 ; e        prints: 5         (expanded at definition)
> alias e "echo $n" ; set n 7 ; e      prints: 7
```

## Conditionals and arithmetic

1. **`if` (classic form)**: `if <a> <op> <b> [then] <commands…> [else <commands…>]`. Operators:
   `==` or `=` (numeric when both sides are numbers, else exact string), `!=` or `<>`, `<`,
   `<=`, `>`, `>=` (numeric), `isin` / `!isin` (a is a substring of b), `=~` / `!~` (b is a
   regular expression matched against a; its groups go to the trigger-match variables). The
   chosen commands are joined with spaces and inserted as one line; an unknown operator prints
   the list of valid ones.
2. **`if` (expression form)**: when the first argument starts with `(`, the condition is an
   expression up to the word `then`: `if (<expr>) then <cmds> [else <cmds>]`. `then` is required.
   The expression language: numbers (integer or float), strings in quotes or apostrophes,
   `+ - * /` (and `+` joins strings), comparisons `< <= = == >= > =~ !~ isin !isin`, `and`/`&&`,
   `or`/`||`, unary minus, parentheses. Cvars are referred to by `$name` (expanded before) or
   `%name`. An empty argument is the empty string `''`.
3. **`if_exists <cvar|alias|trigger|hud> <name> <cmd1> [<cmd2>]`**.
4. **`eval <expr>`** prints the value and its type: `Result: 3 (integer)`.
5. **`set_calc <var> …`** writes into a cvar (creating it): `set_calc v <a> <op> <b>` with `+ - *
   /` (float), `%` and `div` (integers), `and`, `or`, `xor` (bitwise on integers); division by 0
   prints an error and stores 999. Also `set_calc v strlen <text>`, `int <num>`, `substr <var>
   <pos> [len]` (default length 1), `set_substr <var> <pos>` (writes into v), `pos <var> <text>`
   (index or −1), `tobrown`, `towhite` (the coloured or plain font half). `set_eval <var> <expr>`
   stores an expression's value. Both can be refused by a tournament ruleset.
6. **`alias_in`/`alias_out`/`cvar_in`/`cvar_out`**: insert or remove a word in an alias body or a
   cvar value (verify the argument order before implementing).

Fixtures:

```
> if 3 > 2 then echo yes else echo no          prints: yes
> if abc isin xabcx echo in                    prints: in
> if 10 == 10.0 echo eq                        prints: eq   (numeric compare)
> if (1+2 == 3 and "a" < "b") then echo ok     prints: ok
> eval 7/2                                     prints: Result: 3.500000 (double)  (verify)
> set_calc r 7 div 2 ; r                       prints: "r" is "3"
> set_calc r 1 / 0 ; r                         prints the error, then "r" is "999"
> set s hello ; set_calc l strlen $s ; l       prints: "l" is "5"
```

## Binds

1. **`bind <key> [command…]`**: sets the key's command to the rest of the line; `bind key` alone
   prints it; `unbind key`, `unbindall`, `bindlist`.
2. **Key names**: a single printable character names that key; longer names are case-insensitive:
   `TAB ENTER ESCAPE SPACE BACKSPACE CAPSLOCK UPARROW DOWNARROW LEFTARROW RIGHTARROW ALT CTRL
   SHIFT` (and their `L`/`R` forms), `F1`…`F12`, `INS DEL PGDN PGUP HOME END PAUSE`, `MOUSE1`…
   `MOUSE8`, `MWHEELUP MWHEELDOWN`, the keypad keys (`KP_…`), joystick and aux buttons, and the
   semicolon as `SEMICOLON` (it cannot be typed bare). Binding `CTRL`, `ALT`, `SHIFT` or `WIN`
   binds both the left and right keys. `con_bindphysical` (default 0, set to 1 while a config is
   executed) chooses whether a one-character name means the physical key or the character the
   keyboard layout types.
3. **Pressing a key** whose command starts with `+` appends `+cmd <key number>`; releasing it
   appends `-cmd <key number>` (also when the console has opened in between). Any other command
   is appended on press only. A `+` command such as `+attack` remembers up to two keys holding it:
   the action stops only when both are released (a third key is ignored with a warning, verify).
4. **One table**: the modern client has a single table for the game; the console and the menus
   keep their own fixed keys. M18k's "bind tables by mode" is ours to decide; the minimum is the
   one table.
5. **Movement binds** may be protected by a ruleset (a bind that does several things on one key
   refused); not needed for M18k.

Fixtures:

```
> bind space +jump ; bind space                prints: "space" = "+jump"
> bind mouse1 "+attack;wait;-attack"           press: runs the whole line once (no + handling)
> bind CTRL +attack                            both LCTRL and RCTRL are bound
> bind x ; (x unbound)                         prints: "x" is not bound
```

## Team messaging

1. **`say` / `say_team`**: before sending, the text is expanded twice: the percent macros, then
   the fun characters, and the result is wrapped in quotes if it is not already. `cl_parseSay`
   (default 1) and `cl_parseFunChars` (default 1) switch the two passes.
2. **Percent macros** (only in say text): `%a` armour, `%A` armour type, `%b` best weapon and
   ammo, `%c` cells, `%d` where you last died, `%h` health, `%i` what you took and where, `%j`
   where you last pointed, `%k`/`%m` the last item taken or pointed at, `%l` your location, `%L`
   the last location reported, `%p`/`%P` your powerups, `%q` the last powerup seen, `%r` the last
   location reported, `%R` your rune, `%S` your team-fortress skin, `%t` what you point at and
   where, `%u` what you need, `%w` weapon and ammo, `%x` what you point at, `%X` what you took,
   `%y` where what you point at is, `%Y` where you took it; `%[a]`, `%[h]`, `%[p]` are the
   bracketed forms, coloured when low. `%n`/`%N` mark the message for the name filter. A field
   width can be given (`%<width><alignment>…`, verify the syntax).
3. **Fun characters** (say text and `echo`): `$\` CR (it hides the name prefix), `$:` LF, `$[`
   and `$]` the gold brackets, `$G $R $Y $B $W` the coloured LEDs, `$(`, `$=`, `$)` the bar ends,
   `$a`, `$b`, `$c`/`$d`, `$<`, `$-`, `$>`, `$,`, `$.` other glyphs, `$0`…`$9` the gold digits,
   `$$` a `$`, `$^` a `^`, and `$xHH` any byte by hex (00 becomes a space). Outside chat, `^c`
   sets bit 7 of the next character (the coloured half).
4. **Location files**: `locs/<map>.loc` (also the map's group name), one point a line: three
   integers (the position × 8, so 1/8 unit) separated by spaces, then the name to the end of the
   line; `//` comments; names at most 63 characters. Your location is the nearest point by
   straight-line distance; with none loaded, `tp_name_someplace` ("someplace"). A name may hold
   `$loc_name_<x>` placeholders replaced from `loc_name_<x>` cvars. `locations_add`,
   `locations_remove`, `locations_savefile`, `locations_clearall` edit them.
5. **Message triggers**: `msg_trigger <alias> "<text>" [-l <level>]` runs the alias of that name
   when a received print of that level (default high; 3 chat, 4 is matched by 3) contains the
   text; `re_trigger <name> <regex>` (with `re_trigger_options`) matches regular expressions.
   Triggered text runs in the safe buffer, where only a fixed list of commands is allowed (play,
   echo, say, say_team, set, alias, bind, exec, if, wait, toggle, inc, the trigger commands, a few
   more; and some team-fortress impulses at a limited rate). Chat lines containing the proxy's
   `f_…` query words never fire a trigger. `tp_msgtriggers` (default 1) switches them; a ruleset
   or spectating can turn them off.
6. **Event triggers**: aliases named `f_newmap`, `f_spawn`, `f_mapend`, `f_death`, `f_respawn`,
   `f_took`, `f_cfgload`, `f_demostart`, `f_demoend`, `f_exit`, `f_weaponchange`, … run on that
   event (the teamplay ones only with a teammate present, unless `tp_forceTriggers`).
   `tp_triggers` (default 1) switches them.
7. **Canned messages**: `tp_msgreport`, `tp_msgcoming`, `tp_msglost`, `tp_msgpoint`, `tp_msgtook`,
   `tp_msgneed`, … send a prepared `say_team` built from the macros.

Fixtures:

```
> say_team "health %h"         (health 87)            sends: say_team "health 87"
> say "$[go$]"                                         sends the bracket glyphs 0x10 go 0x11
> echo $x41$x42                                        prints: AB
  locs/dm2.loc:  "-1200 400 96 quad room"              location (-150, 50, 12) is "quad room"
> alias got "echo saw it" ; msg_trigger got "joined"   a print "x joined the game" prints: saw it
```

## Saving

1. **`cfg_save [name]`** writes `config.cfg` (or the name) in this order, each part switchable:
   a preamble; the cvars that differ from their default (all of them with
   `cfg_save_unchanged 1`), grouped, as `name "value"`, then the user-created ones as
   `set name "value"`; the server-browser sources; the aliases, after `unaliasall`, as
   `alias name "value"` (server and temporary aliases are not saved); the HUD; the teamplay
   commands (team and enemy colours and skins, the filters); miscellaneous state (`allskins`,
   flood protection, a few `setinfo` keys); the held `+` commands as `+name` or `-name`; and the
   key bindings, after `unbindall`, as `bind key "command"`. Switches: `cfg_save_cvars`,
   `cfg_save_aliases`, `cfg_save_cmds`, `cfg_save_binds` (all default 1), `cfg_save_userinfo`
   (default 2), `cfg_save_onquit` (default 0), `cfg_backup` (default 0).
2. **`cfg_load [name]`** resets first: cvars to their defaults (keeping some user-info ones),
   user cvars and user aliases deleted, binds and `+` states reset, teamplay settings reset; then
   (when `cfg_legacy_exec` allows) `default.cfg` (unless it is the game's original one) and
   `autoexec.cfg`; then the named config. `cfg_reset` resets the same way without a config.
3. **Startup order**: the defaults, `default.cfg`, the saved `config.cfg`, `autoexec.cfg`, then
   the command line's commands. Config files live in the client's own config directory, or the
   game directory (`cfg_use_gamedir`), or the home directory (`cfg_use_home`).
4. **Per mod**: a game directory's own `autoexec.cfg` is found first on the search path when that
   mod is active; there is no other per-mod mechanism.

Fixture: `bind q "say hi"`, `alias x "echo 1"`, `fov 100`, then `cfg_save t` writes, in order, a
line `fov "100"`, `unaliasall`, `alias x "echo 1"`, …, `unbindall`, `bind q "say hi"`.

## What a server may stuff

1. **Where it runs**: a stufftext's lines go to the server's buffer and run only when complete.
2. **First**, a handful of commands meant only for servers run directly: `changing`,
   `fullserverinfo`, `nextul`, `stopul`, `r_drawviewmodel`, `fileul`.
3. **Then the allow list**: a command or cvar from the server runs only if its name is in
   `cl_remote_capabilities`, a comma-separated list the player controls (it cannot be changed
   while connected, and the server cannot change it). The default: `+attack`, `-attack`, `alias`,
   `bf`, `changing`, `cmd`, `color`, `download`, `exec`, `fullserverinfo`, `impulse`, `infoset`,
   the mod's `ktx_infoset` / `ktx_sinfoset` / `sinfoset`, `nextul`, the `on_…` event aliases'
   names (`on_connect`, `on_enter`, `on_matchstart`, `on_matchend`, `on_observe`,
   `on_spec_enter`, `on_admin`, `on_unadmin` and their `_ctf` / `_ffa` forms), `packet`, `play`,
   `rate`, `reconnect`, `say`, `skin`, `skins`, `team`, `tempalias`, `track`, `wait`. Anything else
   prints "Blocked <name>: not in cl_remote_capabilities".
4. **Aliases** the server defines run in the server's buffer under the same list; the player's
   own aliases called by a server line run in the main buffer.
5. **Guarded cvars**: a cvar marked "not from server" is refused even by `set` from the server
   ("Server tried setting …"); every `cl_remote_capabilities`, `cfg_…`, `cl_allow_downloads`,
   `demo_dir` and similar cvar carries that mark. `set` from the server may not change a cvar the
   player created; it may create its own.
6. **`exec`** from the server loads into the main buffer but bracketed by `con_bindphysical` so
   binds in it behave as in a config; files with downloadable extensions are refused (§ The
   command buffer, 8).
7. **Comment lines the client reads**: a stuffed `//vwep …` names the view-weapon models
   (`protocol.md` § 5); `//at <player>` asks a spectator to track that player. Other `//` lines
   are ignored.

Fixtures (lines as a server would stuff them):

```
stuff: "say hello\n"                 sent to the server: say "hello"
stuff: "bind x quit\n"               printed: Blocked bind: not in cl_remote_capabilities
stuff: "cl_remote_capabilities x\n"  printed: Blocked cl_remote_capabilities: …  (the cvar is not in the list)
stuff: "alias on_enter \"echo hi\"\n" then "on_enter\n"     prints: hi
stuff: "echo a"   (no newline)       nothing runs until "\n" arrives
```

## What is ours, not theirs

1. One buffer per source is enough for us if each keeps its own rules (main, safe, formatted,
   server); the order of running them each frame must match § The command buffer, 1.
2. `seta`, `dec` and `cycle` are our additions (§ Cvars, 9), so configs written for other clients
   keep working.
3. Our engine's console keeps the tiers of `docs/console.md`; the "not from server" mark is one of
   them.
