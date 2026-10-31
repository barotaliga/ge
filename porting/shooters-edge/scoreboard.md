# The scoreboards, specified

What the classic game's clients draw when the scores key is held, when the player is dead and
at the level's end, as positions and rules in our words (README.md, the rules). This note is
what the building session reads; it never opens the existing implementations.

**Where each fact came from.** A research session (2026-10-31) read the 1996 source release:
the network game's client (called **QW** below: its status bar and its message parser, and the
server code that sends the scoreboard's messages) and the single-player engine's client
(**NQ** below). The modern community client was not read: the original answers every question
here, and our default look is the original's. Facts marked `verify` are checked by a lab (the
`pak`/charset labs) before code depends on them.

**Coordinates.** Everything is on the 320x200 virtual screen, origin top left, y down. The
status bar is 24 rows high, so "the bar's row r" is screen y = 176 + r (200 - 24 + r); the
inventory line above it starts at y = 152 when the full bar (48 rows) is shown. A character
cell is 8x8; "column c" of a text at x is x + 8c. `H` below is the screen height (200 on the
virtual screen; the code uses the real height, which matters only for the row limits).

## The glyphs and the fills (both clients)

- **Text** is the 128x128 `conchars` sheet from `gfx.wad`: 16x16 cells of 8x8, glyph n at
  cell column n mod 16, row n div 16. Pixel value 0 is transparent (not drawn). A string is one
  glyph per byte, advancing 8 pixels; nothing wraps or measures.
- **The alternate (brown/red) set** is the same glyph with the high bit set (n + 128). A string
  drawn "alt" ORs 128 into every byte.
- **Clipping**: a glyph whose x is below 0 or above width - 8, or whose y is above H - 8, is
  not drawn at all (whole glyphs, never cut). A glyph partly above the top is cut.
- **Special glyphs used here** (`verify` their look in the charset lab): 16 and 17 are the
  left and right square brackets of the gold set that mark the local player; 29, 30, 31 are the
  left end, middle and right end of a horizontal rule (the header's underline: one rule per
  column, a space between rules); 14 is a small dot used as a vertical dotted separator; 12 is
  NQ's single marker beside the local player in its ranking.
- **Fill rectangles** are solid palette indices, drawn over the 3D view with no fade or
  background behind the scoreboard. A fill that would leave the screen is refused whole.
- **Player colours to palette**: a colour number is a 16-entry ramp of the palette; the fill
  uses the ramp's middle, index **colour x 16 + 8**.
  - QW clamps the colour to 0..13 first (so 14 and 15 draw as 13, index 216).
  - NQ does not clamp: the colours byte's high nibble is the top colour, the low nibble the
    bottom, and 14 and 15 give 232 and 248 (fullbright ramps).
- **Numbers in the boxes** are printed right-aligned in a width of three ("%3i" style: spaces
  on the left, a minus sign before the digits), and only the first three characters are drawn:
  a frag count of 1000 or more shows its first three digits, -5 shows " -5", -100 shows "-10".
- **The big numbers** (status bar and intermission) are the `gfx.wad` pictures `num_0` …
  `num_9`, `num_minus`, `num_colon`, `num_slash` (the red set is `anum_*`), each 24 pixels
  wide, drawn with index 255 transparent. A number in a field of d digits is right-aligned in
  d x 24 pixels; one longer than d keeps only its last d characters.

## QW: what fills each player's row

The client keeps up to 32 player slots. A slot with an empty name is unused and never listed.

| Message (id) | Bytes after the id | Fills |
| --- | --- | --- |
| updatefrags (14) | slot byte, frags short (signed 16) | the frags |
| updateping (36) | slot byte, ping short | the ping in milliseconds |
| updatepl (53) | slot byte, loss byte | the packet loss percent |
| updateentertime (37) | slot byte, a float: seconds since that player connected | the time on server: the client stores (its own clock - the float) and shows (now - stored) |
| updateuserinfo (40) | slot byte, user id long, userinfo string | the name, colours, team, spectator |
| setinfo (51) | slot byte, key string, value string | one userinfo key changed; the row is re-derived as for updateuserinfo |

- **Userinfo keys read**: `name` (kept to 15 characters), `topcolor` and `bottomcolor`
  (integers; missing reads as 0), `team` (compared on its first 16 characters, shown on its
  first 4), and `*spectator` (any non-empty value makes the slot a spectator). The server
  strips keys starting with `_` before sending.
- **The serverinfo key `teamplay`**: an integer; non-zero selects the team layouts below.
- **When the server sends them**: all five (frags, ping, loss, enter time, userinfo, in that
  order) for a player whenever his info changes and to a client joining; updatefrags to every
  connected client each frame a player's frags differ from what was last sent.
- **Ping**: the server's average round trip over the player's recent frames that have one
  (in ms), 9999 when none has. **Loss**: the CLIENT computes it and reports it in each move
  message: the share of its last 256 sent packets whose reply never came, as 0..100; the server
  just relays it.
- **Refresh while shown**: while the deathmatch overlay is on screen, the client sends the
  string command `pings` at most every 2 seconds of real time; the server answers with an
  updateping and an updatepl for every fully connected player.

## QW: the deathmatch scoreboard (held key, death, intermission)

**When.** Held `+showscores` (the classic bind is TAB; `verify` against the pack's default
config), or the local player's health at or below 0 (not as a spectator), or the
intermission. With `teamplay` non-zero, a dead player and the intermission get the TEAM
scoreboard instead, unless the scores key is also held. A second key command,
`+showteamscores`, shows the team scoreboard (no default bind). The intermission (message 30)
draws only this overlay: no status bar, no centre print.

**The picture.** `gfx/ranking.lmp` (a loose file in the pack, not in the wad), drawn opaque at
x = 160 - width/2, y = 0. This is the 320-wide column's centre, NOT the real screen's: the QW
layout stays at the left on a wider screen.

**Sort.** Every named slot, spectators included, is listed. A spectator's frags are forced to
-999 (in the client's copy) so spectators sink to the bottom. Then a stable sort by frags,
highest first; equal frags keep slot order (lower slot first).

**Free-for-all layout** (`teamplay` 0), left edge x = 16:

- y = 24: the header `ping pl time frags name`.
- y = 32: the underline, rules of 4, 2, 4, 5 and 16 glyphs with a space between each, under
  the header's five words.
- Rows from y = 40, one every 10 pixels (8 once squished, below). Columns, from x:

| x offset | Draws |
| --- | --- |
| +0 | ping, right-aligned in 4 characters; below 0 or above 999 shows 999 |
| +32 | packet loss, right-aligned in 3; drawn in the alt set when above 25 |
| +64 | minutes on the server, right-aligned in 4 (whole minutes, truncated); during the intermission the clock is frozen at the moment it began |
| +104 | the colour box: 40 wide; top colour rows y..y+3, bottom colour y+4..y+7 |
| +112, +120, +128 | the frags, three glyphs inside the box |
| +104 and +136 | glyphs 16 and 17 around the frags, the local player only (over the box's ends) |
| +152 | the name |

- A **spectator's row**, in draw order: ping at +0 (4 characters, +0..+31), loss at +32
  (3 characters, +32..+55, alt over 25), then the text `(spectator)` at +40 (11 characters,
  +40..+127), then the name at +152 (free-for-all) or +192 (team layout; no team is drawn for
  a spectator). The text DOES overlap the loss: it is drawn after it and only its non-zero
  pixels land, so the loss's tens and units glyphs (at +40 and +48) show through the `(s` of
  the text. That is what the original draws; reproduce it (the classic look). No time, no box,
  no frags. Left edge x is 16 free-for-all, 4 team, so on screen: ping 16 / 4, loss 48 / 36,
  text 56 / 44, name 168 / 196.
- **Rows stop** before a row whose y would be past H - 10 (on 200: the last row is at y ≤ 190,
  so 16 rows at the 10-pixel spacing).
- **Squish**: when the rows reach y ≥ H - 10 the client switches to the "large game" layout
  from the next frame on, and it never switches back for the session: the row spacing becomes
  8 and the top colour fill shrinks to rows y+1..y+3 (the bottom stays y+4..y+7). On 200 that
  fits 19 rows.

**Team layout** (`teamplay` non-zero), left edge x = 4: the header
`ping pl time frags team name`, the underline adds a 4-glyph rule for team; every column as
above, then the team (first 4 characters) at +152 and the name at +192. On a 320-wide screen
a name's characters beyond the 15th fall off the right edge (whole glyphs refused).

## QW: the team scoreboard

- `gfx/ranking.lmp` as above. Left edge x = 36.
- y = 24: header `low/avg/high team total players`; y = 32: rules of 12, 4, 5 and 7.
- **Teams**: built from the non-spectator named players with a non-empty `team`, in order of
  first appearance by slot; each team sums its players' frags, counts its players and keeps
  the lowest, highest and total ping. Then sorted by total frags, highest first: for each
  position i from the first, every later position j whose team has strictly more frags than
  the one at i is exchanged into i (an exchange sort; equal totals are not exchanged).
- One row per team from y = 40, every 8 pixels, while y ≤ H - 10:

| x offset | Draws |
| --- | --- |
| +0 | `low/avg/high` as three numbers each right-aligned in 3 with `/` between (11 characters); each outside 0..999 shows 999; avg is the integer mean (999 with no players) |
| +104 | the team name, first 4 characters |
| +96 and +136 | glyphs 16 and 17 around it when it is the local player's team |
| +144 | total frags, right-aligned in 5 |
| +192 | players, right-aligned in 5 |

- Below the last team, one blank line (8 pixels), then the **player scoreboard follows** at
  that y in the team layout (header, underline, rows), without drawing the picture again.
- With `teamplay` 0 the team scoreboard is just the deathmatch scoreboard.

## QW: the status bar while the scores are shown

- The scores key held or the player dead replaces the normal bar with `scorebar` (from
  `gfx.wad`, 320x24) at the bar's origin (x 0, y 176), and on it only the map time
  `Time :%3i:%i%i` style (the label, minutes right-aligned in 3, a colon, two digits of seconds)
  at the bar's (184, 4). The time is the client's clock since the map began. The overlay above
  is drawn on top of the view as well.
- A spectator not tracking anyone always has `scorebar` with `SPECTATOR MODE` at (104, 4)
  and `Press [ATTACK] for AutoCamera` at (52, 12) of the bar; tracking, he gets the player's
  bar and `Tracking <name up to 13>, [JUMP] for next` one line above the bar (bar row -8).
- **The frags line** (inventory line, only with the full 48-row bar, and only on a screen
  under 512 wide or with the bar not in head-up mode): the top four non-spectators by the
  sort, left to right from column 23: for the i-th, the box at x = 194 + 32i, y = 153
  (28 wide, top colour 4 rows, bottom 3 rows), the frags' three glyphs at x = 196 + 32i,
  204 + 32i, 212 + 32i (each 8 x their cell + 4) on y = 152, and for the local player glyph
  16 at x = 190 + 32i and glyph 17 at x = 216 + 32i.
- **The mini overlay** right of the bar exists only on screens 512 wide or more (never on our
  320 virtual screen): boxes at x = 324 with frags and names, the rows centred on the local
  player; on 640 or more a dotted separator (glyph 14) and team totals. Not needed for 320.

## NQ: the solo scoreboard (single player, coop)

Held scores key or the player dead: `scorebar` at the bar's origin, and on it (positions in the
bar, add 176 for screen y; in NQ single player the whole bar is centred, x offset
(width - 320)/2, 0 on 320):

| At (x, y) | Text |
| --- | --- |
| (8, 4) | `Monsters:` then killed right-aligned in 3, ` /`, total right-aligned in 3 (e.g. `Monsters:  4 / 27`) |
| (8, 12) | `Secrets :` then found in 3, ` /`, total in 3 |
| (184, 4) | `Time :` minutes in 3, `:`, two digits of seconds (`Time :  1:07`) |
| centred on x = 232, y 12 | the map's title (the world's message, the server info's level name): x = 232 - 4 x its length |

The four numbers are the statistics 14 (monsters killed), 12 (total monsters), 13 (secrets
found), 11 (total secrets) (`campaign.md` § The campaign's statistics). In an NQ deathmatch the
same panel is drawn (with zeros) and the NQ ranking is drawn over the view as well.

## NQ: the deathmatch ranking (for comparison)

- Fed by updatename (13: slot byte, name string), updatefrags (14: slot byte, frags short),
  updatecolors (17: slot byte, colours byte: top in the high nibble, bottom in the low).
- `gfx/ranking.lmp` at x = (320 - width)/2 + the centring offset, y = 8.
- No header. Rows from y = 40 every 10, no row limit (they clip). Left edge x = 80 + centring:
  the box 40 wide (top 4 rows, bottom 4), the frags' three glyphs at +8, +16, +24, glyph 12 at
  -8 for the local player, the name at +64.
- Sorted by frags, highest first, stable as in QW; no spectators exist.
- The frags line in the inventory line as in QW (no clamp on colours); the mini overlay as in
  QW but without teams, only on screens 512 wide or more.
- At an NQ deathmatch intermission the ranking alone is drawn.

## NQ: the intermission overlay (single player, coop)

Corrections and additions to `campaign.md` § The level's end. Positions are absolute on the
screen, NOT centred on a wider one (only the 320-wide column):

| Draws | At (x, y) |
| --- | --- |
| `gfx/complete.lmp`, opaque | (64, 24) |
| `gfx/inter.lmp` (the labels: time, secrets, killed), transparent | (0, 56) |
| minutes, big numbers, field of 3 | (160, 64) |
| `num_colon` | (234, 64) |
| tens of seconds, `num_<d>` | (246, 64) |
| units of seconds | (266, 64) |
| secrets found, field of 3 | (160, 104) |
| `num_slash` | (232, 104) |
| total secrets, field of 3 | (240, 104) |
| monsters killed, field of 3 | (160, 144) |
| `num_slash` | (232, 144) |
| total monsters, field of 3 | (240, 144) |

- The time is the client's clock when the intermission message arrived (whole seconds),
  frozen; all digits are the normal (not red) set.
- The finale (message 31) draws `gfx/finale.lmp` transparent, centred: x = (width -
  picture width)/2, y = 16, then the typed-out centre text (`campaign.md`).
- QW's intermission message carries the view's origin (three coordinates) and angles (three
  angles); NQ's carries nothing. QW never shows the campaign overlay.

## Reference looks

- **QW free-for-all, held TAB**: the RANKINGS banner centred at the top; under it a white
  header line `ping pl time frags name` with a gold-ish rule under each word; then rows of
  white numbers, each with a two-colour bar (shirt colour over pants colour) holding the frag
  count, the local player's bar bracketed in gold `[ 12]`; names on the right; a spectator
  row reads `  37   0 (spectator)  name`. The 3D view shows through everywhere, and the status
  bar below is the plain scorebar with `Time :  4:12` on its right half.
- **QW team game, dead**: the banner, a `low/avg/high team total players` table of a few team
  lines (`  12/ 40/ 88  red      34    4`, the player's team bracketed), a blank line, then the
  player table with a team column.
- **QW large game**: the same table with rows 8 apart and thinner top halves of the boxes.
- **NQ solo, held TAB**: the dark scorebar in place of the status bar with
  `Monsters:  4 / 27` and `Secrets :  0 /  5` stacked on the left, `Time :  1:07` and the map's
  title under it on the right; the view is otherwise untouched.
- **NQ deathmatch**: the banner, then a centred column of coloured frag boxes and names, no
  header.
- **NQ level end**: the COMPLETED banner near the top, the labels picture below on the left,
  and the big digits: `1:07` on the time line, `2/5` secrets, `24/27` killed.
