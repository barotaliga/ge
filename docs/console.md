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
| `sv_` | the server's rules, decided for everyone | `sv_seed`, `sv_hour`, `sv_maxclients`, `sv_cheats` |
| `cl_` | the client: the camera, prediction, what it asks of the server | `cl_view`, `cl_chaseback`, `cl_updaterate` |
| `r_` | the picture | `r_bloom`, `r_taa`, `r_scale`, `r_shadowsize` |
| `s_` | the sound | `s_wind`, `s_effects`, `s_mute` |
| `vid_` | the window and the display | `vid_width`, `vid_height`, `vid_vsync` |
| `in_` | input devices | `in_joystick` |
| `m_` | the mouse | `m_pitch`, `m_yaw` |
| `net_` | the network's plumbing, mostly for developers | `net_fakeloss`, `net_fakelag` |
| `con_` | the drop-down console | `con_height`, `con_speed`, `con_chat` |
| `hud_` | the HUD | `hud_scale` |
| none | a few names everyone knows | `name`, `rate`, `sensitivity`, `volume` |

`ge cvar ls <prefix>` lists a game's cvars with a prefix; a new name matches its neighbours'
style.

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

A server's stuffed text may run only the commands marked as safe to stuff (`echo`, `set`,
`wait`, `say`, `say_team`, `say_prox`) and set only the server's cvars. It never touches a player's files,
binds or archived cvars.

`cvarlist` prints the flags as letters: `A` archive, `U` user info, `S` server info, `V` server,
`C` cheat, `L` latch, `R` read only, `*` created by a cfg (no code reads it).

## Startup

A game reads, in order:

1. `default.cfg`: the game's shipped defaults and binds.
2. `config.cfg`: what `cfg_save` last wrote.
3. `autoexec.cfg`: the player's own file, which the game never writes.
4. The command line's `+commands`: `+set r_bloom 0 +exec race.cfg`.

A missing file is skipped. A dedicated server reads `server.cfg` in place of the first three. A
latched value set during startup is applied once it is over.

## The language

- **Commands** are words split by blanks; `"..."` groups words with blanks into one; `//`
  starts a comment; `;` ends a command (outside quotes). A line is at most 4096 characters.
- **Lookup.** The first word is looked up as a command, then an alias, then a cvar. A cvar's
  name alone prints it; with a value it sets it (`r_bloom 0.1`).
- **Expansion.** `$name` and `${name}` become a macro's or a cvar's value (a macro wins); `$$`
  is a `$`. An alias's body gets its arguments as `$1`…`$9` and `$*`.
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
- **Modifiers** come before the key: `ctrl+s`, `alt+shift+f1` (kept in the order ctrl, alt,
  shift). A key held with modifiers runs its modified bind if it has one, else its own.
- **Modes**: `game` (the default), `console`, `menu` and `demo` each keep their own table.
- **Buttons.** A bind that starts with `+` runs that command when the key goes down and the same
  command with `-` when it comes up: `bind space +jump`, or `bind v +voice_prox` to talk to the
  players near you while the key is held. Any other bind runs on the press. A game registers
  its held actions as buttons (`Console::add_button`).

The backquote key (under the tilde) is bound to `toggleconsole` in the game and console modes
by default.

## Built-in commands

### Variables

| Command | What it does |
| --- | --- |
| `set <name> <value>` | Sets a cvar. A name no code reads is created (marked `*`). |
| `seta <name> <value>` | The same, and archives it, so `cfg_save` writes it. |
| `reset <name>` | Puts a cvar back to its default. |
| `cvarlist [<prefix>]` | Every cvar, its value, flags and help. |
| `setinfo [<key> <value>]` | Shows or sets a key of this player's user info. |
| `serverinfo [<key> <value>]` | Shows or sets a key of the server's info. |
| `help <name>` | What a cvar, command or alias is: its value, default, range and help. |

### Aliases and files

| Command | What it does |
| --- | --- |
| `alias <name> "<commands>"` | A new command that runs the commands. With no body, prints it. |
| `tempalias <name> "<commands>"` | The same, but `cfg_save` does not write it. |
| `unalias <name>` / `unaliasall` | Removes one alias, or every alias. |
| `aliaslist` | Every alias and its body. |
| `exec <file>` | Runs a cfg file's commands (`.cfg` is added when there is no extension). |
| `cfg_save [<file>]` | Writes every bind, alias and archived cvar (default `config.cfg`). |
| `cfg_load [<file>]` | Runs a saved config (default `config.cfg`). |
| `cmdlist [<prefix>]` | Every command and what it does. |
| `echo <words>` | Prints the words with one blank between each. |
| `wait` | Leaves the rest of the buffer for the next frame. |

### Scripting

| Command | What it does |
| --- | --- |
| `if <a> [<op> <b>] then <commands> [else <commands>]` | Runs one branch. The ops are `==` `!=` `<` `<=` `>` `>=` `isin` `!isin`. With no op, `a` is tested as true (not empty, not 0). |
| `set_calc <name> <a> <op> <b>` | A calculation into a cvar. The ops are `+` `-` `*` `/` `%` and the comparisons; also `set_calc <name> strlen <s>`, `substr <s> <from> <count>`, `strstr <s> <find>` and `int <a>`. |
| `inc <name> [<by>]` / `dec <name> [<by>]` | Adds to or takes from a cvar (by 1). |
| `toggle <name>` | Flips a cvar between 0 and 1. |
| `cycle <name> <v1> <v2> ...` | Sets a cvar to the value after its own in the list. |
| `msg_trigger <alias> <match>` | A chat, server or event line holding `match` runs the alias. |
| `msg_trigger_remove <alias>` | Stops a trigger. |

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
| `say <text>` | A chat line to everyone. |
| `say_team <text>` | A chat line to your team. |
| `say_prox <text>` | A chat line to the players near you (the game decides how near). |
| `whisper <nick> <text>` | A chat line to one player. |

## The drop-down console

The backquote key (under the tilde) slides the console down from the top of the screen over the
game. It holds everything that happened, each line coloured by its kind: the console's own
output, the commands typed (after `]`), chat to everyone, to the team, to the players nearby and whispers, the server's
messages and the game's events. Closed, the newest few lines show at the top of the screen for
a few seconds (the notify lines).

The input line edits in place (left, right, home, end, backspace, delete). Tab completes a
command, alias or cvar name, and a second Tab lists every match. Up and Down walk the history;
Page Up and Page Down scroll the lines. Enter submits it:

- a line starting with `/` is a command, with the `/` dropped: `/r_bloom 0`, `/say hi`;
- any other line is a chat message to everyone while `con_chat` is 1 (a multiplayer game sets
  it), and a command while it is 0 (single player has nobody to chat to).

Its cvars:

| Cvar | Default | What it is |
| --- | --- | --- |
| `con_height` | 0.5 | the share of the screen it covers when down |
| `con_speed` | 3 | how fast it slides, screens a second (0: at once) |
| `con_lines` | 1024 | the lines of scrollback kept |
| `con_notifylines` | 4 | the newest lines shown at the top while it is closed (0: none) |
| `con_notifytime` | 4 | how long a notify line stays, s |
| `con_chat` | 0 | 1: a line typed without a `/` is said to everyone |

The view (`engine/console/view.hpp`) is a model that holds no pixels: the app's 2D layer draws
it, and the `console` lab draws it on the CPU (`make lab LAB=console`).

## Tooling

```sh
ge cvar ls [<prefix>]            # every cvar of the game: value, default, kind, range, flags, help
ge cvar get <name>...            # the named ones (an unknown name: the nearest known)
ge cfg check <file>... [--server]   # every problem at file:line, and each name nothing reads
ge cfg exec <file>... [--cmd=...]   # the same, then what changed and what it printed
ge cfg dump [--out=my.cfg]       # every player setting, commented: a cfg to start from
```

`--server` checks a cfg as a dedicated server's, where the server's cvars may be set.
