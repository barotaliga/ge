# Answers: Quake II server (Q2PRO upstream, commit 601a8df): downloads, rcon answers, `nextserver`

Reader's notes. These are a clean-room reader's facts about the Q2PRO server at upstream commit
601a8df, written in the reader's own words from the server sources (`src/server`, with
`src/common` for shared pieces). They contain no code. The only identifiers quoted are setting
and command names, and the only exact strings are texts a player, an rcon user or the console
sees. **[read]** = read directly in the source; **[derived]** = inferred from what was read.
"Console" means the server's own output (terminal and log file). A "developer line" is printed
only when the `developer` setting is 1 or more, and that setting exists only in debug builds.
So in a normal release build these lines are never printed **[read]**.

Protocol numbers: 34 = stock, 35 = the R1Q2 extension, 36 = Q2PRO's own **[read]**.

---

## A. Which files a client may download

### 1. The `download <file> [offset]` client command, check by check

Client commands named `download`, `nextdl` and `stopdl` are handled by the server itself. They are
never passed to the game module, and they are accepted in any connection state, including before
the client has spawned and while a picture or cinematic level is up **[read]**.

**Order of the checks** **[read]** unless marked:

1. **Raw length.** The first argument is copied into a 64-byte buffer. If it is 64 characters or
   longer, the request is refused. So a name of 63 characters or fewer passes this check. This
   check uses the name as typed, before it is normalized.
2. **The `http` word.** If the argument is exactly `http`, the server only marks the client as
   "downloading over HTTP" for the `status` listing. Nothing is sent back and nothing else
   happens. It is not a refusal.
3. **Normalization.** The name is normalized in place. Backslashes become forward slashes.
   Repeated slashes collapse to one. Leading slashes are dropped. `.` segments are removed.
   A `..` segment removes the previous segment, and it cannot climb above the root: a leading
   `../` is simply dropped. A trailing slash is dropped.
   - Consequence: a leading `/` is **not** refused. `/maps/x.bsp` becomes `maps/x.bsp` and is
     then judged like that name. `maps/../pics/x.pcx` becomes `pics/x.pcx`. **[derived]**
4. **Offset.** If there is a second argument, it is read as a decimal integer (the usual lenient
   conversion, so garbage reads as 0).
5. **The global gate.** The request is refused when any of these is true:
   - `allow_download` is 0;
   - the normalized name is empty;
   - the offset is negative;
   - the normalized name still contains `..` anywhere (after normalization this can only be a run
     of dots inside a segment, such as `a..b`) **[derived]**;
   - the first character is not a letter, a digit, `_` or `-`;
   - the last character is not a letter, a digit, `_` or `-` (so a name ending in `.` or `/` is
     refused, and a name must end in an extension character or a plain letter or digit);
   - there is no `/` in the name: the file **must be in a subdirectory**.
   - Developer line: `Refusing download of <name> to <player>` + newline.
6. **The category gate.** The first path segment picks the setting. The prefix test is
   **case-insensitive**:
   - `players/` → `allow_download_players`
   - `models/` or `sprites/` → `allow_download_models`
   - `sound/` → `allow_download_sounds`
   - `maps/` → `allow_download_maps`
   - `textures/` or `env/` → `allow_download_textures`
   - `pics/` → `allow_download_pics`
   - anything else → `allow_download_others`. That covers every other top-level directory
     (for example `demos/`, `video/`, `scripts/`, `music/`, `config/`, a mod's own folders).
     Bare top-level files are already refused by step 5, so "others" never sees one **[derived]**.
   - If the chosen setting is 0: refused. Developer line: the same
     `Refusing download of <name> to <player>` + newline.
7. **A download already in progress** is closed silently and the new request goes on. There is
   only a developer line: `Closing existing download for <player> (should not happen)` + newline.
8. **Opening the file.** The file is looked up through the normal search path: loose files in the
   game directories, `.pak` packs and zip packs (`.pkz`) alike.
   - **There is no pack check.** Files inside packs are served for maps and everything else.
     Nothing refuses a map because it came from a pack.
   - There is **no extension check** of any kind (no list of allowed or forbidden extensions).
   - Drive letters: there is no explicit check. On Windows, a `:` (and `< > " | ? *`) makes a
     path invalid for the loose-file lookup, so such a name is never found on disk. It could
     still match a pack entry of that exact name. On other systems `:` is an ordinary character
     **[read]**. Names that are not printable characters are invalid for the disk lookup on
     every system **[read]**.
   - Compressed variant: see 2 below.
   - If the file is not found: refused. Developer line: `Couldn't download <name> to <player>`
     + newline.
9. **Empty file** (size 0): refused. Developer line: `Refusing empty download of <name> to
   <player>` + newline.
10. **Size limit** (see 2): refused. Developer line: `Refusing oversize download of <name> to
    <player>` + newline.
11. **Offset past the end** (see 3).
12. **Offset exactly at the end.** This is **not** a refusal. The client is sent a download
    message with a piece size of **0** and a percent of **100**, as a reliable message. No file
    data follows. Developer line: `Refusing download, <player> already has <name> (<n> bytes)` +
    newline.
13. **Read failure.** The whole file is read into memory at once. If the read comes up short:
    refused. Developer line: `Couldn't download <name> to <player>` + newline.
14. **Accepted.** The whole file stays in memory. The position starts at the offset and the first
    piece is marked as due (see 4). Developer line: `Downloading <name> to <player>` + newline.

**What a refusal sends** **[read]**: every refusal above (steps 1, 5, 6, 8, 9, 10, 11, 13) sends
the client one download message with **size -1** and **percent 0**. It is queued as a reliable
message. No print text is sent, **except** in case 11, where a print comes first (see 3).
Nothing is printed on the console in a release build. Only the developer lines exist.

`stopdl` (with a download in progress) **[read]**: the client is sent a download message with size
-1 and the current percent (bytes sent × 100 / size, truncated). The download is then dropped.
Developer line: `Download of <name> to <player> stopped by user request` + newline. Without a
download in progress, `stopdl` and `nextdl` do nothing.

A download in progress is also dropped when the client's slot is cleaned up (disconnect or drop)
**[read]**.

### 2. `sv_max_download_size`

- Default **8388608**, in **bytes** (8 MiB). Flags: none (not archived, not latched). It is read
  again on every request **[read]**.
- The effective limit **[read]**:
  - value > 0: the limit is that value, but no more than the engine's hard file cap of
    134,221,824 bytes (128 MiB plus 4 KiB). A larger value is **written back** to the setting,
    clamped to that cap.
  - value 0 **or negative**: the limit is that hard cap of 134,221,824 bytes. The server's
    manual says 0 "disables the limit", but in the code it falls back to the hard cap.
- A file whose size is greater than the limit is refused. The client gets the plain refusal:
  download message, size -1, percent 0, with **no print text**. The only line is the developer
  line from step 10. A file exactly at the limit is allowed **[read]**.
- **Which size is compared** **[read]**: the size of the stream that will be sent.
  - Normally that is the file's size.
  - For a protocol 36 client whose minor version is 1021 or more, who said it supports zlib, and
    who asked from offset 0: the server first tries to open the file as a raw deflate stream. That
    works only when the file sits **compressed inside a zip pack** (`.pkz`). Then the
    **compressed** size is compared to the limit and sent, and the pieces use the compressed
    download message.
  - If the file is not compressed in a zip (loose file, `.pak`, stored entry), it falls back to
    the plain file and its uncompressed size.
  - Protocols 34 and 35 always get the uncompressed file.

### 3. Resume offset past the file's size

- Sent to the asking client as a print at the **high** print level (only if the client's
  `msg` level lets high prints through, which it does by default) **[read]**. The exact text is
  one message holding two lines, each ending in a newline:
  `File size differs from server.` + newline + `Please delete the corresponding .tmp file from your system.` + newline
- That print is queued reliably first. Then the normal refusal follows: a download message with
  size -1 and percent 0 **[read]**.
- Developer line on the console: `Refusing download, <player> has wrong version of <name> (<offset> > <size>)` + newline **[read]**.
- Afterwards the server **does not restart from 0**. The request is refused and no download state
  is kept. A new `download` request with a smaller offset (or none) would be judged afresh
  **[read]**.
- With the compressed path, an offset other than 0 always uses the uncompressed file, so the
  offset is compared to the uncompressed size **[read]**.

### 4. The pace

**[read]** unless marked:
- **Where pieces are sent.** Pieces go out only from the server's "between frames" sender, the
  one that serves clients that are not in the game yet. A spawned client with a download in
  progress counts as **not active**. While it downloads it gets **no game frames**: it is
  served by the same between-frames sender, at the pace its `rate` allows. After the last piece,
  normal frames resume.
- **One piece per request.** Accepting a `download` marks one piece as due. Each piece clears
  that mark. **`nextdl` marks the next piece as due**, so **`nextdl` is required for every piece
  after the first**, for every protocol. Without it the download stalls (it is not dropped).
- **A piece goes into the reliable stream.** It is written only when no earlier reliable data is
  still unacknowledged. So at most one piece is in flight per round trip, gated again by
  `nextdl` **[derived]**.
- **Piece size, for every protocol alike.** The piece fills the remaining room in the packet: the
  connection's maximum packet length, minus what is already queued in the reliable buffer, minus
  4 bytes (the message byte, a 2-byte length and the percent byte). It is capped by the bytes
  left. No piece is written if fewer than 4 bytes of room remain.
  - **Stock 34 clients:** the maximum packet length is fixed at 1390 (1400 minus a 10-byte
    header). So a piece is **at most 1386 bytes** **[derived]**.
  - **35 and 36 clients:** the length comes from the client's connect request. If the request
    leaves it out, it is 1390. A 0 means "the most allowed", 4086. The value is capped by the
    server's `net_maxmsglen` (default 1390) for any client that is not local, and it is never
    below 512. On a default dedicated server a remote 35 or 36 client therefore also gets
    pieces of at most 1386 bytes **[derived]**. One later step can only lower this limit: when the
    server learns of a smaller path MTU it may cut a 36 client's length while that client is
    primed.
- **Percent field.** One byte: the bytes sent so far **including this piece** × 100 / total size,
  truncated toward zero. The last piece reads 100.
- When the last piece is written, the download is closed on the server and normal frames resume.

### 5. The settings' flags and defaults

**[read]**:

| Setting | Default | Flags |
| --- | --- | --- |
| `allow_download` | 0 on a dedicated server, 1 otherwise (see the caveat) | archived |
| `allow_download_players` | 1 | archived |
| `allow_download_models` | 1 | archived |
| `allow_download_sounds` | 1 | archived |
| `allow_download_maps` | 1 | archived |
| `allow_download_textures` | 1 | archived |
| `allow_download_pics` | 1 | archived |
| `allow_download_others` | 0 | **none** (not archived) |
| `sv_max_download_size` | 8388608 | none |
| `sv_downloadserver` | empty | none |
| `sv_cinematics` | 1 | none |

- None of them is latched. Every one is read at the moment of each request, so a change takes
  effect on the next `download` command **[read]**.
- **`allow_download` = 0 on a dedicated server: confirmed for the dedicated-only build. "At start
  only" in this sense:** the default is chosen once, when the setting is created at engine
  start, from whether the engine counts as dedicated at that moment **[read]**.
  - In the **dedicated-only build** the engine is always dedicated, so the default is **0**.
  - In a **full (client) build started with `+set dedicated 1`**, the setting is created
    **before** the command line's `+set` commands are applied. At that moment `dedicated` is
    still 0, so the default is **1** **[derived]** (from the order of start-up: the setting is
    created, then the early `+set` commands run).
  - A config file or a `+set allow_download …` on the command line overrides either default.
    Because the setting is archived, a saved config may also carry it.

---

## B. HTTP downloads

### 6. `sv_downloadserver`

- **Value format.** Free text, empty by default, no flags. The code does **no** check and **no**
  rewrite: no scheme check, no trailing-slash check, nothing is appended. The server's manual
  says it "must begin with a 'http://' or 'https://' prefix and should typically end with a
  slash", but that is advice and nothing enforces it **[read]**.
- **Where it reaches the client.** It is sent only in the connect answer (the connectionless
  `client_connect` packet). When the value is non-empty, the text ` dlserver=` (with its leading
  space) followed by the raw value is inserted. The answer's full shape, in order, is
  `client_connect`, then ` nc=0` or ` nc=1` (protocol 36 only), then the anticheat word (only in
  builds and setups that use it), then ` dlserver=<value>`, then ` map=<mapname>`. There is
  **no newline** at the end **[read]**.
- **Which protocols get it.** All of them: 34, 35 and 36 get the key alike. There is no protocol
  test around it **[read]**. A stock client that does not know the key ignores it **[derived]**.
- **Limits.** A space inside the value would break the client's word splitting, and nothing
  guards against it **[derived]**. If the whole answer is longer than the 1396-byte limit for a
  connectionless packet, the answer is **not sent at all**. The console then gets a warning that
  names the out-of-band sender and the word `overflow`, and the client does not connect
  **[read]**.
- **Nowhere else.** It is not in the server info, not in a configstring, and not stuffed to the
  client. The connect answer is its only use **[read]**.

---

## C. rcon answers

### 7. How a long rcon answer is cut

**[read]** unless marked:
- While the command runs, every print the server makes is captured into an answer buffer of
  **1384 bytes** (1400 minus 16). None of it goes to the console or the log.
- Each packet sent back is four 0xFF bytes, then `print` + newline, then up to 1384 bytes of the
  captured text. So every packet starts with `print\n`.
- **Where cuts fall.** Before a print is added, the server checks whether it fits in the room
  left. If it does not fit, the buffer collected so far is sent as one packet and the print
  starts a new one. So a cut normally falls **between two print calls**. A line built from
  several prints can therefore be cut in its middle **[derived]**. A single print longer than
  1384 bytes is split into 1384-byte slices with no regard for lines. One print is at most
  4095 characters.
- **Order.** Packets go out in the order the text was produced, as each buffer fills. The last
  part goes out when the command finishes. There is no sequence number.
- **Empty answer.** When the command finishes, the buffer is **always** flushed, even when it is
  empty. A command that prints nothing therefore sends **one packet holding only `print\n`**.
- Debug builds only: developer lines printed while the command runs are captured into the answer
  too.
- The incoming rcon line is read up to 1024 characters. A longer packet is ignored with only a
  developer line.

### 8. Bad password, limited rcon and the rate limit

**[read]**:
- **Order of checks:** first the rate limit, then the password, then (for the limited password)
  the command's prefix.
- **Rate limit:** `sv_rcon_limit`, default `1`. The syntax is
  `<count>[/<period>[s|m|h]][*<burst>]`. The default means 1 per second with a burst of
  **5**. A count of 0 means unlimited.
  - **Every** rcon packet is charged before the password is checked, so a correct password is
    also dropped while the limit is exhausted.
  - A packet with a correct password (full or limited) has its charge **refunded** at once. In
    effect only bad or blocked attempts use up the allowance.
  - A packet over the limit gets **no answer at all**. There is no text and only a developer
    line: `Dropping rcon from <address>` + newline.
- **Bad password** (no match with `rcon_password` or `lrcon_password`). The comparison is
  case-sensitive and exact. An empty password setting never matches.
  - Sent back as one connectionless packet: four 0xFF bytes, then
    `print` + newline + `Bad rcon_password.` + newline.
  - Console line at the normal print level: `Invalid rcon from <address>:` + newline +
    `<the command text>` + newline. `<the command text>` is the raw rest of the line after the
    password.
- **Limited password** (`lrcon_password`) with a command that matches none of the allowed
  prefixes:
  - Sent back: `print` + newline + `This command is not permitted.` + newline.
  - Console: `Invalid limited rcon from <address>:` + newline + `<command>` + newline.
- **Accepted:**
  - Console line at the **notice** level (cyan): `Rcon from <address>:` + newline + `<command>` +
    newline, or `Limited rcon from <address>:` + newline + `<command>` + newline.
  - With the full password, the command text is tokenized again **with macro expansion**. With
    the limited password, the original words are used and **no macros are expanded**.
- rcon is answered even when no map is running.

---

## D. `nextserver` and cinematic or picture levels

### 9. The `nextserver` client word

**[read]** unless marked:
- The word is handled by the server itself and never reaches the game. It is accepted only when
  all of these hold:
  - the server is showing a **cinematic** level, or a **picture** level with `coop` non-zero.
    During a normal game, a demo, or a picture level in deathmatch the word is ignored silently.
  - its first argument equals the current level's spawn count. An old one from a previous level
    is ignored silently.
  - no `nextserver` has been taken yet on this level. Only the first one counts; later ones are
    ignored.
- **What it runs.** If the `nextserver` setting is non-empty, its text plus a newline is added to
  the command buffer. If it is empty, `killserver` plus a newline is added instead. Either way,
  the `nextserver` setting is then **cleared to empty**.
- **The setting's value** is set when a level starts (it is never registered with flags of its
  own):
  - a level string `a+b` that loads `a` sets it to `gamemap "!b"`;
  - a level string with no `+` sets it to empty;
  - in coop, loading `victory.pcx` sets it to `gamemap "!*base1"`.
  The `!` means "if this level cannot be loaded, run `killserver`".
- **Empty setting → `killserver`.** That stops the **server** (the map), not the process. Every
  client is sent the shutdown text `Server was killed.` + newline and is disconnected. The
  dedicated process stays up and keeps listening for rcon **[read]** (the process staying up is
  **[derived]** from `killserver` being a different command from `quit`). Console output is that
  of the normal server shutdown (see the earlier shutdown facts). Nothing extra is printed for
  `nextserver` itself.
- On a dedicated server, by default `deathmatch` is forced on unless `coop` is set. So a picture
  level there never advances by `nextserver`: it stays up until an admin changes the map
  **[derived]**. Clients on a picture or cinematic level cannot `begin`, so they sit at the
  picture or cinematic without entering a game **[read]**.

### 10. `sv_cinematics` and a `map` naming `.cin`, `.pcx` or `.dm2`

**[read]** unless marked:
- The level kind comes from the extension, compared case-insensitively: `.pcx` is a picture,
  `.cin` a cinematic, `.dm2` a demo, anything else a normal map. The level string may carry
  `*`, `$<spawnpoint>` and `+<next>` parts. A string of 64 or more characters prints
  `Refusing to process oversize level string.` + newline.
- **`sv_cinematics`** (default 1) gates only levels that have a `+` remainder after them:
  - with 0, a `.cin` with a remainder is skipped and the remainder is tried instead;
  - a `.dm2` with a remainder is skipped when `sv_cinematics` is 0 **or** when the server has
    more than one client slot.
  - A `.cin` or `.dm2` with **no** remainder is never skipped by it.
  - Skipping prints nothing.
  - If every part fails and the string began with `!`, `killserver` is queued. If it did not,
    nothing more happens.
- **`.pcx` on a dedicated server.** The file is **not checked** (it counts as present). The
  server spawns a "picture" level: no world map, an empty entity string, map checksum "0". The
  game module still gets its spawn call and two frames. The console shows, each ending in a
  newline, `------- Server Initialization -------`, `SpawnServer: <name>.pcx`, any lines from the
  game module (the stock game prints its own spawn lines), then
  `-------------------------------------`. Clients are told `changing map=<name>` and then
  `reconnect`. They get server data marking the level as having no player slot and the picture
  name as the level name.
- **`.cin` on a dedicated server.** The same as `.pcx`: no file check, the same three console
  lines, the state "cinematic".
  - A 36 client with minor version 1023 or more is told the state is cinematic.
  - An older 36 client is told it is a picture.
  - 34 and 35 clients get no state byte; they work it out from the name **[derived]**.
- **`.dm2` on a dedicated server.** It is **refused** (demos play only in a local client). The
  console line is `Couldn't load demos/<name>.dm2: <error>` + newline. `<error>` is the system's
  text for "function not implemented", which on Linux reads `Function not implemented`
  **[derived]** for the exact wording, which depends on the C library. The map does not change,
  unless a `+` remainder exists, in which case the remainder is tried.
- **A normal map that fails to load** prints `Couldn't load maps/<name>.bsp: <error>` + newline
  in the same way.

---

## Open points

- `allow_download`'s default in a full build started dedicated: the order of start-up says 1
  (see 5), but this was not run. Worth a black-box check: start the full build with
  `+set dedicated 1`, no config, and ask `allow_download`.
- The piece size of 1386 bytes assumes an empty reliable buffer when the piece is written.
  Reliable messages queued for a client that is not yet in the game can be merged into the same
  buffer first, and that makes a piece smaller. The exact split under load was not traced.
- How a 34 or 35 client tells a cinematic from a picture level, and which print level the client
  uses to show the `File size differs…` text, are client-side matters. They were not read.
- What the stock game module prints while it spawns a picture or cinematic level depends on the
  game library, not on the server. It was not read.
- `strerror` wording for the `.dm2` refusal on Windows and macOS was not checked.
