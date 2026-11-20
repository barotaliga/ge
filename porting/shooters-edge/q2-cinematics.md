# Answers: Quake II cinematics (.cin) and the end pictures

Clean-room reader's answers. Facts only, in my own words; no code. Tags:
[read] seen in the source, [derived] worked out from what was read,
[measured] checked in the pak files under the project's paks folder (read only).
"The original" is the 1997 release's client and server; "q2pro" is the later client
where it differs. yquake2 was not needed for any answer.

---

## 1. The .cin file layout

All multi-byte integers in the file are **little-endian, 32-bit**. [read]

### Header (20 bytes, offset 0)

| Offset | Size | Meaning |
| --- | --- | --- |
| 0 | 4 | picture width in pixels |
| 4 | 4 | picture height in pixels |
| 8 | 4 | sound sample rate in Hz (e.g. 22050) |
| 12 | 4 | sound sample width in bytes (1 or 2) |
| 16 | 4 | sound channel count (1 or 2) |

[read] There is no magic number, no frame count, no frame rate field (the rate is
fixed, § 3), and no palette in the header.

Validation: the original checks nothing in the header. q2pro refuses width outside
1..640, height outside 1..480, and, when the rate is non-zero, a rate outside
8000..22050, a sample width outside 1..2 or channels outside 1..2. q2pro treats a rate
of 0 as "no sound" and then reads no sound bytes per frame. [read]

### Huffman count table (65 536 bytes, offset 20)

Immediately after the header: 256 rows of 256 bytes. Row *p* (p = 0..255) is the
table for the context "previous pixel value was p"; byte *s* of that row is the
(unsigned 8-bit) frequency count of pixel value *s* following *p*. The trees are built
from these counts once at open (§ 2). [read]

So the first frame chunk starts at byte offset **65 556**. [derived]

### Frame chunks (repeat until the end marker)

| Field | Size | Meaning |
| --- | --- | --- |
| command | 4 | 0 = a frame; 1 = a palette follows, then a frame; 2 = end of file (nothing follows that is read) |
| palette | 768 (only when command is 1) | 256 entries of R, G, B, one byte each, full 0..255 range (not 6-bit) |
| size | 4 | byte length of the compressed picture block that follows |
| compressed picture | *size* | first 4 bytes: decompressed byte count (little-endian 32-bit); the rest: the Huffman bitstream (§ 2) |
| sound | *n* × width × channels | this frame's sound samples, *n* from § 3 |

[read] Order within a chunk is exactly: command, optional palette, size, picture block,
sound.

- The original rejects *size* < 1 or > 131 072 (0x20000) bytes as a fatal "bad
  compressed frame size" error (drops to the console). q2pro rejects < 4 or > 131 072
  and just ends the cinematic. [read]
- The original treats only command value 2 as the end; q2pro treats any value ≥ 2 as
  the end. Running out of file where a command is expected also ends playback (the
  original retries the 4-byte read once before giving up). [read]
- The palette stays in force for later frames until the next command-1 chunk. Since the
  header carries no palette, the first frame of a well-formed file must be command 1
  [derived]; the original otherwise shows it with an all-zero (black) palette, because
  the client state that holds the palette was cleared on connect [derived].
- Sound sample encoding: 16-bit samples are signed little-endian; channels interleaved
  left then right. 8-bit **mono** is treated as unsigned (128 = silence). 8-bit
  **stereo** is treated as signed by the original's mixer (an inconsistency of the
  original; stock files are 16-bit or 8-bit mono, not checked) [read for the mixer;
  the stock files' formats were not measured].
- The per-frame sound buffer in both clients is 6 300 bytes, i.e. at most 1 575
  samples × 4 bytes: the format as played tops out at 22 050 Hz, 16-bit stereo. [read,
  derived]

The decompressed picture is *width × height* bytes, row-major, top row first, one
palette index per pixel. [read]

---

## 2. The picture compression: order-1 Huffman

A **separate Huffman tree for each previous pixel value** (256 trees), each built from
its row of the count table. The bitstream for one frame is continuous; there is no
per-frame tree data.

### Building one context's tree [read]

Node numbering: nodes 0..255 are the leaves and stand for pixel values 0..255.
Internal nodes are numbered from 256 upward, in the order they are created; at most
255 internal nodes (256..510).

Each node has a count: a leaf's count is its byte from the row; an internal node's
count is the sum of its two children's counts. Each node also has a "used" mark,
initially clear.

Repeat:

1. Among all nodes that exist so far (leaves 0..255 and internal nodes created so
   far), find the one that is not used and has a **non-zero** count, with the
   **smallest** count. Ties go to the **lowest node number** (the scan runs upward and
   only a strictly smaller count replaces the best). Zero-count leaves are never
   picked. If none is found, stop.
2. Mark it used. It becomes **child 0** (taken on a 0 bit) of the next new internal
   node.
3. Find the next such node the same way. If none is found, stop (the half-made node is
   discarded: it is not counted).
4. Mark it used. It becomes **child 1** (taken on a 1 bit).
5. Create the new internal node with number = next free number (256, 257, ...), count
   = sum of the two. Continue until 255 internal nodes exist.

The **root** of the context's tree is the **last internal node created** (the highest
number). [read]

Consequences [derived]:

- Leaves win ties against internal nodes (lower numbers), and among leaves the smaller
  pixel value wins.
- Counts are 8-bit in the table but internal sums can exceed 255; keep them in a wider
  integer.
- A context row with fewer than two non-zero counts creates no internal node; the
  original's "root" for it is then node 255 (a leaf) and decoding through that context
  reads outside the context's node table (undefined in the original). q2pro, for the
  same case, emits pixel value 255 without consuming any bits. A decoder should refuse
  or define this case; a well-formed file never reaches such a context with pixels
  still to decode.

### Decoding a frame [read]

1. Read the first 4 bytes of the picture block: the number of pixels to produce
   (little-endian). It should equal width × height. (q2pro ignores this field and
   always decodes width × height; the original allocates and decodes exactly this
   count.)
2. The bitstream starts at byte 4 of the block. **Bits are taken from each byte least
   significant bit first** (bit 0, then bit 1, ... bit 7), then the next byte. Symbols
   are not byte-aligned: a code may span a byte boundary.
3. The context at the start of every frame is **previous value = 0**.
4. For each pixel: start at the root of the current context's tree; take one bit at a
   time, going to child 0 on a 0 bit and child 1 on a 1 bit, until a leaf (node number
   < 256) is reached. The leaf's number is the pixel value. Output it; it becomes the
   context for the next pixel.
5. Stop after the required number of pixels. Leftover bits in the last byte are
   ignored.

End-of-block check: the original reads input a byte at a time ahead of need and may
fetch one byte past the block when the last code ends exactly on a byte boundary; it
accepts a consumed length equal to the block size or the block size + 1 and only
prints a warning otherwise ("overread"). q2pro fails the frame (ends the cinematic) if
a bit is needed past the block's end. A decoder that fetches bytes only when a bit is
needed never needs that extra byte. [read, derived]

Worked micro-example [derived]: a row with counts A=1, B=1, C=2 (all others 0) gives:
node 256 = (A, B) with count 2; then the unused non-zero nodes are C (2) and 256 (2):
C is the lower number, so node 257 = (C, 256). Root 257. Codes, written in stream
order: C = 0, A = 1 0, B = 1 1.

---

## 3. Frame rate, sound per frame, keeping in step

- **14 frames per second**, fixed by the client, not stored in the file. [read]
- Sound samples for frame *k* (k counted from 0): start = floor(k × rate / 14),
  end = floor((k + 1) × rate / 14), count = end − start (integer arithmetic). This
  carries the remainder: at 22 050 Hz every frame has exactly 1 575 samples; at
  11 025 Hz frames alternate 787 and 788 samples. The count multiplies by width ×
  channels for the bytes to read. [read, derived]
- Sound path: the client changes its sound device rate to the file's rate in kHz
  (rate/1000, so 22 for 22 050) and restarts sound if that differs from the current
  setting, and restores it when the cinematic stops. Each frame's samples are appended
  to a raw-sample stream the mixer plays (rate converted by sample skipping/repeating
  if the device rate differs; 16-bit kept, 8-bit widened). CD music is stopped when a
  cinematic starts. [read]
- Pacing: the picture runs on the wall clock. The start time is taken after frame 0 is
  read. The wanted frame number = (now − start) × 14 / 1000. When it passes the current
  frame, the client advances by **one** frame per screen update, reads the next chunk
  (queuing its sound at that moment). If it has fallen behind by more than one frame it
  prints "Dropped frame" and moves the start time so that no frame is skipped: the
  video slows down rather than dropping pictures. There is no other synchronisation of
  picture to sound: sound is queued as each frame is read and plays out at the device
  rate. [read]
- Pause: while the console or the menu is open the clock is held (the start time is
  re-based every update), and playback resumes where it was. [read]
- One-frame display lag in the original [derived]: the client keeps a "current" and a
  "pending" picture. At start it reads frame 0 straight into current and shows it. On
  each advance, pending becomes current and the next frame is read into pending. So at
  the first advance current becomes empty (nothing new is drawn for 1/14 s), and from
  then on frame *k* is shown one frame period after its sound was queued (≈71 ms
  picture behind sound); when the end marker is read, the last frame (still pending) is
  discarded unseen. q2pro has no pending slot: it shows each frame as soon as it is read
  (sound and picture together, and the last frame is shown).

---

## 4. How the client shows it

- **Scaling** (original): the picture is stretched to **the whole screen**, ignoring
  aspect ratio; no letterboxing and no black bars are added. The software renderer
  stretches with nearest-pixel sampling. The GL renderer first resamples the frame into
  a 256 × 256 texture (each row nearest-sampled to 256 columns at column centres; if the
  picture has more than 256 rows, rows are nearest-sampled down to 256, otherwise the
  extra texture rows are unused and the texture coordinates cover only the used part),
  then draws it over the whole screen with **bilinear** filtering. [read]
- **Palette** (original): the 768-byte palette is applied as-is (8-bit components) for
  the cinematic. The software renderer clears the screen to black first (to avoid a
  palette flash) and loads the palette through its gamma table; the GL renderer builds a
  256-entry RGBA table from it for the texture conversion and clears the screen. The
  game's own palette is restored when the cinematic stops or when the menu/console is
  drawn over it. A palette change in the stream takes effect from the next screen draw.
  [read]
- **Menu/console over a cinematic** (original): menu up: playback pauses, the game
  palette returns, the screen is filled black behind the menu. Console up: pauses,
  the console is drawn. [read]
- **q2pro** draws differently: the screen is filled black, the picture is scaled
  **uniformly** to fit (the smaller of the width and height scales) and centred, so a
  4:3 picture is pillarboxed on a wide screen. It also knows a fixed table of stock
  files (matched by name **and exact file size**) whose frames carry black letterbox
  bars, and crops those rows when fitting: ntro.cin (82 836 235 bytes) from frame 727
  on, 30 rows top and bottom; end.cin (19 311 290 bytes) from frame 0, 30 rows; and the
  mission packs' rintro.cin / rend.cin (24 rows) and xin.cin / xout.cin (32 rows).
  Pixels are converted through the palette to RGBA on the CPU. q2pro can also play other
  video formats through FFmpeg. [read]
- **Skipping** (original): skipping is driven by the movement command the client sends
  every frame. Any key or mouse button held down while the game (not the menu or
  console) has the keyboard sets the "any button" bit, and if that bit is set **more
  than 1 second after the cinematic started** and the server is not a demo/attract loop,
  the client sends the skip (§ 6). It is sent again on every command while the key is
  held; the server ignores the repeats. Escape is not a skip: it opens the menu (and
  pauses). The console key opens the console. During an attract loop (the title demo
  cycle) any key opens the menu instead. [read]
- **Skipping** (q2pro): on the key-down of any key whose binding is a "+" button
  command (e.g. +attack, +forward), immediately, with no 1-second guard; unbound keys
  and non-button bindings do not skip. [read]
- **At the end** (original): when the end marker (or end of file) is read: the
  cinematic stops (file closed, pictures freed, game palette back, sound rate
  restored), the client sends the "next server" command (§ 6), and a loading plaque is
  shown over a black screen until the server's next level arrives. On a skip, the
  original only sends the command; the picture keeps playing until the server's level
  change disconnects it. q2pro stops the cinematic and shows the loading plaque on a
  skip as well. If the .cin file cannot be opened, the client sends the "next server"
  command at once (the cinematic is silently skipped). [read]

---

## 5. A .pcx shown as a "cinematic" (victory.pcx)

- When the server's level name ends in ".pcx", the client loads it from
  `pics/<name>` and shows it as a still. [read]
- The original accepts only: manufacturer byte 0x0A, version 5, encoding 1 (RLE), 8
  bits per pixel, and xmax < 640, ymax < 480. Width = xmax + 1, height = ymax + 1. RLE:
  a byte with both top bits set (≥ 0xC0) gives a run length in its low 6 bits and the
  next byte is the value; any other byte is a single pixel. Runs fill rows of exactly
  width pixels (the bytes-per-line field is not used). The palette is taken as **the
  last 768 bytes of the file** (the 0x0C marker before it is not checked). A missing or
  bad file prints "not found" and shows nothing. [read]
- It is drawn exactly like a cinematic frame: stretched over the whole screen with its
  own palette (q2pro: fitted uniformly and centred). [read]
- It has **no timer**: it stays until the server changes level. In **single player the
  server refuses the "next server" request for a picture**, so a key press does
  nothing: the victory picture stays until the player uses the menu (new game, load,
  quit). In **co-op** the server forces the next server to be a new game from base1
  (unit-start form, wiping the current save), so any key (after the usual
  more-than-1-second rule, always true here) restarts the campaign. q2pro does the same
  with its own spelling of that command. [read]
- [measured] victory.pcx is in pak0 of both the full and the demo release, 84 193 bytes:
  manufacturer 10, version 5, encoding 1, 8 bpp, window 0..319 × 0..239 (320 × 240),
  one plane, 320 bytes per line; the byte just before the last 768 is 12 (the palette
  marker).

---

## 6. The server–client hand-off

### Level strings [read]

A level change is a string of the form `[*]name[$spawnpoint][+next]`:

- `+next`: everything after the first `+` becomes the server's pending "next server"
  command, `gamemap "<next>"`. Without a `+` the pending command is cleared.
- `$spot`: the spawn point name for a map.
- A leading `*` marks the start of a new unit: the current savegame's per-level files
  are wiped (no going back across units); it is stripped before loading.
- The name's extension picks the server mode: `.cin` cinematic, `.pcx` picture, `.dm2`
  demo, anything else a map (`maps/<name>.bsp`).

### End of a unit, in single player [read]

1. The player touches a level exit whose target string is, e.g.,
   `eou1_.cin+*bunk1$start`.
2. The game starts an intermission. If the target string contains a `*` (a new unit
   follows, which is the case for every end-of-unit string because the part after `+`
   starts with `*`), single player gets a real intermission: dead players respawn, the
   view moves to one of up to four intermission camera spots chosen at random (falling
   back to the player start), the player is frozen, and the level can be left only
   **after 5 seconds AND a button press** (any key). Co-op strips all keys from the
   players. Without a `*` (an ordinary level change, and the final `end.cin+victory.pcx`
   exit) single player leaves at the next server frame with no intermission.
3. On leaving, the game issues `gamemap "<target string>"`. The server saves the level
   being left (its first character is not `*`), then splits the string: the pending next
   server becomes `gamemap "*bunk1$start"`, and the server respawns as a **cinematic
   server** named `eou1_.cin` (no world loaded). It autosaves into the save0 slot (so a
   saved game can resume at the cinematic).
4. Connected clients get "changing" then "reconnect". Each reconnecting client asks for
   the new level and receives the **server data** message: protocol number (34, 32-bit),
   the spawn count (32-bit), the attract-loop flag (byte), the game directory (string),
   the player number (16-bit, **−1** for a cinematic or picture), and the level name
   string (here `eou1_.cin`). For a cinematic or picture nothing else follows (no
   config strings, no baselines). The client stores the spawn count, and on a player
   number of −1 starts the cinematic (or picture) named by the level string.
5. When the cinematic ends or is skipped, the client sends the reliable string command
   **`nextserver <spawn count>`** using the spawn count from the server data.
6. The server ignores it if the number is not its current spawn count (left over from
   an earlier level), if it is running a normal game, or if it is showing a picture
   outside co-op. Otherwise it bumps the spawn count (so a second request cannot sneak
   in), runs the pending next-server command (here `gamemap "*bunk1$start"`, which wipes
   the unit's saves and loads bunk1 at spawn `start`) and clears it. With no pending
   command it shuts the server down (`killserver`).

q2pro's server adds a cvar (`sv_cinematics`, default on) that skips cinematics in a
chain when off, skips straight on to the next server when a cinematic file does not
exist, and tracks a pending flag instead of the spawn-count bump. [read]

### New game and the title loop [read]

- The menu's new game runs: show loading, kill the server, wait, then the `newgame`
  alias from default.cfg: kill the server, one client, deathmatch off,
  `map *ntro.cin+base1` (the intro, then base1 as a new unit).
- `demomap` starts a server in attract-loop mode (the title cycle). The full release's
  aliases cycle `idlog.cin` (the logo movie), demo1.dm2, `idlog.cin`, demo2.dm2, each
  with the next as its next server. In attract mode the client cannot skip; a key opens
  the menu.

---

## 7. The stock files

The .cin files are **not inside any pak** in either release here [measured: neither
full nor demo pak0/pak1/pak2 lists a `video/` entry]; on the retail CD they are loose
files under `baseq2/video/`. Which files exist is therefore derived from the strings
that name them [measured from the maps' entity strings and default.cfg in the paks]:

| File | Where it plays | Then |
| --- | --- | --- |
| idlog.cin | Title attract loop (d1 and d3 of the demo cycle) | the next demo in the loop |
| ntro.cin | New game | `*base1` (unit 1) |
| eou1_.cin | Leaving base2 (end of unit 1) | `*bunk1$start` |
| eou2_.cin | Leaving ware2 (end of unit 2) | `*jail1` |
| eou3_.cin | Leaving security (end of unit 3) | `*mintro$start` |
| eou4_.cin | Leaving mine3 (end of unit 4) | `*fact1$start` |
| eou5_.cin | Leaving fact1 (end of unit 5) | `*power1$pstart` |
| eou6_.cin | Leaving power2 (end of unit 6) | `*biggun$bstart` |
| eou7_.cin | Leaving biggun (end of unit 7) | `*hangar1$unitstart` |
| eou8_.cin | Leaving strike (end of unit 8) | `*city1$unitstart` |
| (none) | Leaving city3 (end of unit 9) | `*boss1$bosstart` (no movie before the last unit) |
| end.cin | Leaving boss2 (end of the game) | `victory.pcx` (the still, § 5) |

- Exact names: the end-of-unit files have an underscore before the extension
  (`eou1_.cin` .. `eou8_.cin`). [measured]
- Sizes known from q2pro's crop table: ntro.cin is 82 836 235 bytes, end.cin is
  19 311 290 bytes. Frame sizes and sound formats of the stock files were not measured
  (no .cin is available here). [read]
- The original server source's comments mention an `inter.cin`; no stock map or config
  references it. [read, measured]
- Demo release [measured]: the demo's pak0 default.cfg has `newgame` =
  `map *ntro.cin+demo1` and an attract loop of only `q2demo1.dm2` (no idlog.cin). Its
  maps reach `victory.pcx` straight from demo2 with no end movie (exit string
  `victory.pcx`, no `*`, so no intermission in single player). victory.pcx is in the
  demo's pak0 too. The demo folder here also has a pak1 identical in size to the full
  release's pak1 whose default.cfg restores the full game's aliases (`base1`, idlog.cin);
  since a higher-numbered pak is searched first, that one is the default.cfg in effect
  for this folder as laid out.
