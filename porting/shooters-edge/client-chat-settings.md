# Client chat settings: the console line, chat sounds, notify lines, timestamps, highlighting (round 24)

Read: ezQuake release 3.6.9 (commit b2d448f, March 2026), its source and its own player help
(the variable descriptions it ships). Q2PRO could not be read in this round: the session was refused
access to the Q2PRO repository, so questions 6 to 10 are open. Each fact is marked [read] (seen in
the source or the project's player help), [derived] (worked out from what was read) or [measured]
(none this round). Times in seconds, sizes in pixels at scale 1 unless said otherwise.

## ezQuake 3.6.9

### 1. `cl_chatmode`

- Default 2. The menu calls the values "All Commands" (0), "All Chat" (1) and "First Word" (2).
  [read]
- What counts as "a command" for modes 0 and 2: the first word of the typed line (everything up to
  the first space or control character) names a console variable, a console command (this includes
  the commands the client only forwards to the server), an alias, or an old renamed name the client
  still accepts (such as `cl_chatsound`). The match ignores letter case. Nothing else counts: an
  unknown word is chat in mode 2. [read]
- The checks on Enter, in this order: [read]
  1. A line whose first two characters are `//` is not executed or sent at all; it is only echoed
     into the console.
  2. Ctrl+Enter (when connected) sends the whole line as team chat; Shift+Enter sends it as normal
     chat. This happens in every mode, even if the line is a command. An empty line sends nothing.
  3. A line that starts with `/` or `\` is executed as a console command with that one character
     removed, in every mode.
  4. While disconnected, every line is executed as a command, in every mode.
  5. Mode 0 or 2, first word is a command: the line is executed as typed.
  6. Otherwise, when connected: in modes 1 and 2 the line is sent as `say "<line>"` (always plain
     `say`, never `say_team`); in mode 0 the line is executed as a command line anyway.
- Quoting: the text is wrapped in one pair of double quotes. No character inside is escaped, so a
  double quote typed in the line ends the quoted argument early on the way to the server. [read]
  [derived: the effect on the server side]
- Characters beyond the 8-bit range are sent in an encoded form chosen by `cl_textencoding`
  (default 0); plain 8-bit text is sent unchanged. [read]
- Messages: in mode 0 a line that is not a command, when connected, is executed as one; an unknown
  first word prints `Unknown command "<word>"` (only while `cl_warncmd` is 1, its default). No
  message is printed when a line goes out as chat. [read]
- Mode 1: everything is chat unless it starts with `/` or `\` (or the player is disconnected).
  [read]
- `con_hide_chat_input` (default 1): when a line went out as chat by modes 1 or 2, the typed line is
  not echoed into the console (the server's chat line comes back on its own). It has no effect on
  Ctrl/Shift+Enter chat or on mode 0. [read]

### 2. Chat sounds

- Settings, defaults: [read]
  - `s_chat_custom` default 1 (old name `cl_chatsound` still accepted). 0: no chat sound at all.
   1: sounds for every kind. 2: a sound only for a team line (see below); every other chat line is
   silent.
  - `s_mm1_file`, `s_mm2_file`, `s_spec_file`, `s_otherchat_file`, each default `misc/talk.wav`.
   The path is relative to the game's `sound/` folder. [read: defaults; derived: the folder]
  - `s_mm1_volume`, `s_mm2_volume`, `s_spec_volume`, `s_otherchat_volume`, each default 1.
   Old names `con_sound_mm1_file` ... `con_sound_other_volume` are still accepted.
- Volume: clamped to 0 to 1 and applied on top of the master `volume`; 0 or less plays nothing.
  The sound is played as a local sound on the player's own entity (full volume, not placed in the
  world). [read]
- Only lines the server sends at chat level are considered. [read]
- How a line's kind is recognised (by comparing the start of the line against every player's name,
  cut to 31 characters): [read]
  - `<name>: ` → normal say (mm1),
  - `(<name>): ` → team say (mm2),
  - `[SPEC] <name>: ` → spectator team say,
  - `console: ` → treated as normal say.
  - If no name matches, or two players' names both match, the line is "other".
- Which file plays: [read]
  - Sender recognised and the sender is a spectator → the spec file and spec volume.
  - Otherwise mm1 → mm1 file/volume, mm2 → mm2 file/volume, spectator team say → spec.
  - Not recognised → the other file and other volume.
  - Restriction: the custom FILE is honoured only when the local player is a spectator or watching
   a demo. A playing player always hears `misc/talk.wav` for a recognised sender, at that kind's
   custom volume (players may still change volumes, not files). Unrecognised lines use the other
   file even for players. [read]
- `s_chat_custom 2` keys on the team classification used by the teamplay code: a line counts as team
  only when it has the `(<name>): ` form AND the sender is the local player or (with teamplay on)
  on the local player's team (a spectator uses the tracked player's team, and gets no team lines
  when tracking nobody). [read]
- Lines that play no sound and are not shown at all: lines suppressed by the teamplay message
  filters, lines from ignored players or flood-ignored lines, spectator lines in the bracket form
  when `ignore_qizmo_spec` is 1 (default 0), team lines containing `#inlay#`, and team lines that
  fail the player's `filter` list. [read]
- `msg_filter` (default 0; 1 hides mm1 and spectator lines, 2 hides team lines, 3 hides all three)
  hides a recognised line entirely. Edge case: it is only applied when the chat sound would have
  played, so with `s_chat_custom 0`, or for a line whose sound was cut by a sound trigger, the
  line is shown anyway. [read]
- Sound triggers in team lines: a team line containing the trigger character (`tp_soundtrigger`,
  default `~`) followed by a file name before the line end plays that file (`.wav` added if missing,
  only if it exists, no `..` allowed, not when the ruleset restricts triggers), removes the trigger
  text from the shown line, and replaces the chat sound. [read]
- The player's own lines are not skipped: the server's echo of the player's own say is classified
  like any other and plays its sound. [read]

### 3. `con_notify`, `con_notifylines`, `con_notifytime`

- `con_notify` default 1; 0 hides the notify area AND the chat input line drawn under it ("say:",
  "say_team:", "say_game:" followed by the typed text and a blinking cursor), unless the HUD's
  notify element is on. [read: the input line is drawn by the same routine; derived: hidden with it]
- `con_notifylines` default 4, used range 0 to 15 (values outside are clamped). 0 shows no notify
  lines but still the chat input line. A console line longer than the notify width wraps onto
  further screen rows, which do not count against the limit. [read]
- `con_notifytime` default 3, used range 0 to 10 seconds. Its value is cut to a whole number of
  seconds before use, so 0.5 behaves as 0 (lines vanish at once) and 3.7 as 3. The menu offers 0.5
  to 16 in steps of 0.5 even so. [read]
- A line's age starts when it is printed; a line older than the limit disappears at once, with no
  fade. Only the newest 16 lines carry a time. [read]
- Placement: 8 pixels from the left, top at the console shift (`con_shift` clamped to 0 to 8, so 0
  with its default of -10), rows 8 pixels high, as wide as the console. [read]
- Chat and other lines are treated alike, except: [read]
  - `con_mm2_only` (default 0): when 1, during play (not before the match and not during the
   countdown) only team lines enter the notify area; other lines still go to the console.
  - `cl_showFragsMessages 2` keeps frag messages out of the notify area.
- `con_clearnotify` (default 1): the notify area is emptied each time the console is toggled.
  It is also emptied on map change, disconnect and some menu and demo transitions. [read]
- A second, movable copy exists as a HUD element, `hud_notify_*`: off by default, top-left of the
  screen, 4 rows, 30 columns (0 means the console's width), scale 1, time 4 seconds (also cut to a
  whole number), proportional font off. [read]

### 4. Timestamps on console lines

- `con_timestamps`, default 0; the menu shows it as an on/off item "Timestamps". [read]
- 1: the local time as hours and minutes, the hour padded with a space to two characters, then a
  space, e.g. ` 9:05 `. 2: hours, minutes and seconds (` 9:05:42 `), but only while the player is a
  spectator, watching a demo, or the match has not started (standby); while playing, mode 2 prints
  hours and minutes like 1. No brackets are printed, although the player help describes the format
  as `[HH:mm]` and `[HH:mm:ss]`. [read]
- Which lines: every non-empty line the server sends at chat level (normal, team, spectator and
  unrecognised chat alike) that survived the filters above. The player help says "mm1 or spectator
  messages" and excludes faked names; the source does not limit it so. Other console lines get no
  stamp. [read]
- The stamp is printed into the console just before the chat line, so it goes wherever console text
  goes: the notify area and the console log when logging is on. [derived]

### 5. Highlighting lines with the player's name

- `con_highlight`, default 0. `con_highlight_mark`, default empty. [read]
- A line is highlighted when the player's current `name` occurs in it (exact, case-sensitive
  match) at character position 2 or later; a match at the very start is taken to be the player's
  own line and is not highlighted. This is checked on every printed server line, not only chat.
  [read]
- What changes: chat lines are normally shown in the game's coloured (bronze) alphabet, with text
  between `{` and `}` shown white (`cl_parseWhiteText`, default 1; 2 for team lines only).
  - 1: the highlighted line is shown all white (the colouring is skipped).
  - 2: the line keeps its colouring and starts with the `con_highlight_mark` text.
  - 3, and any other non-zero value: all white AND starting with the mark. [read]
- `con_shift` (default -10) is not about highlighting: it shifts the console's background picture
  vertically by its value while in game, and shifts the console text and notify area down by its
  value clamped to 0 to 8 pixels. [read]

## Q2PRO (master)

6. `cl_chat_notify` and `cl_chat_sound`: open. The Q2PRO repository was refused to this session,
   so nothing was read.
7. `cl_chat_filter`: open, for the same reason.
8. `scr_chathud` and the other `scr_chathud_*` settings: open, for the same reason.
9. `con_timestamps` and its format setting: open, for the same reason.
10. Other chat and notify-area settings: open, for the same reason.
