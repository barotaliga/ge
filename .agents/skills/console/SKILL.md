---
name: console
description: "Use for settings, cvars, cfg files and the console in gameengine2 or a game on it: engine/console (cvars and their tiers, the console language, aliases, scripting, binds, cfg_save and the startup order, the drop-down console's model), a game's cvars (GameSpec::cvars, Fall Line's cfg/cvars.hpp), naming a new setting, ge cvar and ge cfg, and the `console` lab. Load it before adding a setting, a console command or a bind, or when a cfg does not do what it says."
---

# The console

Every setting a person may change without a rebuild is a CVAR, and every cvar is set from text:
a cfg file, the drop-down console, the command line's `+set`, or the server. The design and the
whole language are [`docs/console.md`](../../../docs/console.md); the roadmap is `docs/goals.md`
§ M21. `engine/console` is `ge::console` (target `ge_console`, no platform, no clock).

## Adding a setting

1. **Decide its tier first** (docs/console.md § Tiers). Does a player choose it (the picture, the
   sound, the camera, the controls)? `kArchive`. Does the server decide it for everyone (the
   rules, the map, the tick rate)? `kServer | kServerInfo`. Does it only make sense while
   developing (fake lag, a debug draw)? `kCheat`. Is it read once when something is made (the
   window, a pipeline)? add `kLatch`. Anything else stays a constant in code: not every number
   is a setting, and a setting nobody will tune is noise in `cvarlist`.
2. **Name it** by its owner's prefix (docs/console.md § Names): `sv_` `cl_` `r_` `s_` `vid_`
   `in_` `m_` `net_` `con_` `hud_`, then the thing in one short word, abbreviated when long
   (`r_shadowsize`, not `r_shadow_map_size_texels`). `ge cvar ls <prefix>` shows the neighbours;
   match their style.
3. **Bind it to the struct the subsystem already reads** (`cvars.bind(spec, &settings.field)`),
   so the subsystem never includes the console and reads its field each frame for free. Give the
   range (`min`, `max`, the kind) and a `help` phrase with the unit: `help <name>` and
   `ge cfg dump` print it, and that is how a player meets it.
4. **Register it where the game registers its cvars** (Fall Line: `games/fall_line/cfg/cvars.cpp`),
   which is both the app's and the play executable's, so `ge cvar` sees it. Engine subsystems
   offer an `add_<subsystem>_cvars(Cvars&, Settings&)` the game calls.
5. Test it the way `tests/fall_line_cvars_test.cpp` does: a cfg line reaches the field (after
   `apply_latched` when latched), and a client refuses a server's cvar.

## Changing the language

The fixtures in `tests/golden/console_script.txt` ARE the language's specification by example.
A new command or a changed behaviour adds or moves a fixture: write the cfg, run
`GE_GOLDEN_WRITE=1 build/debug/tests/console_script_test`, read every moved line in the diff,
and say in the PR why each is right. `docs/console.md` lists every built-in command; keep it in
step.

Commands live by subject: variables, aliases, files (`console_builtins.cpp`), scripting
(`script.cpp`), binds (`console_binds.cpp`), the drop-down console's own (`view.cpp`). A command
whose words must not be expanded when it runs (it stores a body to run later) is `kNoExpand`;
a command a server may stuff into a client is `kStuffable`, and nothing that touches the
player's files, binds or archived cvars is.

## Checking a cfg

`ge cfg check <file>` runs a cfg in a fresh console of the game and lists every unknown command,
refused set and name nothing reads as findings at file:line (with `--server` for a server's
cfg). `ge cfg exec <file> --cmd="..."` says what it changed. `ge cfg dump --out=my.cfg` writes
every player setting, commented, to start from.

## Looking at the console

`make lab LAB=console` pictures the drop-down console (down, sliding, closed with the notify
lines) from the same `ConsoleView` model the app draws. Change the view, run it with
`--tag=before` first and `--compare=before` after, and look at the three pictures.
