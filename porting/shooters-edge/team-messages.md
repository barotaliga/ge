# Team messages: fun characters, percent codes, event triggers, canned messages (clean-room facts)

Read: ezQuake (ezquake-source), the default branch as of 2026-11-14 (version string 3.7.0-dev),
by a clean-room reader that wrote these facts in its own words with no code (M18k). The glyph
descriptions were measured on the shareware release 1.06's character sheet (`conchars` in
`gfx.wad` in its `pak0.pak`). Every fact is marked **read**, **measured** or **derived**; derived
facts are to be verified by a lab before code depends on them. This note extends `configs.md`
§ Team messaging and does not repeat its lists. A byte written `0xNN` is also an index into the
16x16 character sheet (row = high nibble); "the coloured half" is 0x80..0xFF.

## 1. Fun characters

### 1.1 The sequences and the bytes they become (read)

| Sequence | Byte | Glyph at that index in the stock sheet (measured) |
| --- | --- | --- |
| `$\` | 0x0D | a right-pointing arrowhead in the sheet's warm tones (palette 0x60..0x77); identical to 0x8D. In chat it is used as a carriage return, see 1.4 |
| `$:` | 0x0A | an empty cell; it is the newline byte |
| `$[` | 0x10 | an opening square bracket in the warm tones (palette 0x60..0x6B) |
| `$]` | 0x11 | the matching closing bracket |
| `$G` | 0x86 | the modern client calls it the green light |
| `$R` | 0x87 | the red light |
| `$Y` | 0x88 | the yellow light |
| `$B` | 0x89 | the blue light |
| `$W` | 0x84 | the white light |
| `$(` | 0x80 | left end of the slider track (palette 0xA9..0xAF with a dark rim) |
| `$=` | 0x81 | middle piece of the slider track |
| `$)` | 0x82 | right end of the slider track |
| `$a` | 0x83 | the slider's knob: a track piece with a gold centre |
| `$b` | 0x8B | a filled gold square with a dark drop shadow on its right and bottom |
| `$c` and `$d` | 0x8D (both) | the arrowhead (same as 0x0D) |
| `$<` | 0x1D | left end of the raised brown separator bar (palette 0x11..0x16, with light studs) |
| `$-` | 0x1E | middle piece of that bar |
| `$>` | 0x1F | right end of that bar |
| `$,` | 0x1C | a small dot, a little below the cell's centre, in greys (palette 0x02..0x0A) |
| `$.` | 0x9C | the same dot in browns (palette 0x12..0x1F) |
| `$0`..`$9` | 0x12..0x1B (digit + 0x12) | the digits 0..9 drawn in browns (palette 0x10..0x1C) |
| `$$` | 0x24 | the ordinary dollar sign |
| `$^` | 0x5E | the ordinary caret |
| `$xHH` | the byte 0xHH | whatever that index holds |

There are no other fun sequences (read).

**Are they defined by the sheet?** Partly (derived). Which sequence selects which index is the
modern client's own convention; the glyphs at those indices are the sheet's. Measured on the stock
sheet: indices 0x84 and 0x86..0x89 do NOT hold coloured lights. Each is a full 8x8 block of
dark grey, brown and blue-grey texels (palette 0x01..0x24) with no distinct shape. The names
green/red/yellow/blue/white light are the modern client's, and a client that wants lights there
must draw its own glyphs (the modern client ships its own character sets; this source does not
show their pixels). Also measured: 0x00 and 0x85 hold the same small grey dot as 0x1C; 0x7F holds
a small grey left-pointing arrow-like mark (relevant to 2.6); 0x20 and 0x8C are empty.

### 1.2 Edge cases (read)

- **An unknown `$` sequence** (e.g. `$z`, `$!`): the `$` is kept as a literal and the next
  character is then handled normally, so `$z` stays `$z`. A `$` at the very end of the text stays `$`.
- **`$x` with bad hex**: the two characters after `x` must both be hex digits (0-9, a-f, A-F; case
  does not matter). Otherwise the `$` is kept literally and `x` and what follows are copied as
  text: `$xg1` stays `$xg1`, and `$x4` at the end of the text stays `$x4`.
- **`$x00`** becomes a space (0x20), never a 0x00 byte. Every other value 0x01..0xFF is written as is,
  including 0x0A, 0x0D, 0x7F and 0xFF.
- The pass reads each `$` pair once, left to right, and never rescans its output. `$$G` becomes
  `$G` as text, not a green light.
- **`cl_parseFunChars 0`** turns the whole pass off, the `^` rule below included.
- The output is never longer than the input. The pass works on text up to 2048 bytes long.

### 1.3 Where the pass runs, and the `^` high-bit rule (read)

The pass runs in two modes, "chat" and "not chat". The only difference: in "not chat" mode, `^`
followed by any character other than a space or the end of the text is replaced by that character
with bit 7 set (`^a` becomes 0xE1). `^` followed by a space or at the end stays `^`. A `$` pair is
tested first, so `$^` gives a plain `^` and does not touch the next character. `^$` gives 0xA4.
`^^` gives 0xDE.

- **Chat mode (no `^` rule)**: `say` and `say_team` text. The fake-name prefix (`cl_fakename` +
  `cl_fakename_suffix`). The local copy of a message with hidden parts (2.6). `say`/`say_team`
  sent to a broadcast proxy as a viewer.
- **Not-chat mode (`^` rule on)**: `echo`. `set_ex` (but not `set_ex2`, which skips fun
  characters). Every user-info setting's value as it is sent to the server, so a `name` or `team`
  may hold fun characters and `^` letters. The prefixes `qtv_chatprefix` and
  `qtv_gamechatprefix` put before a broadcast viewer's chat. Several HUD texts: the team-info and
  weapon-stats elements, the score element's format strings, and the classic third-party HUD's
  text elements. Place names shown by demo tools.

### 1.4 What the receiver does with 0x0D and 0x0A (read)

`cl_nofake` (default 2): at 1, every 0x0D, and every 0x0A that is not the last character, in a
received chat line is shown as a space. At 2, the same is done for every chat line except team
messages. At 0, nothing is replaced. A 0x0D at the start of a team message therefore returns the
console to the line's start, and the message is drawn over the sender's `(name): ` prefix. The
canned messages (6) and `cl_fakename` use this.

## 2. Percent codes

### 2.1 The codes (read; examples derived)

Settings that supply words are named with their defaults. Numbers are plain decimal with no
padding unless a width is set (2.4). "Took" and "pointed" are explained in 2.5.

| Code | Text | With nothing to report |
| --- | --- | --- |
| `%a` | armour value, e.g. `150` | `0` |
| `%A` | armour type word (section 3) | `tp_name_none` (default empty) |
| `%b` | best weapon and its ammo (section 3), e.g. `{&cf13rl&cfff}:23` | `tp_name_none` + `:0`, which is `:0` by default |
| `%c` | cells, e.g. `30` | `0` |
| `%d` | where you last died this map | `tp_name_someplace` (`someplace`) until you have died on this map |
| `%e` | enemies near you (2.6) | needs the server's permission, else literal `%e` |
| `%E` | as `%e`, but within 5 s of your death it gives the count frozen at death | as `%e` |
| `%h` | health, e.g. `87` | (always a number; can be 0 or negative) |
| `%i` | last item taken, `<item> <tp_name_at> <place>`, e.g. `{&cf00ra&cfff} at quad room` | `tp_name_nothing` (`nothing`) |
| `%j` | last thing pointed at, `<thing> at <place>` | `nothing` |
| `%k`, `%m` | whichever of took or pointed is newer, within 5 s | `nothing at someplace` (the words come from `tp_name_nothing`, `tp_name_at`, `tp_name_someplace`) |
| `%l` | your place now (nearest loc point) | `someplace` |
| `%L` | where you died if that was 5 s ago or less, else your place now | `someplace` |
| `%n`, `%N` | empty text; they mark the message (2.6) | |
| `%o` | teammates near you, yourself included (2.6) | needs permission, else literal |
| `%O` | as `%o`, frozen at death for 5 s | as `%o` |
| `%p`, `%P` | your powerups, e.g. `{&c05fquad&cfff}/{&cff0ring&cfff}` | `tp_name_none` (empty) |
| `%q` | the last powerup seen on an enemy, within 5 s | the `tp_name_quad` word |
| `%r` | the last place the client worked out for you: by `%l` or `%L`, by `%y` or `%j` falling back to your own place, or at your death or a flag drop | `someplace` until one has been worked out this map |
| `%R` | your rune word | empty |
| `%s` | a light for how outnumbered you are (2.6) | needs permission, else literal |
| `%S` | your skin; in team fortress a class name | the skin name |
| `%t` | what you point at now, `<thing> at <place>` | `nothing` |
| `%u` | what you need (2.3) | `nothing` |
| `%w` | your current weapon and its ammo, e.g. `ng:120` | `:<ammo>`, with an empty name |
| `%x` | what you point at now | `nothing` |
| `%X` | last item taken | `nothing` |
| `%y` | where the thing you point at is | your own place when you point at nothing |
| `%Y` | where you took the last item | `someplace` |
| `%[a]` | `<type>:<armour>` | type `a` when you have none, e.g. `a:0` (bracketed, see 2.2) |
| `%[h]` | health | (always a number) |
| `%[p]`, `%[P]` | `%p` | empty text, no brackets |

`%x`, `%t` and `%y` (and `%j` when it falls back) also give `nothing` while the team-fortress
flash blinds you. There are no other codes (read). `%%` is a literal `%`; any other letter is
in 4.

**The words** (read; the default of every word setting the codes use):

- `tp_name_axe` `axe`, `tp_name_sg` `sg`, `tp_name_ssg` `ssg`, `tp_name_ng` `ng`, `tp_name_sng`
  `sng`, `tp_name_gl` `gl`.
- `tp_name_rl` `{&cf13rl&cfff}`, `tp_name_lg` `{&c2aalg&cfff}`, `tp_name_rlg`
  `{&cf13rl&cfff}{&c2aag&cfff}`.
- `tp_name_armortype_ga` `{&c0b0g&cfff}`, `tp_name_armortype_ya` `{&cff0y&cfff}`,
  `tp_name_armortype_ra` `{&cf00r&cfff}`.
- `tp_name_ga` `{&c0b0ga&cfff}`, `tp_name_ya` `{&cff0ya&cfff}`, `tp_name_ra` `{&cf00ra&cfff}`.
- `tp_name_quad` `{&c05fquad&cfff}`, `tp_name_pent` `{&cf00pent&cfff}`, `tp_name_ring`
  `{&cff0ring&cfff}`, `tp_name_suit` `suit`.
- `tp_name_shells` `shells`, `tp_name_nails` `nails`, `tp_name_rockets` `rox`, `tp_name_cells`
  `cells`.
- `tp_name_mh` `{&c0a0mega&cfff}`, `tp_name_health` `health`, `tp_name_armor` `armor`,
  `tp_name_weapon` `weapon`, `tp_name_backpack` `{&cf2apack&cfff}`, `tp_name_flag` `flag`.
- `tp_name_sentry` `sentry gun`, `tp_name_disp` `dispenser`, `tp_name_filter` empty.
- `tp_name_rune1` `{&c0f0resistance&cfff}`, `tp_name_rune2` `{&cf00strength&cfff}`,
  `tp_name_rune3` `{&cff0haste&cfff}`, `tp_name_rune4` `{&c0ffregeneration&cfff}`.
- `tp_name_teammate` empty, `tp_name_enemy` `{&cf00enemy&cfff}`, `tp_name_eyes`
  `{&cff0eyes&cfff}`, `tp_name_quaded` `{&c05fquaded&cfff}`, `tp_name_pented`
  `{&cf00pented&cfff}`.
- `tp_name_nothing` `nothing`, `tp_name_someplace` `someplace`, `tp_name_at` `at`,
  `tp_name_none` empty, `tp_name_separator` `/`.
- `tp_name_status_green` `$G`, `tp_name_status_yellow` `$Y`, `tp_name_status_red` `$R`,
  `tp_name_status_blue` `$B`, `tp_name_status_white` `$W`.
- The super lightning gun is always `slg`, and the keys are always `key1` and `key2`; no setting
  names them.

The `{...}` and `&cRGB` in these defaults are markup the RECEIVING client reads (read):

- A received chat line is drawn in the coloured half (bit 7 set). The exceptions are CR, LF, a
  leading 0x01 or 0x02, and bytes already at 0x80 or above.
- Text between `{` and the next `}` after the name prefix is drawn white instead, and the braces
  are not shown. This happens when `cl_parseWhiteText` is 1 (the default), or when it is 2 and
  the line is a team message. A `{` with no `}` after it is shown as is.
- `&cRGB` (three hex digits) colours the text after it and `&r` (or `&cfff`) ends the colour.
  This is done by the console's drawing.

So `{&cf13rl&cfff}` shows "rl" white, tinted orange-red.

**Other settings** (read):

- `tp_tooktimeout` (15 s): how long a pickup is remembered.
- `tp_pointtimeout` (15 s): how long a point is remembered.
- `tp_weapon_order` (`78654321`): the order for "best weapon".
- `tp_need_*`: the thresholds for `%u` (2.3).
- `tp_poweruptextstyle` (0): the form of `$tp_powerups` (6).
- `tp_pointpriorities` (0) and `tp_tookpriorities` (0): rank items by a priority list set with
  `tp_point` and `tp_took`.

### 2.2 The bracketed forms and "low" (read)

The thresholds are fixed numbers, not settings. The marking is the two bracket bytes 0x10 and
0x11 (the `$[` `$]` glyphs) written directly around the text. The bracketed forms set no bit 7
and add no markup of their own.

- `%[a]`: the text is `<A>:<armour>`, where `<A>` is the `%A` word, or the letter `a` when you
  wear none. When armour is below 30, the whole text is wrapped in 0x10 ... 0x11. For example,
  `{&cff0y&cfff}:25` with brackets around it; `a:0` with brackets at no armour.
- `%[h]`: health below 50 is wrapped in 0x10 ... 0x11; 50 or more is plain.
- `%[p]` / `%[P]`: when you hold any powerup, the `%p` text is wrapped in 0x10 ... 0x11. With
  none, the text is empty.
- Any other letter in `%[?]` is not a code: the `%` is written literally and the rest is copied.

### 2.3 `%u`, what you need (read)

The words are joined by `tp_name_separator` (`/`) in this order:

1. **Armour** (`tp_name_armor`). Needed when you wear green armour below `tp_need_ga` (60),
   yellow below `tp_need_ya` (80) or red below `tp_need_ra` (120). Also needed when you wear none
   and any of those three thresholds is not 0.
2. **Health** (`tp_name_health`). Needed when `tp_need_health` (50) is not 0 and your health is
   below it.
3. **In team fortress** only ammo follows, each when below its threshold, in the order rockets,
   shells, nails, cells: `tp_need_rockets` 5, `tp_need_shells` 0, `tp_need_nails` 0,
   `tp_need_cells` 13. Then the list ends.
4. **Otherwise a weapon**. `tp_need_weapon` (`87`) is read from its LAST digit to its first, and
   the first weapon you own decides; digits 2..8 are the shotgun to the lightning gun. If you own
   none of them, the need is the weapon word (`tp_name_weapon`). If you own one:
   - When `tp_need_rl` (1) is not 0 and you have no rocket launcher, the rocket launcher word is
     added.
   - Then that weapon's ammo word is added when you have less than its threshold: shells for
     weapons 2 and 3, nails for 4 and 5, rockets for 6 and 7, cells for 8. A threshold of 0 never
     triggers.
5. If nothing was added, the text is `nothing`.

Example (derived): green armour at 40, health 30, a lightning gun with 5 cells and no rocket launcher
gives `armor/health/{&cf13rl&cfff}/cells`.

### 2.4 Field width (read)

The syntax is `%<` width, then an optional alignment letter, then `>`, then the code letter:
`%<5>h`, `%<8r>l`, `%<10c>[a]`.

- The width is decimal digits. The alignment letter is `l`, `r` or `c` (either case) and must
  come directly before `>`.
- Anything else inside the angle brackets, or no closing `>`, makes it not a format. Then the
  `%` is written literally and the rest is copied as text.
- An explicit width has no upper limit beyond the 2048-byte buffer.
- **Default widths come from settings the player creates.** These are found by name and are not
  defined by the client: `tp_length_<k>` (0..40, clamped) and `tp_align_<k>` (first letter `r`
  or `c`; anything else is left).
  - `<k>` is the code letter. An upper-case letter is prefixed `caps_` (e.g. `tp_length_caps_a`).
   A bracketed form is prefixed `ext_` (e.g. `tp_length_ext_h`).
  - The location codes `d i j l L r y Y` all use `l`, so `tp_length_l` sets them all. `%P` uses
   `p` and `%X` uses `x`.
  - The same settings apply to `$name` macros, where `<k>` is the macro's name, e.g.
   `tp_length_health`.
- **Padding and cutting**:
  - Width counts printable characters only. `&cRGB` and `&r` count 0. While `cl_parseWhiteText`
   is not 0, a `{` or `}` that is not doubled counts 0 too.
  - Text longer than the width is cut at that many printable characters. If a colour was left
   open, `&r` is added; if a brace was left open, `}` is added.
  - Shorter text is padded with spaces. Left alignment (the default) pads on the right; right pads
   on the left; centre puts floor(spaces/2) on the left and the rest on the right.
- A width on `%n`/`%N` (which give empty text) would produce spaces (derived).

### 2.5 Took, pointed and places (read)

**Took.** A pickup is noticed by its sound:

- Quad, pent, ring and suit are recognised by their own pickup sounds. Health is recognised by the
  small and large health sounds, and the mega health by its own sound.
- The weapon pickup sound, the armour sound and the ammo/pack sound are matched to the nearest
  visible item of that kind.
- When no nearby item explains the sound, the client looks at that frame's stat changes. An
  armour value of exactly 100, 150 or 200 means green, yellow or red armour. A newly set weapon
  bit means that weapon, tested from the lightning gun down to the super shotgun. A new item
  stat with no weapon bit means a pack.
- In team fortress, a new key bit means the flag. In capture the flag, the flag effect appearing
  on you means the flag.
- Only item kinds in the "pickup" set are remembered:
  - Remembered (`tp_pickup`): powerups, armour, the five big weapons, the mega health, the pack,
   rockets, cells, the flag and the runes.
  - Fire `f_took` (`tp_took`): the same set.
  - Small health and ordinary ammo pickups are recorded only if the player adds them.
  - In deathmatch 4, weapons and ammo are removed from both sets.
- Spectators never record a took.

**Pointed.** "What you point at" is chosen fresh each time a pointing code is used, at most once
per frame:

- The eye is 22 units above your origin, moved by `v_viewheight` clamped to -7..4.
- Candidates are items in the last valid packet that are in the `tp_point` set:
  - Default set: powerups, all players, armour, the big weapons, mega, pack, rockets, cells,
   sentry, dispenser, flag, runes.
  - In deathmatch 4 ammo is removed. In deathmatch other than 1, weapons are removed.
- Other players in the current packet are also candidates when they are alive, not spectators,
  and not the gibbed-head model.
- A candidate must be at least 10 units ahead along the view. Its miss distance from the view
  line must be at most 300 units and at most 1.7 times its distance ahead (0.55 times with
  `tp_pointpriorities`).
- It must pass a line-of-sight test to its centre or to points around it. Items use their own
  radius. Players use 27 units, 30 above their origin. Anything glowing quad or pent, or carrying
  a light, uses 200.
- **Rank (smallest wins)**: the miss distance, times (distance x 0.0016 + 0.3) when closer than
  375 units.
- **Names**:
  - An item is its `tp_name_*` word. Armour is told apart by its skin: 0 green, 1 yellow, else
   red.
  - A player wearing the eyes model is `tp_name_eyes`.
  - A teammate is their own name while `tp_name_teammate` is empty, else that word.
  - An enemy is `tp_name_enemy`.
  - A spectator pointing sees player names.
  - `tp_name_quaded` and/or `tp_name_pented` are prefixed, space-separated, for a quad or pent glow.
  - In team fortress the class from the skin is appended.
- The place is the place of the pointed thing.

**Places.** `%l` and the others name the nearest loc point. With no loc file loaded, or before the
client is fully in the game, the text is `tp_name_someplace`.

### 2.6 Player counts, the light, and message marks (read)

**Permission.** `%e %E %o %O %s` work only when the server's `fpd` serverinfo value has bit 16
(65536) set. It is always treated as 0 in demo playback. Otherwise the code is not recognised
and stays literal, e.g. `%e`.

**Counting.** The counts use the PREVIOUS received frame's player list: other players present in
it, alive, not spectators.

- A teammate is someone on your team while teamplay is on. Everyone else counts as an enemy.
- `%o` includes yourself unless you are a spectator.
- At your death the counts are saved, and `%E`/`%O` give those saved counts for 5 s.

**Hidden parts.** `%e`, `%E`, `%o` and `%O` produce the number wrapped as 0xFF `z` <digits>
0xFF. `%s` produces 0xFF `l` <light> 0xFF. The light is `tp_name_status_green` with no enemies,
`..._yellow` with enemies but no more than friends, and `..._red` otherwise. Any of these turns
on "hidden" mode for the message:

1. The client prints the message locally. It shows `(yourname): ` for say_team, or the fake-name
   prefix, and `yourname: ` for say.
2. In the local copy, each hidden part is replaced by a single `x` (when its marker letter is
   `z`) or by glyph 0x8B (otherwise), then the fun-character pass runs on it.
3. The text actually sent has the 0xFF bytes and marker letters removed, so teammates see the real
   number or light.
4. Then 0x7F `!` and the letter `A` + your player slot are appended to the sent text, inside the
   closing quote.
5. The server's echo of your own message, ending in exactly 0x7F `!` <your letter> then a newline,
   is dropped by your client, so you see only the local copy.
6. Every other client strips that 4-byte tail from the line before showing it.
7. In demo playback, and while you are ignoring yourself, no local copy is printed. The local
   copy obeys the team-message filters and the `cl_nofake` rule, and plays the
   `con_sound_mm2_file` sound.

**Under the MTFL ruleset**:

- `%e` becomes the text `banned by MTFL ruleset` wrapped in 0xFF bytes, sent with no hidden mode,
  so the 0xFF bytes go out as they are.
- `%E`, `%o` and `%O` become that text plainly.

**`%N`** appends 0x7F `!` and your letter, the same tail as above. Your own client then drops the
server's echo, so the message is not shown to you at all. Outside hidden mode the tail of `%N` or
`%n` is appended at the very end of the text, after a closing quote if the text had one. The
quoting rule (4) then wraps the whole text in another pair of quotes (derived).

**`%n`** appends 0x7F (unless `%N` already did) and then one letter `A` + slot for each teammate
present and alive in the previous frame, while teamplay is on.

- No receiving behaviour for those letters exists in this client.
- When the letters are present, the tail no longer matches the 4-byte pattern. Other clients
  then show the 0x7F glyph and the letters as text (derived).
- In hidden mode with `%n` and some letters, your own echo is not dropped either (derived).

## 3. `%A`, `%w`, `%b` exactly (read)

**`%A`**: tested in the order green, yellow, red armour bit. It gives `tp_name_armortype_ga`,
`..._ya`, `..._ra`, or `tp_name_none` (empty) with none. The words are the single letters `g`,
`y`, `r` wrapped in white-text braces and a colour (2.1).

**`%w`**: `<current weapon word>:<current ammo>`, with no spaces. The ammo is the server's
"current ammo" stat, the number on the status bar.

- The weapon words are `tp_name_axe`, `_sg`, `_ssg`, `_ng`, `_sng`, `_gl`, `_rl`, `_lg`, and
  `slg` for the super lightning gun.
- With no weapon the word is `tp_name_none`. So holding the axe gives `axe:0`, and the nailgun
  with 120 nails gives `ng:120`.

**`%b`**: `<best weapon word>:<its ammo>`.

- "Best" is the first weapon you own in `tp_weapon_order` (default `78654321`; digit 1 axe,
  2 shotgun, 3 super shotgun, 4 nailgun, 5 super nailgun, 6 grenade launcher, 7 rocket launcher,
  8 lightning gun). If none in that order is owned, the fixed order `78654321` is tried.
- Its ammo is shells for 2 and 3, nails for 4 and 5, rockets for 6 and 7, cells for 8, and `0`
  for the axe or nothing.
- So with the defaults, a rocket launcher and 23 rockets gives `{&cf13rl&cfff}:23`, which shows
  as "rl:23" with "rl" white and tinted. With no weapon it gives `:0`.

## 4. Expansion order in `say` / `say_team` (read unless marked)

1. **When the command line runs**, `$name` is expanded everywhere outside double quotes. This
   covers both cvars and the client's `$` macros (`$health`, `$rockets`, `$colored_armor`,
   `$tp_powerups`, `$bestweapon`, `$took`, `$point`, `$need`, `$location` and about 60 more:
   `macrolist` prints them).
   - The name runs from after `$` to the next character at or below space, or the next `$`.
   - The longest prefix that exactly names a cvar wins over a macro, unless the macro's name is
     longer. The rest of the word is kept after the value.
   - No match leaves `$` and the word unchanged. A `$` directly followed by `$` expands to an
     empty name, so it stays `$`; the second `$` then starts its own expansion.
   - The output line is cut at 1023 characters.
   - Text typed in the chat box is sent quoted, so it gets no `$name` expansion. A `say` typed
     unquoted in the console, or put in an alias or bind, does.
2. **`say` itself** then takes everything after the command name, quotes included, and runs the
   percent codes (only while `cl_parseSay` is 1).
3. **Then the fun characters** (only while `cl_parseFunChars` is 1).
4. **Then quoting.** Unless the text already starts and ends with `"` and is at least 2
   characters long, it is wrapped in a pair of `"`. Quotes already present are kept, so
   `say "hi" there` is sent as `""hi" there"` (derived from the rule).
5. With `cl_fakename` set and not spectating, `say_team` text that holds no 0x0D gets a prefix:
   0x0D, then `cl_fakename` + `cl_fakename_suffix` (default `: `, fun characters applied), then
   the text without its outer quotes.
6. The command word is sent in lower case. Flood protection (`cl_fp_messages`,
   `cl_fp_persecond`) may refuse with `Flood Protection`.

**Consequences.**

- A `$name` value may itself contain percent codes or fun sequences, and they are expanded
  later. The `tp_name_status_*` defaults (`$G` and so on) depend on this.
- A cvar whose value is `$G`, used as `say $x`, becomes a light (derived).
- `$$x` unquoted becomes `$` + the value of x. If that value starts with a fun letter it becomes
  a glyph (derived).

**A literal `%`**: write `%%`, which gives one `%`.

**Unknown codes**: an unknown `%z` is left as `%z` (the `%` is written and `z` copied). A lone
`%` at the end is kept. A malformed `%<...` is kept as text.

**Size**: the percent pass's output is limited to 2048 bytes. Overflowing it while inserting a
code's text ends the program with a fatal error rather than cutting the text.

`echo` runs the same percent pass (also governed by `cl_parseSay`) and then the fun pass in
not-chat mode. `set_ex` expands `$name`, then percent codes, then fun characters. `set_ex2`
skips the fun characters.

## 5. Event triggers (read)

**How a trigger runs.** A trigger is an alias the player defines. When the event happens, the
alias's text is appended to the command buffer and runs at the next buffer execution, not at
once.

- The text goes to the main buffer. If the alias was created by the server it goes to the
  server's buffer instead.
- It never runs in the message-trigger safe buffer, so that buffer's command list does not apply.
  (Under a tournament ruleset, a line that uses teamplay `$` macros may still only run `if`,
  `wait`, `echo`, `say`, `say_team`, `set_tp`, `tp_point`, `tp_pickup`, `tp_took` and the
  `tp_msg*` commands.)

**The gate for every trigger below unless stated:**

1. Nothing runs while `tp_triggers` is 0.
2. Nothing runs during demo playback or while spectating when `cl_restrictions` is not 0. Its
   default is 0, so by default triggers DO run while spectating and in demos.
3. A trigger marked **teamplay** also needs teamplay on and at least one other non-spectator
   player with your team name, unless `tp_forceTriggers` is 1.
4. A trigger marked **restricted** is dropped under the smackdown, qcon, thunderdome and
   smackdrive rulesets. The default and mtfl rulesets do not restrict it.

| Alias | When | Marks |
| --- | --- | --- |
| `f_newmap` | the client has every model of the new map (downloads done) and is about to ask for the map's signon; the map is not drawn yet. Every level change, every connect, every demo's start; also for the single-player demo format | none |
| `f_spawn` | the first entity frame arrives while the client is on the server, i.e. the first frame that can be drawn; once per level or connect, also in demos | none |
| `f_mapend` | the server's intermission message arrives | none |
| `f_death` | your health stat goes from above 0 to 0 or below | teamplay, restricted; also never for spectators or under a restricting ruleset |
| `f_flagdeath` | the same death while you carry the flag | teamplay, restricted |
| `f_respawn` | your health stat goes from 0 or below to above 0 | teamplay, restricted, not for spectators |
| `f_took` | a pickup in the `tp_took` set is recorded (2.5), when the pickup sound or that frame's stat arrives | teamplay, restricted, not for spectators; in deathmatch 4 never for weapons or ammo |
| `f_weaponchange` | the current-weapon stat arrives with a value different from the last one remembered | none |
| `f_demostart` | a demo or a broadcast stream has been opened, before its first frame | none |
| `f_demoend` | demo playback has stopped, after the next playlist entry (if any) was queued | none |
| `f_exit` | the first thing done on quit; the buffer is run at once | none |
| `f_cfgload` | after `cfg_load` reads a config (also the startup `cfg_load` of `config.cfg`) | see below |
| `f_countdownstart` | the serverinfo is processed while `status` is `countdown` and already was before | none |
| `f_countdownbreak` | `status` changes from `countdown` to `standby` | none |
| `f_demomatchstart` | in multiview demo playback, when the match starts | none |
| `f_reloadstart`, `f_reloadend` | team fortress: your view weapon model vanishes or returns | none |
| `f_conc` | team fortress: a concussion forces a field of view other than 90 at full idle sway | teamplay, restricted |
| `f_flash` | team fortress: a flash grenade's screen tint | teamplay, restricted |
| `f_bonusflash` | the server's item flash, at most once per frame | teamplay, restricted |
| `f_sbrefreshdone`, `f_sbupdatesourcesdone` | the server browser finishes | none |
| `f_focusgained` | the window gains focus | none |
| `f_freeflyspectate` | the spectator camera unlocks to free flight | see below |
| `f_trackspectate` | the spectator camera locks onto a player, each time | see below |
| `f_captureframe` | each captured movie frame | see below |

Notes:

- **After a new map**: the remembered health, items and weapon are cleared at `f_newmap`. So the
  first positive health stat after a map change fires `f_respawn`, and the first weapon stat
  fires `f_weaponchange` (derived from the clearing).
- **Restricting rulesets**: under smackdown, qcon, thunderdome and smackdrive the death, respawn
  and took paths also refuse.
- **Bypassing the gate**:
  - `f_cfgload` is put in the buffer as an ordinary command after the config's text, so an alias
   the config itself defines is found. It ignores `tp_triggers` and every gate, and is
   silently skipped when no alias exists. It does not run when the file is not found.
  - `f_freeflyspectate`, `f_trackspectate` and `f_captureframe` are also queued directly when the
   alias exists, ignoring `tp_triggers` and the gate.
- An unknown name given to the trigger runner prints `Unknown f_trigger "<name>"`.

## 6. The canned messages (read; examples derived)

**Who and how.** Every canned message is sent to the team: it queues `say_team ` + prefix + body
as an unquoted command line. So `$name` macros, then percent codes, then fun characters are
expanded as in section 4.

**The prefix.**

- It is empty while `cl_fakename` is set; `say_team`'s own fake-name rule then applies.
- Otherwise it is `$\` followed by the value of a cvar named `nick` (when the player has made one
  and it is not empty), or by the first 3 characters of your name, then `cl_fakename_suffix`
  (default `: `).
- The leading 0x0D makes teammates' consoles draw `abc: ` over the `(name): ` prefix (1.4).

**Macros used in the bodies.**

- `$colored_armor`: `{&c0b0%a&cfff}` with green armour, `{&cff0%a&cfff}` with yellow,
  `{&ce00%a&cfff}` with red, `0` with none. Its `%a` is expanded afterwards.
- `$tp_powerups`, when `tp_poweruptextstyle` is 0: the quad, pent and ring words you hold, in that
  order, with NO separator.
- `$tp_powerups`, when it is 1: the letters `q`, `r`, `p` (in that order), each wrapped as
  `{&c03Fq&cfff}`, `{&cff0r&cfff}` and `{&ce00p&cfff}`.

"Holding a powerup" below means quad, pent or ring. "Dead" means health below 1. "Need" means the
`%u` rules (2.3). "Pointing" uses 2.5 and is skipped (treated as pointing at nothing) while
flashed.

- **`tp_msgreport`**: when dead, it does `tp_msglost` instead. Otherwise the body is:
  `[$tp_powerups ]$colored_armor/%h <weapons>$[{%l}$]<extra rockets><extra cells>`.
  - With both a rocket launcher and a lightning gun, `<weapons>` is
   `<rl>:$rockets <lg>:$cells ` (with a trailing space).
  - With only the rocket launcher it is `<rl>:$rockets `, and with only the lightning gun
   `<lg>:$cells `.
  - Otherwise the first of grenade launcher (`<gl>:$rockets `), super nailgun (`<sng>:$nails `)
   and super shotgun (`<ssg>:$shells `) that you have. With none of these it is empty.
  - `<extra rockets>` is ` {&cf13r&cfff}:$rockets` when you have rockets but no launcher.
  - `<extra cells>` is ` {&c2aac&cfff}:$cells` when you have cells but no lightning gun.
  - Example: 100 health, red armour 150, a launcher with 10 rockets, at "quad". This sends 0x0D
   `abc: {&ce00150&cfff}/100 {&cf13rl&cfff}:10 ` then 0x10 `{quad}` 0x11.
- **`tp_msglost`**: alive, or dead with neither quad nor a held RL/LG, the body is
  `{&cf00lost&cfff} {&cf00[&cfff}{%d}{&cf00]&cfff} {%E}`. When dead:
  - With quad: `<quad> over ` comes first.
  - Holding the rocket launcher or lightning gun: `{&cf00DROPPED} $weapon ` follows. Its brace
   has no colour reset.
  - Then `{&cf00[&cfff}{%d}{&cf00]&cfff} {%E}`.
  - Without the count permission `%E` stays literal (2.6).
- **`tp_msgenemypwr`** sends nothing while flashed. Otherwise:
  - **Pointing at eyes**: `<quaded><pented><eyes> at $[{%y}$]` when they glow both colours,
   `<quad><eyes> at ...` for a quad glow, `<pent><eyes> at ...` for a pent glow, and
   `<eyes> at $[{%y}$]` for no glow. There are no spaces between the words.
  - **Pointing at an enemy**: `<enemy> <quad><pent> at $[{%y}$]`, keeping the words that match
   its glow. With no glow the body is `<enemy> %q ` (a trailing space and no place).
  - **Else, when you hold a powerup**: if dead it does `tp_msglost`. If you need health, armour,
   the rocket launcher, rockets or cells it does `tp_msgneed`. Otherwise the body is
   `{&c0b0team&cfff} $tp_powerups at $[{%y}$]`.
  - **Else, pointing at a teammate**: `{&c0b0team&cfff} <quad><pent> at $[{%y}$]` for its glow.
   With no glow it is `<enemy> %q ` instead.
  - **Else**: `<enemy> %q`.
- **`tp_msgquaddead`**: holding quad while dead does `tp_msglost`. Pointing at a quad glow does
  `tp_msgenemypwr`. Otherwise `<quad> dead/over`.
- **`tp_msggetquad`** / **`tp_msggetpent`**: pointing at eyes with that glow sends nothing.
  Holding that powerup, or pointing at its glow, does `tp_msgenemypwr`. Otherwise
  `get <quad>` / `get <pent>`.
- **`tp_msgtook`**: nothing unless a pickup was recorded in the last `tp_tooktimeout` seconds.
  - **For quad, pent or ring**: if dead it does `tp_msgquaddead`. If you need something (as for
   enemypwr) and hold a powerup, the body is `{&c0b0team&cfff} $tp_powerups need %u $[{%Y}$]`.
   If you hold quad or ring, `{&c0b0team&cfff} $tp_powerups $[{%Y}$]`. Otherwise
   `need %u $[{%Y}$]`.
  - **Otherwise** the body is `[$tp_powerups ]took <item> $[{%Y}$]`. `<item>` is the word for
   the first match among rl, lg, gl, sng, pack, cells, rockets, mega, ra, ya, ga, flag and runes
   1 to 4, else `{$took}` (e.g. for the super shotgun or the suit).
- **`tp_msgpoint`**: nothing while flashed or when pointing at nothing. Eyes or a quad or pent
  glow does `tp_msgenemypwr`.
  - An enemy gives `{%E} <enemy> at $[{%y}$]`.
  - A teammate gives `<tp_name_teammate> at $[{%y}$] {%E}`. With `tp_name_teammate` empty, the
   teammate's name is used as `{&c0b0<name>&cfff}`.
  - An item gives `<item> at $[{%y}$] {%E}`, with the item's word chosen in the order quad, pent,
   ring, rl, lg, gl, sng, pack, ra, ya, ga, rockets, cells, nails, shells, mega, flag,
   dispenser, sentry, runes 1 to 4.
- **`tp_msgneed`**: nothing when dead or when nothing is needed (here shells and nails count
  too). Otherwise `[{&c0b0team&cfff} $tp_powerups ]need %u $[{%l}$]`.
- **`tp_msgsafe`**: nothing when dead, and nothing when pointing at an enemy without a glow.
  Pointing at a glowing enemy does `tp_msgenemypwr`. Otherwise the body is
  `{&c0b0safe&cfff} {&c0b0[&cfff}{%l}{&c0b0]&cfff} <armour><sep><weapon>`:
  - `<armour>` is `$colored_armor` when you wear any.
  - `<sep>` is `tp_name_separator` when you have armour and the RL or LG.
  - `<weapon>` is `tp_name_rlg` with both, or `$bestweapon` with one.
- **`tp_msgkillme`**: nothing when dead. Otherwise the body is
  `[{&c0b0<teammate>&cfff} ]{&cb1akill me [&cfff}{%l}{&cf2a]&cfff} <held><extra>`. The teammate
  part is there only when pointing at a teammate.
  - `<held>` is `<rl>:$rockets ` when holding the rocket launcher, or `<lg>:$cells ` when holding
   the lightning gun.
  - `<extra>` is `{&cf13r&cfff}:$rockets ` when not holding the launcher but having rockets,
   then `{&c2aac&cfff}:$cells` when not holding the lightning gun but having cells.
- **`tp_msgutake`**: `{&c0b0<teammate>&cfff} take $[{%l}$]` when pointing at a teammate,
  otherwise `you take $[{%l}$]`.
- **`tp_msghelp`**: `[$tp_powerups ]{&cff0help&cfff} {&cff0[&cfff}{%l}{&cff0]&cfff} {%e}`.
- **The one-line set**: `[$tp_powerups ]<word> $[{%l}$]`, where `<word>` is:
  - `{yes/ok}` for `tp_msgyesok`
  - `{&cf00no/cancel&cfff}` for `tp_msgnocancel`
  - `item soon` for `tp_msgitemsoon`
  - `waiting` for `tp_msgwaiting`
  - `enemy slipped` for `tp_msgslipped`
  - `replace` for `tp_msgreplace`
  - `trick` for `tp_msgtrick`
  - `coming` for `tp_msgcoming`
- **`tp_msgtfconced`** (team fortress): `$point conced at $[{%y}$] <tp_name_filter>` when
  pointing at an enemy, otherwise `{Enemy conced} <tp_name_filter>`.

There is no command for the help menu's other words. The list above is every `tp_msg*` command
(read).
