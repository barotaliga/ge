# Answers: Quake II client (Q2PRO, commit 601a8df): what joining fetches, server-pushed commands, pause

Read from Q2PRO at commit 601a8df (GPL-2.0), its client, server, shared code and its own player documentation. Each fact is marked **read** (stated by the source or its documentation), **derived** (follows from facts read, not stated in one place) or **open** (the source does not settle it). Nothing here was measured by running the program.

Configstring indices below use the classic layout (protocol 34): the sky name is index 2, models 32 to 287 (slot 1 of the models range is the map), sounds 288 to 543, pictures ("images") 544 to 799, player looks ("playerskins") 1312 to 1567. **read.** With the extended layout (the client's newer protocol flag) the ranges move (models begin at index 62) and grow to 8192 models, 2048 sounds and 2048 pictures, but the sky is still index 2 and every rule below is the same. **read.**

## A. What joining fetches, in order

### 1. The stages, in order

The whole check is triggered by the server's pushed `precache <spawncount>` command (see 12). **read.** It runs in four stages, each falling through to the next unless it has to wait:

**Before any stage (read):** if `allow_download` is 0 or below, or the server's address is a local one (the client's own listen server or loopback), nothing is fetched: the client loads the map and goes straight to `begin`.

**Stage 1, the map and the models (read).**
- The map: the name in models slot 1, used exactly as written (a server writes it as `maps/<name>.bsp`). Gated by `allow_download_maps`. It is queued as the "map" kind.
- Every model name from models slot 2 upward, as written (for example `models/items/armor/body/tris.md2`, `sprites/s_explod.sp2`). The walk stops at the first empty slot, except that an empty slot at index 255 of the range (the slot kept for the player model) is stepped over instead of ending the walk. Names starting with `*` (the map's inline brush models) and `#` (view-weapon names, see 5) are skipped. Gated by `allow_download_models`. Queued as the "model" kind.

**Stage 2, everything that does not need the map (read).** Only when `allow_download_models` is on, the stage first waits until no download of the "model" kind is still pending (developer print: `waiting for models...`), and then:
- the skins named inside each model of stage 1 (see 2), same walk and same skips as stage 1,
- with the extended layout only, every picture configstring that contains a `/` and has an extension (a full path such as a custom skin) is fetched as written, under the models gate,
then, each under its own gate:
- sounds (`allow_download_sounds`), see 3,
- pictures (`allow_download_pics`), see 4,
- figures (`allow_download_players`), see 5,
- the sky (`allow_download_textures`), see 6.

**Stage 3, the map's textures (read).** Waits until no "map" download is pending (developer print: `waiting for map...`). Then loads the map file. If its checksum differs from the server's map-checksum configstring the client is dropped with `Local map version differs from server:` followed by the local and the server checksums joined by a not-equal sign (during demo playback only a warning is printed). Then the wall textures (`allow_download_textures`), see 7.

**Stage 4, finish (read).** Waits until no download of any kind is pending (developer print: `waiting for others...`), then sends `begin`.

**Where the waiting resumes (read):** every finished download, UDP or HTTP, re-enters the check at the stage it stopped at, so the waits above are re-tested after each file.

### 2. Files named inside models

- **.md2 (read):** after stage 1's model downloads are all finished, the client opens each model file it now has and asks for every skin name stored in it, as written in the file (a full path, normally `models/.../skin.pcx`). The file's skin count must be 32 or fewer and every name must end inside its 64-byte field, else the model is treated as damaged and none of its skins is asked for.
- **.sp2 (read):** the same, for each frame's picture name in a sprite (32 frames at most, each name ending within its field).
- **Other kinds (read):** a model file of any other kind (an .md3, anything unrecognised) has nothing inside it fetched. A model that is not present (its download failed or was refused) is skipped silently.
- **Ordering (read):** yes, the skins are asked for only after the models have arrived, because stage 2 waits for the "model" kind first. That wait happens only while `allow_download_models` is on, and the skin check itself is under the same gate.

### 3. Sounds

- Sound configstrings from slot 1 up, stopping at the first empty slot. **read.**
- A name starting with `*` (a figure's gendered sound) is skipped here. **read.**
- A name starting with `#` is used as a full path with the `#` removed. Any other name becomes `sound/<name>`. **read.**
- A figure's own sounds are fetched in the figure stage (5): for each sound configstring that starts with `*`, the file `players/<figure>/<name without the *>` (for example `*death1.wav` becomes `players/male/death1.wav`). Gated by `allow_download_players`, not by the sounds switch. **read.**

### 4. Pictures

- The picture configstrings ("images", 544 to 799 in the classic layout) from slot 1 up, stopping at the first empty slot. **read.**
- A plain name becomes `pics/<name>.pcx`. No other extension is tried for the download. **read.**
- A name starting with `/` or `\` is used as a full path with that first character removed, and no extension is added (so it must carry its own). **read.**
- With the extended layout, a name that contains a `/` and has an extension is skipped here (it was already handled under the models gate in stage 2). **read.**

### 5. Figures

For each non-empty player-look configstring (up to 256 slots, an empty slot is stepped over, not an end), the value has the form `<player name>\<figure>/<skin>`. **read.** It is split into the figure and the skin. **read.** The rules that replace a bad look (read):
- No `/` (and no `\`) between figure and skin: the look becomes `male/grunt`.
- An empty figure (the value starts with `/`): the figure becomes `male`.
- The skin name is not made only of letters, digits, `_` and `-`, or `cl_noskins` is 2: if the figure is `female` the look becomes `female/athena`, otherwise `male/grunt`.
- The figure name is not made only of letters, digits, `_` and `-`, or `cl_noskins` is 1: the look becomes `male/grunt`.

So an invalid figure folder name is never fetched: it is replaced by `male` (with the skin `grunt`). **read.**

For the resulting `<figure>/<skin>`, the files asked for, in this order (read):
1. `players/<figure>/tris.md2`
2. the view-weapon models: `players/<figure>/<w>` for each name in the list of figure weapon models. The list always starts with `weapon.md2`; when `cl_vwep` is on (default 1) it adds, in model-slot order, every model configstring that starts with `#`, with the `#` removed (for example `#w_shotgun.md2` gives `players/<figure>/w_shotgun.md2`). The list holds 256 names at most. The list is built when `precache` arrives, before the check.
3. `players/<figure>/weapon.pcx` (the default weapon skin)
4. `players/<figure>/<skin>.pcx`
5. `players/<figure>/<skin>_i.pcx` (the icon)
6. the gendered sounds, as in 3.

### 6. The sky

- The name is configstring index 2. **read.**
- The six sides in order: `rt`, `lf`, `bk`, `ft`, `up`, `dn`. **read.**
- Each is asked for as `env/<sky><side>.tga`, only .tga, no .pcx. Gated by `allow_download_textures`. **read.**
- With an empty sky name the paths would be `env/rt.tga` and so on. **derived** (the client does not test for an empty name here).

### 7. Wall textures

- After the map is loaded, one path per surface record (texinfo) of the map, in record order: `textures/<name>.wal`. **read.**
- Only .wal is asked for. **read.**
- A record flagged as never drawn (the no-draw surface flag) is skipped, but only when the map carries a BSPX extension block. **read.**
- Duplicates (many records share one texture) are dropped by the queue, which refuses a path it already holds, and by the "already present" check once the first copy has arrived. **read.**
- Animation chains are not followed as such. Every frame of a chain is itself a surface record in the map, so it is covered by the walk anyway. **derived.**

### 8. What makes a file be skipped

Every path goes through the same checks, in this order (read):
1. The path is 64 characters or longer: refused.
2. The path is normalised: backslashes become slashes, `./` and `../` pieces are folded.
3. Refused as an invalid path if: it is empty or has a character that is not printable (on Windows also any of `< > : " | ? *`); its first or last character is not a letter, digit, `_` or `-`; it still contains `..`; it has no `/` at all (so nothing is fetched into the game folder's root).
4. Its extension must be one of: bsp, dm2, ent, jpg, loc, md2, md3, ogg, pcx, png, sp2, tga, txt, wal, wav (any case). Otherwise refused.
5. Already present, in any pack or as a loose file in the search path: skipped.
6. A path with capitals is lower-cased before it is asked for.
7. Ignored: any path starting with `save/`, `music/` or `video/`, and any path matching a wildcard pattern in the player's `download-ignores.txt` (one pattern per line; empty lines and lines starting with `#` or `/` are comments). The file is read once at start-up and again when the file system restarts.
8. If an HTTP download server is set, the file goes to the HTTP queue, else to the UDP queue.
9. The queue refuses a path it already holds. An entry stays in the queue, finished or not, for the whole connection, so each path is tried exactly once per connection: a file that failed is not asked for again until the client disconnects (the queue is emptied on disconnect).

**Derived:** a map change on the same connection (`changing` then `reconnect`, which does not disconnect, see 12) keeps the queue, so a file that failed on the previous map is not retried on the next one.

The gate switches, all saved to the config (read): `allow_download` (default 1 on a client, 0 on a dedicated server; -1 means permanently off, after which the setting becomes read-only and the command `download` answers `Downloads are permanently disabled.`), `allow_download_maps`, `allow_download_models`, `allow_download_sounds`, `allow_download_pics`, `allow_download_players`, `allow_download_textures`, all default 1. `allow_download_others` exists but is only read by the server side; the client's join check never consults it. **read.**

The player command `download <path>` runs the same checks (it needs a connection, and exactly one argument; usage text `Usage: download <filename>`; a refusal prints `Couldn't download <path>: <reason>`). **read.**

### 9. Order, `begin`, and what is printed

- Stage order is as in 1: map and models, then (after the models arrive) model skins, sounds, pictures, figures, sky, then (after the map arrives) textures. **read.**
- Within the UDP queue, downloads run one at a time in the order they were queued, the first one starting at the moment it is queued. **read.**
- `begin` waits until every queued download of every kind is finished (succeeded, failed or refused by the server). **read.** Before sending `begin` the client loads the renderer's data for the level, then the sounds, then the place-name file, then sends `begin <spawncount>`. **read.**
- Printed per UDP file when it ends: `[UDP] <path> [DONE]`, `[FAIL]` (the server refused it) or `[STOP]` (the server stopped it part way), followed by `[<n> remaining file]` or `[<n> remaining files]`. **read.** A resumed file prints, at developer level only, `[UDP] Resuming <path>`; a new one `[UDP] Downloading <path>`. **read.** The three stage waits print at developer level only (see 1). **read.**
- Resume: a partial download is kept as `<path>.tmp`; if it exists the client asks `download "<path>" <bytes already held>`, else `download "<path>"`; on the last block the temporary file is renamed to the real name. Each block is acknowledged by the client sending `nextdl`. **read.**
- HTTP side note: when the first file of a connection is queued for HTTP, and `cl_http_filelists` is on (default 1), the client also asks for `<gamedir>.filelist`, and for each map `<gamedir>/maps/<map>.filelist`; it sends `download http` to the server as a notice. A file list may name further files (and .pak or .pkz packs) to fetch. **read.**

## B. Commands the server pushes

### 10. How pushed text is run

- The text of a pushed command (up to 1024 characters) is appended to a separate command buffer kept for server text, not the player's console buffer. Nothing runs while the packet is being read. **read.**
- Once per client frame, after all the packets that arrived that frame have been read, the player's console buffer runs first and then the server's buffer. So pushed commands run in the same frame, after the rest of the packet (and after the rest of any other packet received that frame). **read.**
- One exception: when a new server-data message arrives (the start of a level), anything still waiting in the server's buffer runs first, before the client resets its level state. **read.**
- In demo playback during the opening of a demo, the buffer runs between each demo message until the demo reaches `precache`. **read.**
- The buffer holds 1024 characters in all; text that would overflow it is dropped whole, with a warning ending in `overflow`. **read.**
- Lines split on newlines and on semicolons outside quotes. `wait` defers the rest of the buffer by one command frame per count (at most 1000 in all, else `Runaway wait count`); frames are counted at the input rate. Alias expansion from pushed text is limited to 16 per frame (`Runaway alias loop`). **read.**
- Pushed text gets macro and variable expansion (`$name` becomes that setting's value), except that settings marked private (`rcon_password`, `rcon_address`, and the server's passwords) expand to an empty string. **read.**

### 11. What is refused or limited when the server sends it

Each pushed line is handled in this order (read):
1. `changing` and `precache` are handled by the client itself, always.
2. During demo playback (unless the demo was opened in compatibility mode), every other command except `play` is dropped silently.
3. `reconnect` is always allowed. `cmd` (send the rest of the line to the server) is always allowed when no whitelist entries are loaded.
4. If `cl_ignore_stufftext` (default 0) is 1 or more, the raw line (before expansion) must match a wildcard pattern from the player's `stufftext-whitelist.txt`, or it is dropped. At 2 or more each drop prints `Ignored stufftext: <line>`. If the whitelist has at least one entry, `cmd` must itself be whitelisted.
5. Everything else runs as if typed.

With `cl_ignore_stufftext 0` (the default) the server may run any command and set any setting the player could, under the same protections the player has (read):
- read-only settings (`<name> is read-only.`), command-line-only settings (`<name> may be set from command line only.`), cheat-protected settings when the server does not allow cheats (`<name> is cheat protected.`), and latched settings, which wait for the next game. There is no separate list of settings a server may not set. **read.**
- `vid_restart` from the server is ignored silently unless it carries the word `force` or `cl_allow_vid_restart` is 1. **read.**
- `reconnect` from the server, while the client has not finished loading, does not disconnect first (it only re-sends `new`); typed by the player, or once loaded, it disconnects first. **read.**
- Once `allow_download` is -1 the server cannot turn downloading back on. **read.**
- The server can read any non-private setting by pushing `cmd <something> $<setting>`, which the client expands and sends back. **read** (the player documentation says this outright).

The player documentation adds that commands are matched "raw, before macro expansion, but after splitting multi-line or semicolon separated commands", and that the stock server will not admit a client that cannot answer the version query. **read.**

### 12. What a stock server pushes, and what the client does

From the Q2PRO server and its bundled base game (read unless marked):
- **An empty line**, twice during `new`: does nothing.
- **`cmd <marker>c version $version`** (and, with the anti-cheat build, the same for `actoken`) on the first `new` of a connection: the client answers with its version string. The server drops a client at `begin` that never answered (`failed version probe`). `<marker>` is the delete character (code 127).
- **Admin lists:** whatever the server admin put in the `connect` list (`addstuffcmd connect ...`) after the version query, and the `begin` list (`addstuffcmd begin ...`) when the client begins. Each runs as ordinary pushed text.
- **Setting checks:** for each banned-setting rule, `cmd <marker>c <setting> $<setting>`; the client sends the value back. An admin can push the same for any setting by hand (`stuffcvar`), and raw text to one client (`stuff`) or all (`stuffall`). A matched rule or filter may push its comment text as a command.
- **`precache <spawncount>`** after the game state: starts the fetch of A, ending in `begin <spawncount>`.
- **`changing map=<name>`** when the server starts a new level: the client stops a demo it is recording, prints `Changing map...`, runs the player's `cl_changemapcmd` text and the `#cl_changelevel` trigger (not in demos), shows the loading plaque, falls back from active to connected (which also clears pause, see 13), and remembers the map name for the plaque.
- **`reconnect`** right after the new level is set up: the client prints `Reconnecting...` and sends `new` (unless a UDP download is in progress, in which case it does nothing more); the server answers with the new level's data and a new `precache`.
- **The forced-reconnect probe** (only when the server sets `sv_force_reconnect` to an address, and not for local clients): a series of `set` lines with random 15-character names, built so that the client ends up storing a random setting the server checks later and running `connect <that address>`; the server then drops the client and expects it back. A client that does not come back is dropped at `begin` with `failed to reconnect`.
- **`set name "<name>"`** when the server refuses a name change (empty, or changed too often): the client's `name` setting is put back.
- **`connect <address>`** in the refusal packet of a full server that redirects: the client connects there.
- **`spectator 0`** or **`spectator 1`** from the base game when a spectator password is wrong, the spectator limit is full, or the player password is wrong: sets the client's `spectator` setting back.
- **`record`**: the stock server never pushes it. **read.** (A mod may, and with filtering off it would run.) **derived.**
- **`alias`**: the stock server and base game never push it. **read** (none found in the server or the bundled game).

## C. Pause

### 13. The `pause` command

The state is two read-only settings: `cl_paused` (0 not paused, 1 paused automatically by a menu or the console, 2 paused by hand) and `sv_paused` (0 or 1: whether the game is actually stopped). Neither can be set by the player directly. **read.**

`pause` (read):
- If `cl_paused` is 2 it becomes 0; otherwise (0 or 1) it becomes 2. Then the pause state is re-evaluated.
- While watching a broadcast through the built-in relay viewer, `pause` instead runs `mvdpause @@` and nothing else.

The re-evaluation (read):
- If the client is not fully in the game (connecting, loading, disconnected), both settings are forced to 0.
- If the menu or console is open, `cl_paused` is 0 and `cl_autopause` is on, `cl_paused` becomes 1.
- If neither is open and `cl_paused` is 1, it becomes 0. A hand pause (2) is not cleared by closing them.
- In demo playback, `sv_paused` follows `cl_paused` directly (1 when it is non-zero), except during `timedemo`, when it stays 0.

**Single player on a local server (read):** the local server, on every server frame, sets `sv_paused` to 1 and skips the whole frame when all of these hold: it is not a dedicated server, `cl_paused` is non-zero, `timedemo` is off, exactly one client is on the server, and no broadcast relay viewers are attached. While paused it does not run the game, client timeouts, pings, move-time budgets, sending to clients or master heartbeats, and the frame counter does not advance. When any condition fails it sets `sv_paused` back to 0 and runs normally. So a listen server with a second player cannot be paused.

**On a remote server (derived):** `sv_paused` is only ever set by the client's own local server or by demo playback, so on a joined remote server `pause` sets `cl_paused` to 2 and nothing else changes: the game goes on, the view and the sound go on, and the pause picture is not drawn (it needs `sv_paused`). This holds even when the player is alone on that remote server.

**Demo playback (read):** `pause` stops the demo (see the clock below) unless `timedemo` is on.

What `sv_paused` stops on the client (read):
- the client's game clock: it does not advance, so the view's interpolation stops and, in demo playback, no further demo messages are read (the demo clock stops),
- movement commands: the move is cleared and no time accumulates in the command; no movement commands are sent, only keep-alives and reliable messages (such as a changed setting),
- prediction: off, and its error is zeroed,
- the mouse is released (input is not grabbed while paused in a window),
- entity looping sounds (the ambient sounds of moving things) are not mixed,
- the heat-beam effect's per-frame update,
- one of the engine's two local millisecond clocks stops (what reads that clock is not settled by this question). **open** as to its users.

What does not stop (derived, from the sound code testing pause only for looping sounds): sounds already playing play out, and the music track goes on (music has its own `ogg pause` command, unrelated to game pause).

A cinematic (video) is a separate case: it holds its place while the menu or console is up, whatever `cl_autopause` says. **read.**

### 14. What unpauses, and the menu and console

- A hand pause (2) ends only with `pause` again, or when the client leaves the active state (disconnect, a map change's `changing`, an error), which forces both settings to 0. A server starting a level, or shutting down, also sets `sv_paused` to 0. **read.**
- An automatic pause (1) ends when the menu and the console are both closed. Pressing `pause` while auto-paused turns it into a hand pause (2) instead of ending it. **read.**
- Opening the menu or the console pauses only if `cl_autopause` is on (default 1, not saved by default) and the client is not already paused; it pauses single player (subject to the one-client rule above) and demo playback. The player documentation describes it as "single player game or demo playback is automatically paused once client console or menu is opened". **read.**
- Losing window focus or being minimised does not pause. **read** (the focus change touches input, sound and frame timing, not pause). Sound may go silent on focus loss through `s_auto_focus` (default 0, sound always on; 1 off when minimised or on another desktop; 2 off without input focus). **read.**
- The keyboard has a Pause key name (`PAUSE`), but the source sets no default binding to the `pause` command; whether the stock configuration binds it is not in this source. **open.**

### 15. What is drawn while paused

- The picture named `pause`, which the picture loader resolves to `pics/pause.pcx` (the renderer may substitute another image format for the same name when the player's texture settings allow). **read.**
- Drawn only when `sv_paused` and `cl_paused` are both non-zero and `scr_showpause` is 1. **read.**
- Placed at the centre of the HUD area: its left edge at (HUD width minus picture width) / 2 and its top at (HUD height minus picture height) / 2, in HUD units, so it scales with `scr_scale`. **read.**
- It is drawn last among the HUD elements (after the status bar, layouts, inventory, centre print, net graph, chat and the lag icon), with the HUD's common transparency `scr_alpha`. Nothing of the HUD is drawn while a full-screen (opaque) menu is up. **read.**
- `scr_showpause` (default 1, not saved by default): 0 draws nothing; 1 the centred picture; 2 the text `[PAUSED]` right-aligned in the demo progress bar instead, which is shown only during demo playback (and relay playback) when `scr_demobar` is on and the demo is not in compatibility mode. **read.**
- On a joined remote server nothing is drawn, since `sv_paused` stays 0. **derived.**
