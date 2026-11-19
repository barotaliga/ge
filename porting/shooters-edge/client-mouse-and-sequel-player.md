# The modern clients' mouse and the sequel's player settings (clean-room note)

WORKING AREA: deleted when porting ends. A reader's facts on how the first game's modern client
turns mouse movement into view angles (`m_filter`, the `m_accel` family, `in_raw`), and on the
sequel's player settings (`hand`, `gender`, `gender_auto`, `cl_vwep`, `cl_noskins`,
`cl_predict`, `m_autosens`), the sequel's modern client's chat filters (`ignoretext`,
`ignorenick`) and its spellings of a few console commands. This closes question 9 of
`chat-ignore.md`.

Read: ezQuake 3.6.9 (release tag) with its shipped settings help; Q2PRO at commit 601a8df
(December 2025, the q2pro/q2pro mirror, since the main repository was unreachable) with its
manual; the stock sequel's 3.21 source release (client, renderers, game library).

Tags: [read] seen in the source or its shipped manual; [derived] follows from what was read,
[measured] run. Nothing here was measured. No code is quoted.

## The first game's modern client: from mouse delta to angles

Order each rendered frame [read]: take the frame's delta, apply `m_filter`, apply sensitivity
(with acceleration when on), then turn by `m_yaw` and pitch by `m_pitch` (or move by `m_side` /
`m_forward` when strafing or not mouse-looking). Pitch is clamped to the server's limits after.

Defaults [read]: `sensitivity` 12, `m_yaw` 0.022, `m_pitch` 0.022, `m_side` 0.8, `m_forward` 1,
`m_filter` 0, `m_accel` 0, `m_accel_power` 2, `m_accel_offset` 0, `m_accel_senscap` 0, `in_raw`
1, `in_grab_windowed_mouse` 1, `in_release_mouse_modes` 2, `in_ignore_touch_events` 1,
`scr_cursor_sensitivity` 1.

### `m_filter`
- Weight w = the setting clamped to 0..1, halved. Used delta = this delta × (1 − w) + previous
  delta × w, per axis. [read]
- "Previous" is last frame's unfiltered delta, so 1 is a 50/50 two-frame average and nothing
  older is remembered. [read]
- While a menu, the HUD editor or demo controls hold the mouse, the remembered delta becomes that
  frame's delta × `scr_cursor_sensitivity`. [read]
- Works the same on raw and non-raw input. [read]
- The built-in troubleshooter calls it a source of input delay and advises 0. [read]

### Acceleration (`m_accel` above 0)
- Speed = length of this frame's unfiltered delta ÷ the frame's length in milliseconds (counts per
  ms). The frame length is the rendered-frame interval less 1 ms, floored at the cap's minimum
  frame time. [read]
- s = speed − `m_accel_offset` (offset may be negative). [read]
- If s is above 0 and `m_accel_power` above 1: effective sensitivity = `sensitivity` + (s ×
  `m_accel`) to the power (`m_accel_power` − 1). Power 2 is linear in speed. [read]
- If s is above 0 and power is 1 or below: effective sensitivity = 1 exactly (`sensitivity`
  discarded). [read]
- If s is 0 or below: effective sensitivity = `sensitivity`. [read]
- Then, if `m_accel_senscap` is above 0, the effective sensitivity is limited to it, in every
  case (so a cap below `sensitivity` slows slow movement too). [read]
- No setting is clamped; the menu offers `m_accel` 0 to 1 in 0.1 steps. [read]

### `in_raw`
- Integer; above 0 is on. Changing it restarts mouse input. [read]
- On: relative mouse mode with the window grabbed (macOS: a separate game-controller mouse
  reader; if it cannot start, a warning and `in_raw` falls to 0). [read]
- Off: each frame the cursor position's offset from the window centre is the delta and the cursor
  is warped back to the centre; desktop pointer settings then apply. [read; desktop settings
  derived]
- Startup prints "RAW mouse input initialized" or "SDL mouse input initialized". [read]
- `in_release_mouse_modes` is a bit sum of the modes in which a windowed client releases the
  cursor: 1 demo playback, 2 console, 4 message typing, 8 menu, 16 HUD editor. [read]

## The sequel: player settings

### `hand`
- Default 0 (0 right, 1 left, 2 centre), user info, saved. Stock menu resets values outside 0..2
  to 0. [read]
- Game: a shot's start offset (per weapon, about 8 to 24 forward, 7 or 8 right, view height − 8
  up) has its right part negated for 1 and zeroed for 2; other values act as right. [read]
- Game: with 2, or a field of view above 91, the help-icon slot of the status bar shows the held
  weapon's icon. The game never hides the gun. [read]
- Stock client: 1 draws the view weapon mirrored, 2 does not draw it; no view weapon at all above
  a 90 degree field of view. Gun-start beams are shifted +1, −1 or 0 times their side offset.
  [read]
- Q2PRO: `cl_gun` default 1 (0 hidden, 1 by `hand`, 2 forced right, 3 forced left) decides the
  view weapon and the beam side; the over-90 hiding applies only to old-format demos. When the gun
  is hidden (`cl_gun` 0, or `hand` 2 with `cl_gun` 1) on its own or R1Q2's protocol, it asks the
  server to stop sending the gun, except while recording. [read]

### `gender`, `gender_auto`
- `gender` default male (user info, saved); `gender_auto` default 1 (saved). [read]
- The pick: the model folder of `skin` (text before the first slash, any case): male or cyborg
  gives male, female or crackhor gives female, else none. [read]
- Stock runs the pick whenever changed user info is about to be sent, unless `gender` was set
  since the last check. [read]
- Q2PRO runs the pick only when `skin` is set from the command line or by the program, never when
  typed, set in the menu or stuffed; so a typed skin leaves `gender` alone. [read; consequence
  derived]
- Effect: only the wording of self-kill death messages (her, his, its) by the first letter (f/F
  female, m/M male, else neuter). Sounds follow the model folder, not `gender`. [read]

### `cl_vwep`
- Default 1, saved. On: others carry the model of the weapon they hold; off: the generic weapon of
  their player model. [read]
- Needs the game to list `#`-prefixed weapon models and to mark the player's linked model as a
  custom weapon with the weapon number in the skin number's upper bits; the stock game lists ten
  weapons plus the grenades. Limits 20 models (stock), 256 (Q2PRO). Q2PRO reloads at once on
  change; stock only on the next info reload. [read; stock timing derived]

### `cl_noskins`
- Default 0, not saved. Stock: any non-zero, everyone is male/grunt. Q2PRO: 1 everyone male/grunt,
  2 female models become female/athena and all others male/grunt; applied at once. Missing files
  fall back to male/grunt, or female/athena for a female model. [read]

### `cl_predict`
- Default 1, not saved. 1 predicts the player's own move over unacknowledged commands; 0 draws the
  server's interpolated position. Off whenever the server flags "no prediction". [read]
- A miss is faded out across the frame; misses of at most 1/8 unit or above 80 units are dropped.
  [read]
- Steps of about 8 to 20 units on the ground are eased over 100 ms (Q2PRO stacks them, capped at
  32 units). [read]
- Q2PRO: no prediction in demo playback; on its own protocol the setting is sent to the server,
  which then omits prediction-only fields. Command history 128. [read]

### `m_autosens` (Q2PRO)
- Default 0, not saved. Base field = the value when above 90 and at most 179, else 90 for any
  non-zero value. [read]
- Horizontal factor = current field ÷ base; vertical factor = the 4:3 vertical field of the
  current view ÷ the 4:3 vertical field of the base. The current field is the player's own,
  before widescreen adjustment. [read]
- Q2PRO's mouse order: `m_filter` (any non-zero: average of this and last delta), sensitivity with
  acceleration (`sensitivity` + delta length in counts × `m_accel`, `m_accel` clamped 0..1, no
  time division), `m_autosens`, then `m_yaw` / `m_pitch`. Q2PRO defaults: `sensitivity` 3,
  `m_side` 1. No mouse in menu or console. [read]

## The sequel's modern client: chat filters

- `ignoretext <words>` adds one pattern (words joined by single spaces); `unignoretext <words>`
  removes exactly that pattern; `unignoretext all` removes all. `ignorenick <nick>` (first word,
  name completion) and `unignorenick <nick>` / `all` (a literal nick "all" needs any second word).
  Each command with no argument lists its own list. [read]
- Only chat-level prints are checked (team chat included); other prints pass. [read; team chat
  derived from the stock game's levels]
- The line is cleaned first: top bit cleared, non-printables (newline too) removed. [read]
- Text patterns match the WHOLE line, case-sensitive; `*` any run, `?` one character, backslash
  escapes. Text filters run before nick filters; the first hit drops the line silently and counts
  a hit. [read]
- Nick filters: exact and case-sensitive at the line's start, followed by ": ", or inside "(",
  "): "; a leading "[...] " tag is skipped once. [read]
- Minimum lengths 3 (text) and 1 (nick); no count limit; duplicates silently ignored; memory only,
  never saved; applied in demo playback too. [read]
- Messages: "Current ignore filters:" then each pattern quoted with "(n hits)" or "(1 hit)"; "No
  ignore filters."; "Match string "x" is too short."; "Can't find ignore filter "x""; "Removed n
  ignore filters." (singular for 1). [read]

## The sequel's modern client: command spellings

- Reset: `reset <cvar>`, `resetall`. No `cvar_reset`, no `unset`. [read]
- One alias: `alias <name>` prints it or says it is undefined; `alias` lists all; `unalias <name>`
  or `unalias -a`. No `viewalias`. [read]
- Macros: `macrolist`. [read]
- `toggle <cvar>` flips 0 and 1 (else "can't toggle"); with values it cycles through them,
  case-insensitive, wrapping; an unlisted current value prints "can't cycle". [read]
- Also `inc`, `dec`, `set` (optional `u` or `s` flag), `setu`, `sets`, `seta`, `cvarlist` with a
  wildcard. Wildcards only (`*`, `?`); no regular expressions. [read]
