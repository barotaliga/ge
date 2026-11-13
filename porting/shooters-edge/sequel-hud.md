# The sequel's status bar and layout language

This is a working note of clean-room facts about how the sequel's stock client draws its HUD and
what the stock game feeds it: the layout language word by word (positions, numbers, pictures,
text, conditions, the scoreboard blocks), the 32 stats the game fills each frame, the two status
bar programs as elements, positions and conditions, the scoreboard, help computer and inventory
screens, and the centre print and crosshair. It was written by a reader who read id Software's GPL
source release of Quake II (the original release on GitHub, default branch; protocol 34) for M18v4,
and contains no code. Each fact is marked **[read]** (stated by the source), **[measured]**
(counted from files or runs; none yet in this note) or **[derived]** (worked out from read facts).
Where the source does not settle something, it says so. The sections are numbered after the
questions they answered.

Conventions: "layout units" and screen pixels are the same thing in the stock client (see Q1).
The stats are 32 signed 16-bit values carried in the player state **[read]**. The configstring
ranges named below are: models 32..287, sounds 288..543, images 544..799, light styles 800..1055,
items 1056..1311, player skins 1312..1567, general 1568..2079; 2080 configstrings in all, each at
most 64 bytes **[read]**. Image configstring 544 (image index 0) is never assigned: index 0 means
"no picture" **[read]**.

---

## 1. The words of the layout language

**Parsing.** A layout program is a string of whitespace-separated words. Anything at or below the
space character is whitespace; a double quote starts a quoted word that runs to the next double
quote (or the end) and may hold spaces; two slashes start a comment to the end of the line **[read]**.
A plain word longer than 127 characters becomes an empty word; a quoted word is cut to 128
characters **[read]**. Numbers are read as decimal integers, with the usual "leading digits, else 0"
reading of a word that is not a number **[read]**.

**When it runs.** The client runs the status bar program (configstring 5, running on into later
slots) every rendered frame, and the last layout message's program on top of it when stat 13 has
bit 1 set **[read]**. Nothing is drawn unless the client is fully connected and the level's
pictures are loaded, and an empty program draws nothing **[read]**.

**State.** The cursor starts at x = 0, y = 0 at the start of every program run (status bar and
layout each start fresh) **[read]**. A number width also exists, starting at 3, but only the `num`
word uses its own argument; nothing carries it between words **[derived]**.

**Positions (all take one integer, n):** **[read]**

| Word | Sets | To |
| --- | --- | --- |
| `xl n` | x | n pixels from the screen's left edge |
| `xr n` | x | the screen width plus n (n is normally negative: from the right edge) |
| `xv n` | x | half the screen width minus 160, plus n: a 320-wide virtual field centred horizontally |
| `yt n` | y | n pixels from the top edge |
| `yb n` | y | the screen height plus n (n normally negative: from the bottom edge) |
| `yv n` | y | half the screen height minus 120, plus n: a 240-high virtual field centred vertically |

(The table's shape is ours; the words are the format's own.)

**Scale.** There is no scaling. One layout unit is one pixel of the current video mode; on a
640x480 screen the 320x240 virtual field is simply centred, at its original size, with
`xv 0` at x = 160 and `yv 0` at y = 120 **[read]**. Pictures and characters are drawn at their
native pixel size **[read]**.

**Drawing words** (Q2 and Q3 give each in full): `pic`, `picn`, `num`, `hnum`, `anum`, `rnum`,
`stat_string`, `string`, `string2`, `cstring`, `cstring2`, `client`, `ctf`, and the condition
`if` with its terminator `endif` **[read]**. That is the whole vocabulary of the stock client:
there is no `face`, no colour word, no `else` and no nesting **[read]**.

**Unknown words.** A word the client does not know is skipped silently, one word at a time; its
arguments, if any, are then met as words themselves and skipped the same way (a number is just an
unknown word) **[derived]**. No error and no console message **[read]**. `endif` met outside a
skip is such an ignored word **[derived]**.

**A stat out of range.** The words that take a stat number (`pic`, `num`, `if`, and the first
argument of `stat_string`) do not check that the number is in 0..31: the client reads whatever
lies past the 32 stats **[derived]**; `stat_string` checks only that its argument is below 2080
**[read]**. The value-side checks are listed with each word below.

## 2. The numbers: `num`, `hnum`, `anum`, `rnum`

**The digit pictures** **[read]**: two sets of eleven. The plain set is `num_0` .. `num_9` and
`num_minus`; the alternate set is `anum_0` .. `anum_9` and `anum_minus`. All 22 are loaded with the
level's pictures. A picture name without a leading slash is looked up as `pics/<name>.pcx`
**[read]**. The pictures' size is not in the source; the earlier note gives 16x24.

**The field** **[read]**: a number is drawn in a field of `width` digits, 16 pixels per digit,
with no gap between digits (the advance is exactly 16). A width below 1 draws nothing; a width
above 5 is treated as 5. The value is written in decimal (a minus sign first when negative, drawn
with the minus picture); the text is right-aligned: its first character starts at
x + 2 + 16 x (width - characters). The 2-pixel inset is always there.

**Too wide** **[derived]**: when the text has more characters than the width, only its first
`width` characters are drawn, starting at x + 2. So 1234 in a width of 3 shows "123", and -150 in
a width of 3 shows "-15". No clamping and no overflow mark.

**`num w s`** **[read]**: draws stat s in a field of width w, always in the plain set, no flash
background.

**`hnum`** (no arguments) **[read]**: stat 1 (health), width 3.
- Above 25: plain set.
- 1 to 25: alternates between the plain and the alternate set, switching every 4 server frames
  (0.4 s at the 10 Hz server frame), by the server frame number the client last received.
- 0 or below: alternate set, steady.
- Stat 15 bit 1 set: the picture `field_3` is drawn at the cursor first, behind the digits.

**`anum`** (no arguments) **[read]**: stat 3 (ammo), width 3.
- Above 5: plain set.
- 0 to 5: alternating as for health (every 4 server frames).
- Negative: nothing is drawn at all.
- Stat 15 bit 4 set: `field_3` behind it. The stock game never sets bit 4 (Q4) **[derived]**.

**`rnum`** (no arguments) **[read]**: stat 5 (armour), width 3.
- Below 1: nothing is drawn.
- Otherwise always the plain set (no low-value flashing).
- Stat 15 bit 2 set: `field_3` behind it.

The colours of the two digit sets are in the pictures, not in the source **[read]**; the source
does not settle what colour the player sees. The size of `field_3` is not in the source either.

## 3. The other words

**`pic s`** **[read]**: takes the value of stat s as an image index and draws image configstring
544 + value at the cursor (top-left corner at the cursor, native size). A value of 256 or more
drops the client from the server with the error "Pic >= MAX_IMAGES" **[read]**; a negative value
is not checked and reads a configstring below the image range **[derived]**. A value of 0 (or any
unassigned slot) is not skipped: the client asks for a picture with an empty name, which the
renderer cannot find and reports on the console as "Can't find pic: " followed by the name
**[derived]**. That is why the stock programs wrap every `pic` in an `if` on the same stat
**[derived]**.

**`picn name`** **[read]**: draws the picture named by the word itself at the cursor. Without a
leading slash or backslash the name means `pics/<name>.pcx`; with one, the rest is a path from the
game's root (the scoreboard's player icons use this) **[read]**.

**`stat_string s`** **[read]**: s must be 0..2079, else the client drops with "Bad stat_string
index"; the value of stat s must also be 0..2079, else the same error. It draws configstring
number (value) as one line of plain text at the cursor. The stock game points it at an item's name
(1056 + item) or a player-skin slot (1312 + client) **[read]**.

**`string "text"`** **[read]**: draws the text at the cursor in the plain character set.
**`string2 "text"`**: the same in the alternate set. Both draw one line: a newline character is
not treated as a line break but drawn as its glyph **[derived]**.

**`cstring "text"`** and **`cstring2 "text"`** **[read]**: the text is split at newlines; each
line is centred within a 320-pixel-wide field whose left edge is the cursor's x (line start =
x + (320 - 8 x characters) / 2, integer division), and each following line is 8 pixels lower.
`cstring` uses the plain set, `cstring2` the alternate set. Centring is on character count; a line
wider than 320 pixels starts left of x **[derived]**.

**Text drawing** **[read]**: characters come from the picture `conchars` (`pics/conchars.pcx`),
a 16 x 16 grid of 8 x 8 glyphs, 256 characters; each character advances 8 pixels. The alternate
set is the same character with its high bit set (code + 128), which is the coloured half of the
font. A space, plain or alternate (32 or 160), draws nothing but still advances. A character whose
top is 8 or more pixels above the screen is not drawn.

**`if s` .. `endif`** **[read]**: reads stat s. When it is non-zero, drawing simply continues (and
the later `endif` is ignored). When it is zero, every following word is skipped up to and
including the first `endif`. There is no nesting: an inner `endif` ends the outer skip
**[derived]**. A skip that reaches the end of the program ends the program **[read]**. Position
words inside a skipped block do not run, so x and y keep their values from before the `if`
**[derived]**.

**`client x y c score ping time`** **[read]**: a deathmatch scoreboard block, 160 x 32 pixels.
x and y are in the virtual field (the same as `xv x yv y`) and become the cursor. c is a client
number 0..255; outside that range the client drops with "client >= MAX_CLIENTS". Drawn:
- the player's name, alternate set, at (x + 32, y),
- "Score: " plain at (x + 32, y + 8), then the score in the alternate set from x + 88,
- "Ping:  " and the ping (two spaces after the colon), plain, at (x + 32, y + 16),
- "Time:  " and the time, plain, at (x + 32, y + 24),
- last, the player's icon picture at (x, y): the picture at `players/<model>/<skin>_i.pcx`
  for that client's skin; when that icon did not load, the default player's icon
  (`players/male/grunt_i.pcx`) **[read]**.
The name is the part of the client's player-skin configstring before the backslash **[read]**.
Numbers are plain decimal with no padding **[read]**.

**`ctf x y c score ping`** **[read]**: a one-line team-game row. x, y as for `client`; c checked
the same way; ping above 999 shows as 999. It draws, at the cursor, the score right-aligned in 3
columns, a space, the ping right-aligned in 3 columns, a space, and the name left-aligned and
padded or cut to 12 characters. The row is in the alternate set when c is the viewing player's own
client number, else in the plain set. The stock deathmatch game never sends it; the team-game mod
in the same release does **[read]**.

## 4. The stats the stock game fills

Stats are recomputed for every player at the end of every 10 Hz server frame **[read]**. The stock
game uses indices 0..17; 18..31 stay 0 **[read]** (the team-game mod in the same release uses more,
not covered here). Picture values are image indices (image configstring 544 + value).

| Index | Meaning | Value and when it changes |
| --- | --- | --- |
| 0 | health icon | always the image index of `i_health` **[read]** |
| 1 | health | the player's current health, every frame **[read]** |
| 2 | ammo icon | the icon of the ammo the current weapon uses; 0 when the weapon uses none (the blaster) **[read]** |
| 3 | ammo | the count of that ammo in the inventory (0 is shown); 0 when no ammo type **[read]** |
| 4 | armour icon | see below **[read]** |
| 5 | armour | see below **[read]** |
| 6 | selected item icon | the icon of the selected inventory item; 0 when none is selected **[read]** |
| 7 | pickup icon | the icon of the item just picked up **[read]** |
| 8 | pickup string | 1056 + the item's number: the item configstring holding its pickup name **[read]** |
| 9 | timer icon | the power-up being timed **[read]** |
| 10 | timer | its seconds left **[read]** |
| 11 | help icon | the flashing help picture, or the weapon icon (below) **[read]** |
| 12 | selected item | the selected item's number, -1 when none **[read]** |
| 13 | layouts | bit 1: draw the layout; bit 2: draw the inventory **[read]** |
| 14 | frags | the player's score **[read]** |
| 15 | flashes | bit 1: health hit, bit 2: armour hit (bit 4, ammo, read by the client but never set) **[read]** |
| 16 | chase | for a spectator chasing a player, 1312 + the target's client number; else 0 **[read]** |
| 17 | spectator | 1 for a spectator, 0 for everyone else **[read]** |

(The table is ours, written from what each index does.)

**Armour (4, 5)** **[read]**: when power armour is switched on and the player has no cells left,
it is switched off and `misc/power2.wav` plays. Then: when power armour is on and either the player
has no ordinary armour or bit 8 of the server frame number is set (alternating every 8 frames,
0.8 s), icon 4 is `i_powershield` and stat 5 is the cell count; otherwise, with ordinary armour,
the armour item's icon and its count; with neither, 0 and 0. So with both kinds, the readout
alternates between them every 0.8 s **[derived]**.

**Pickup (7, 8)** **[read]**: set when an item is picked up, with a 3.0 s hold; both are cleared
on the first frame whose level time is later than the pickup time plus 3.0 s. Picking up an item
that can be used also makes it the selected item (stats 6 and 12) **[read]**.

**Timer (9, 10)** **[read]**: the first of quad damage (`p_quad`), invulnerability
(`p_invulnerability`), environment suit (`p_envirosuit`) and rebreather (`p_rebreather`) that
is still running, in that order; the value is the frames left divided by 10, truncated, so whole
seconds rounded down (a power-up with 0.9 s left shows 0). With none running, 0 and 0. Going to
the intermission clears all four **[read]**.

**Help icon (11)** **[read]**: when the help message has changed since the player last opened the
help computer, and bit 8 of the server frame number is set, the image index of `i_help`
(flashing, 0.8 s on and 0.8 s off). Otherwise, when the player's `hand` setting is 2 (centre) or
the field of view is above 91 degrees, and the player holds a weapon, that weapon's icon (the
held weapon is not otherwise visible then). Otherwise 0. The help flag is set when a level's help
message changes, and three beeps (`misc/pc_up.wav`) sound at most, one every 64 frames (6.4 s)
**[read]**. In the help-off half of the flash a centre-handed player sees the weapon icon instead
**[derived]**.

**Layouts (13)** **[read]**, rebuilt from zero every frame:
- in deathmatch, bit 1 when the player's persistent health is 0 or below (that copy is taken at
  level changes), or the intermission is on, or the scoreboard is open; bit 2 when the inventory is
  open and the persistent health is above 0,
- in single player and co-op, bit 1 when the scoreboard or the help computer is open; bit 2 as in
  deathmatch,
- spectators use the deathmatch rule whatever the mode.

**Flashes (15)** **[read]**: rebuilt every frame from the damage taken that frame: bit 1 when any
damage reached health, bit 2 when ordinary armour absorbed any, unless the player is in god mode or
invulnerable. So the `field_3` background shows for one server frame (0.1 s) per hit
**[derived]**. During the intermission the flashes are not recomputed **[read]**.

**Spectators (16, 17)** **[read]**: a spectator who chases no one gets the normal stats computed
for itself; a chasing spectator gets a copy of the target's whole stats each frame, then 17 is set
to 1, 13 is recomputed from the spectator's own state, and 16 points at the target's player-skin
configstring. That configstring holds the name, a backslash and the model/skin, so the chase line
shows all of it **[derived]**.

## 5. The stock status bars

The game sends one of two programs as configstring 5 when a level starts: the deathmatch program
when the `deathmatch` setting is non-zero, else the single-player one (co-op uses the
single-player one) **[read]**. Both programs share the lower rows; deathmatch adds three elements.
Positions below are the cursor (top-left) of each element, in the format's own words.

**Bottom row, y at `yb -24` (24 pixels above the bottom edge)** **[read]**:
- health number (`hnum`, stat 1) at `xv 0`; always shown,
- health icon (stat 0) at `xv 50`; always shown,
- when stat 2 is non-zero: ammo number (`anum`) at `xv 100` and ammo icon (stat 2) at `xv 150`,
- when stat 4 is non-zero: armour number (`rnum`) at `xv 200` and armour icon (stat 4) at `xv 250`,
- when stat 6 is non-zero: the selected item's icon at `xv 296`.

**Second row, y at `yb -50`** **[read]**:
- when stat 7 is non-zero: the pickup icon at `xv 0`, `yb -50`, and the pickup name (the text of
  the configstring stat 8 names) at `xv 26`, `yb -42` (8 pixels lower, so the 8-pixel text sits
  level with the middle of a 24-pixel icon **[derived]**),
- when stat 9 is non-zero: the timer, stat 10 as a 2-digit plain number, at `xv 262` in single
  player and at `xv 246` in deathmatch, and the timer icon (stat 9) at `xv 296`,
- when stat 11 is non-zero: the help or weapon icon at `xv 148`.

**Deathmatch only** **[read]**:
- the frags, stat 14 as a 3-digit plain number, at `xr -50`, `yt 2` (top right corner); always
  shown, outside any condition,
- when stat 17 is non-zero: the words "SPECTATOR MODE" in the alternate set at `xv 0`, `yb -58`,
- when stat 16 is non-zero: the word "Chasing" in the plain set at `xv 0`, `yb -68`, and the
  configstring stat 16 names, plain, at `xv 64` on the same row.

**Layout of the field** **[derived]**: in the 320-wide field, health digits cover 2..50, health
icon 50..74, ammo digits 102..150, ammo icon 150..174, armour digits 202..250, armour icon
250..274, selected item 296..320; the 3-digit frags cover the last 48 pixels at the top right.
The single-player timer's two digits cover 264..296, butting onto its icon; the deathmatch timer's
cover 248..280.

## 6. The layout message, the scoreboard, help and the inventory

**The layout message** **[read]**: one string. The client keeps the latest one (up to 1023
characters; more is cut) until the next replaces it; it is drawn every frame while stat 13 has
bit 1 set, over the status bar.

**The deathmatch scoreboard** **[read]**:
- Players who are in the game and not spectators, sorted by score, highest first; equal scores
  keep client-number order **[derived]**. At most 12 are shown.
- Rank r (counted from 0) goes at x = 0 for ranks 0..5 and x = 160 for ranks 6..11, and
  y = 32 + 32 x (r mod 6), both in the virtual field: two columns of six, from `yv 32` down to
  `yv 192`.
- For the viewing player's own row, a dog-tag picture `tag1` is first drawn at (x + 32, y),
  behind the name; for the row of the player who last killed the viewer, `tag2` at the same place.
  Other rows have no tag.
- Then a `client` block (Q3) with the client number, score (frags), ping in milliseconds and
  time in the game in whole minutes (server frames since the player entered, divided by 600).
- The program has no title or level name. It is kept under 1024 characters: a row that would not
  fit, and every row after it, is left out.
- Co-op sends the same scoreboard when asked **[read]**.

**When it shows and how long** **[read]**:
- the `score` command toggles it in deathmatch and co-op (in single player it does nothing but
  close help and the inventory); opening sends it at once, reliably,
- while it is open, it is re-sent every 32 server frames (3.2 s), unreliably, so scores and pings
  stay current,
- dying in deathmatch runs `help`, which in deathmatch means `score`: it toggles the scoreboard
  (so it opens, unless it was already open, in which case it closes) **[read]**/**[derived]**,
- respawning closes it: the player's per-life state is wiped **[derived]**,
- the intermission, in deathmatch or co-op, opens it and sends it with no killer tag,
- `score` and `help` still work during the intermission; `inven`, `putaway` and the other game
  commands do not,
- `putaway` closes the scoreboard, help and the inventory together; `inven` closes the
  scoreboard and help.

**The help computer** (single player and co-op) **[read]**: the `help` command shows it, or hides
it when it is already up and the help message has not changed since. Opening it clears the
flashing help icon. Its program, in the virtual field: the picture `help` at `xv 32 yv 8` as the
background; the skill word ("easy", "medium", "hard", or "hard+" for skill 3 and up) in the
alternate set at `xv 202 yv 12`; the level's name centred (`cstring2`) from `xv 0` at `yv 24`,
the first help message centred at `yv 54`; the second at `yv 110`; at `xv 50 yv 164` the heading
" kills     goals    secrets" in the alternate set, and under it at `yv 172` the three
found/total pairs, kills each in 3 columns, goals and secrets unpadded. It is sent once, reliably,
on the command; it is not refreshed while open. In deathmatch `help` is `score`.

**The inventory** **[read]**:
- `inven` toggles it (and closes the scoreboard and help). Opening sends the inventory message:
  256 signed 16-bit counts, one per item number (512 bytes). It is not re-sent while open, so the
  counts shown are those at opening; the selection (stat 12) moves live **[derived]**.
- It is drawn while stat 13 has bit 2 set: the player is alive and it is open.
- Placement: a 256 x 240 box centred on the screen, origin at ((width - 256) / 2,
  (height - 240) / 2). The picture `inventory` at (origin x, origin y + 8). From
  (origin x + 24, origin y + 24) the header "hotkey ### item" and under it "------ --- ----",
  rows from origin y + 40, 8 pixels each, at most 17 rows.
- Each row is one item the player has a non-zero count of, in item-number order: the name of the
  first key bound exactly to "use <item name>" (compared without case), right-aligned in 6
  columns (blank when none); a space; the count right-aligned in 3 columns; a space; the item's
  name. The selected item's row is in the plain set with a blinking cursor (character 15) at 8
  pixels to its left, toggling every 0.1 s of real time; every other row is in the alternate set.
- Scrolling keeps the selection about in the middle: the first row shown is the selection's place
  minus 8, moved back so the window is full when there are 17 or more items, and never before the
  first item.
- The selection commands are `invnext`, `invprev`, `invnextw`, `invprevw` (weapons), `invnextp`,
  `invprevp` (powered items), `invuse` and `invdrop`.

## 7. Centre print, the pickup line and the crosshair

**Draw order each frame** **[read]**: the 3D view, then the crosshair, then the status bar
program, the layout (stat 13 bit 1), the inventory (bit 2), the lag icon, the centre print, the
debug graphs, the pause picture, the console, the menu, the loading picture. So the centre print
is drawn over the scoreboard and the inventory.

**Centre print** **[read]**: the centre print message carries one string. The client shows it for
`scr_centertime` seconds (default 2.5), counted down by real frame time, and then stops drawing it,
a new one replaces the old and restarts the time. Lines (split at newlines) are drawn in the plain
set, each centred horizontally on the screen by its own length; at most 40 characters of a line
are drawn and the rest of that line is dropped **[derived]**. The first line's top is at 35% of
the screen height (rounded down) when the message has 4 lines or fewer, else at y = 48; lines are
8 pixels apart. It is also echoed to the console between two ruled lines, each line centred in 40
columns, and the console's notify lines are cleared. Characters with the high bit keep the
alternate colour **[derived]**.

**The pickup line** **[read]**: there is no separate pickup line; the stock status bar draws it
(Q5: the icon at `xv 0 yb -50`, the item's name at `xv 26 yb -42`), for 3.0 s of level time after
each pickup (Q4). Item pickups also flash the screen briefly; that is the view blend, not the HUD.

**Crosshair** **[read]**: the `crosshair` setting (default 0, saved in the config). 0 draws none,
1, 2 or 3 draws the picture `ch1`, `ch2` or `ch3` (`pics/chN.pcx`); a value above 3 or below 0 is
set to 3. It is centred in the 3D view rectangle (which `viewsize` shrinks), not the screen, by
the picture's own size, and drawn right after the 3D view, under the status bar. When the picture
cannot be found nothing is drawn.

**Two more pictures the client draws itself** **[read]**: `net` at 64 pixels right of the view
rectangle's top-left corner when 63 or more commands are unacknowledged (a stalled connection),
`pause`, centred horizontally with its top 8 pixels below the screen's middle, while the game is
paused and `scr_showpause` is 1 (its default).

## 8. Item icons, numbers and the selection

**The item list** **[read]**: item number 0 is an empty row kept unused; real items are numbered
1 to 41, 42 rows in all. At level load configstring 1056 + n gets item n's pickup name for n from 0
to 41 (1056 is the empty string); 1098..1311 are never set **[derived]**. Stat 8 holds 1056 + the
number, stat 12 the bare number. Names are matched without case. In number order, as classname,
icon picture (pics/ name without .pcx), pickup name:

| # | Classname | Icon | Pickup name |
| --- | --- | --- | --- |
| 1 | item_armor_body | i_bodyarmor | Body Armor |
| 2 | item_armor_combat | i_combatarmor | Combat Armor |
| 3 | item_armor_jacket | i_jacketarmor | Jacket Armor |
| 4 | item_armor_shard | i_jacketarmor | Armor Shard |
| 5 | item_power_screen | i_powerscreen | Power Screen |
| 6 | item_power_shield | i_powershield | Power Shield |
| 7 | weapon_blaster | w_blaster | Blaster |
| 8 | weapon_shotgun | w_shotgun | Shotgun |
| 9 | weapon_supershotgun | w_sshotgun | Super Shotgun |
| 10 | weapon_machinegun | w_machinegun | Machinegun |
| 11 | weapon_chaingun | w_chaingun | Chaingun |
| 12 | ammo_grenades | a_grenades | Grenades |
| 13 | weapon_grenadelauncher | w_glauncher | Grenade Launcher |
| 14 | weapon_rocketlauncher | w_rlauncher | Rocket Launcher |
| 15 | weapon_hyperblaster | w_hyperblaster | HyperBlaster |
| 16 | weapon_railgun | w_railgun | Railgun |
| 17 | weapon_bfg | w_bfg | BFG10K |
| 18 | ammo_shells | a_shells | Shells |
| 19 | ammo_bullets | a_bullets | Bullets |
| 20 | ammo_cells | a_cells | Cells |
| 21 | ammo_rockets | a_rockets | Rockets |
| 22 | ammo_slugs | a_slugs | Slugs |
| 23 | item_quad | p_quad | Quad Damage |
| 24 | item_invulnerability | p_invulnerability | Invulnerability |
| 25 | item_silencer | p_silencer | Silencer |
| 26 | item_breather | p_rebreather | Rebreather |
| 27 | item_enviro | p_envirosuit | Environment Suit |
| 28 | item_ancient_head | i_fixme | Ancient Head |
| 29 | item_adrenaline | p_adrenaline | Adrenaline |
| 30 | item_bandolier | p_bandolier | Bandolier |
| 31 | item_pack | i_pack | Ammo Pack |
| 32 | key_data_cd | k_datacd | Data CD |
| 33 | key_power_cube | k_powercube | Power Cube |
| 34 | key_pyramid | k_pyramid | Pyramid Key |
| 35 | key_data_spinner | k_dataspin | Data Spinner |
| 36 | key_pass | k_security | Security Pass |
| 37 | key_blue_key | k_bluekey | Blue Key |
| 38 | key_red_key | k_redkey | Red Key |
| 39 | key_commander_head | k_comhead | Commander's Head |
| 40 | key_airstrike_target | i_airstrike | Airstrike Marker |
| 41 | (none) | i_health | Health |

(The table is ours, written from the item list's names and order.)

- The four health classnames (item_health_small 2 points, item_health 10, item_health_large 25,
  item_health_mega 100) all spawn item 41, "Health", icon i_health **[read]**.
- The shard has its own name but the jacket armour's icon; it adds 2 to the worn armour, or gives
  2 of jacket armour when none is worn **[read]**.
- The Ancient Head's icon is i_fixme; whether the pak has that picture is not settled by the
  source.
- The hand grenades are the ammo item Grenades, which is also a weapon and is its own ammo
  **[read]**. The Blaster cannot be picked up or dropped; every player holds it **[read]**.
- Stat 4 never shows i_powerscreen: power armour of either kind shows i_powershield (section 4)
  **[derived]**.

**The pickup icon and the ammo icon** **[read]**: stat 7 is always the taken item's own icon
from the table (any health i_health, the shard i_jacketarmor, a weapon its w_ picture, an ammo its
a_ picture); a refused pickup leaves stats 7 and 8 alone. Stat 2 is 0 with the Blaster in hand (no
ammo; stat 3 is 0 too), and a_grenades with the hand grenades (stat 3 the grenade count); both
follow the weapon raised.

**What becomes selected** **[read]**: picking up an item that has a use makes it the selected
item. Usable: Power Screen, Power Shield, every weapon, Grenades, Quad Damage, Invulnerability,
Silencer, Rebreather, Environment Suit. Never selected by a pickup: the armours and the shard, the
other ammo, health, Ancient Head, Adrenaline, Bandolier, Ammo Pack, the keys. A weapon picked up is
selected (a grenade launcher selects itself, not its grenades); a refused pickup (weapons stay or
co-op and already owned, full ammo, a power-up at its skill limit) changes nothing.

**The start and "none"** **[read]**: a fresh player holds only the Blaster and has it selected
(stat 12 is 7, stat 6 the w_blaster image). The fresh start is the game's first start in single
player and co-op, and every respawn and level change in deathmatch; in single player and co-op the
selection carries across levels. "None" is -1: stat 12 is -1 and stat 6 is 0. Stats 6 and 12 are
rewritten every frame from the selection, whatever the count.

**When the selection moves by itself** **[read]**: only on a pickup of a usable item, at a fresh
start, and in a check run after using a power-up (it is used up), after dropping a general item or
an ammo, and before `invuse` and `invdrop`. The check keeps the selection while the player holds at
least one; otherwise it steps forward through the numbers, wrapping, to the next held usable item,
or to -1. `use <name>`, weapon switches and `give` never change it. So **[derived]**:
- The last grenade thrown, the last of an ammo spent or a weapon's last copy dropped leaves the
  selection on that item at count 0 (its icon still in stat 6) until a check runs; in the inventory
  screen that selection has no row and no row is marked.
- Once -1, the check leaves it at -1 (`invuse` prints "No item to use.", `invdrop` "No item to
  drop."); only a usable pickup or a selection command brings one back.
- With instant power-ups in deathmatch (and always for a quad another player dropped), the
  power-up is used during the pickup, and the pickup then selects it at count 0.

**The selection commands** **[read]**: `invnext`/`invprev` step through every held usable item,
`invnextw`/`invprevw` through the weapons and Grenades, `invnextp`/`invprevp` through quad,
invulnerability, silencer, rebreather and environment suit (not the power screen or shield). A step
that finds nothing sets -1, so `invnextp` with no power-up clears the selection even while weapons
are held **[derived]**. A spectator chasing a player changes the chased player with them instead.
