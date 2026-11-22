# Shooter's Edge: the working notes

**A WORKING AREA**, like the rest of `porting/`: deleted when the port ends. The design is
`docs/shooters-edge.md`; this directory holds what only matters while the game is being built:
the survey of existing implementations, and the specifications the building sessions read.

Shooter's Edge is compatible with QuakeWorld: its file formats, its QuakeC and its protocol. This
working area is the only place in the tree that names them; everywhere else they are "the
classic game", "QC" and "the classic protocol", and `make agents-check` holds it.

| File | What |
| --- | --- |
| `README.md` | This page: the clean-room rules for these notes |
| `survey.md` | What today's community clients and servers do, their licences, and which features the plan takes |
| `configs.md` | The console language at its modern extent, as a specification in our words (`engine/console` is built from it) |
| `cvar-meanings.md` | Settings that share a name across the three classic games and the modern client: each one's default, values, flags and effect, same or different concept, the same concept under other names, the shadow settings, the modern client's changed defaults (clean-room facts) |
| `client-inventory.md` | Every setting, command and user info key of the first game's modern client (and its element HUD's), its options menu, what its commands print, the stop-sound message, what it forwards to a server (clean-room facts, M18k4) |
| `q2-client-inventory.md` | Every setting and command of Quake II's stock client, its game library's commands, its user info and menus, and the modern client's daily additions (clean-room facts, M18k4) |
| `q2-client-fork-delta.md` | The Action Quake community's fork of the modern Quake II client, the owner's main reference for Quake II's client: every client setting and command it adds, drops or changes against the modern client, by subject (clean-room facts) |
| `q2-cvarbans.md` | Setting bans and user info bans on the modern Quake II server: how a client's setting is asked (a stuffed `cmd` line its console expands), when, the match forms, the actions, the list words and their answers; macro expansion in the stock clients (clean-room answers, M18v) |
| `q2-vispatch.md` | The modern Quake II server's visibility patch for four stock maps (recognised by checksum, the clusters each row gains), entity files beside maps (the CRC-16 in the name) and the binary override file (clean-room answers, M18v) |
| `q2-switches.md` | Quake II game library's switches: aim from the muzzle at the crosshair's point (`aimfix`), the machine gun climb and `g_machinegun_norecoil`, player and monster footsteps, `g_fix_triggered`, `g_commanderbody_nogod`, `maxentities` and running out of slots (clean-room answers, M18v) |
| `server-settings-browse.md` | What a connected client can learn of a classic server's settings (server info keys, status replies, the match mod's listing commands) and how players change them (direct commands, votes, their words and lines), QuakeWorld's match servers and Quake II's (clean-room facts, the server settings menu) |
| `q2-gaps2-rules.md` | Quake II's second gap list, the game rules: single player after the first audit, co-op, deathmatch, the client commands, ranked (clean-room facts, 2026-11-21) |
| `q2-gaps2-server.md` | Quake II's second gap list, the server: what each client is sent (area culling), hosting a game library, the net, admin, downloads (clean-room facts, 2026-11-21) |
| `q2-gaps2-client.md` | Quake II's second gap list, the client: drawing, the HUD, menus, sound, user info, downloads, demos (clean-room facts, 2026-11-21) |
| `q2-ledger-game.md` | Quake II's complete checklist ledger, the game: every command, setting, entity, item, monster, message, dmflag and layout, each done, partial, missing or not taken (clean-room, 2026-11-21) |
| `q2-ledger-server.md` | The ledger's server half, against the upstream server reference at a pinned commit: console commands, settings with defaults, server messages, what clients may send, the game library's interface, messages, features |
| `q2-console-menus.md` | Quake II's client console commands, macros, fixed keys, menu pages row by row and printed lines, word for word, against the upstream client at a pinned commit (clean-room facts, 2026-11-21) |
| `q2-mvd-relay.md` | Facts read for the multiview recorder on a game server (the recording, the TCP stream, the access lists, the dummy, every `sv_mvd_` setting, the log lines) and the relay that plays such streams (`mvdconnect`, channels, the waiting room, the viewers' words, every `mvd_` setting) |
| `q2-proto36-server.md` | Facts read for the server's side of protocols 35 and 36: the challenge's list, the connect's tail and its refusals, the packet length, the newer channel and its pieces, the frame's top, the signon as a game state or streams, compression, the later moves, the client's settings and what they leave out |
| `q2-console-words-facts.md` | Facts read for the dedicated server's general console words: quit, recycle and killserver and what clients read, `_echo`, `text`, `setu`, `sets`, the `$` macros, the file and network words |
| `q2-downloads-facts.md` | Facts read for Quake II's server downloads (the kinds and their settings, the size limit, the resume, the web address), rcon answers in packets, and `nextserver` |
| `q2-game-api-ex-facts.md` | Facts read for Quake II's extended game interface: the second entry point, its imports and exports by version, the file extension, the feature bits, the extra user info, the extended limits, the binary layout |
| `q2-mvd-dummy-facts.md` | Facts read for Quake II's recorder's dummy client (its settings, commands, what is recorded) and the relay's own rewrites, viewer lines and free flight |
| `q2-operator.md` | Facts read for the server's operator words and player queries: `status` and its letters, kick and kickban, stuff and the stuff lists, `dumpuser`, `lag`, `sinfo`, `info`, `packetdup`, the user info checks at a connect and a change, the version probe, the forced reconnect, the shutdown lines |
| `q2-ledger-client.md` | The ledger's client half, against the same reference: commands, settings, menus, server messages, effects, muzzle flashes, the HUD, messages, demos, keys, sound and picture |
| `q2-flashes.md` | Muzzle flashes on the wire: a player's and a monster's flash values, their lights, sounds, models, who sends them and when, the guns' places rounded (read) |
| `q2-sp-fixes.md` | Single player's built-in map repairs (fact1, fact2, command, the eight maps' unnamed starts), item teams, the item and cheat commands' exact lines, `cycleweap` and `prefweap` (read) |
| `q2-client-fx2.md` | The temporary entities left undrawn (bubbles, force wall, welding and tunnel sparks, steam, flashlight, the mission packs' bursts), the CTF and mission-pack effect bits, the layout's `rstring`, `rstring2` and `color`, the area bits (read) |
| `client-gaps.md` | Which of those the window has, and the ranked rounds still to build, gameplay first (M18k4) |
| `chat-ignore.md` | The ignore lists and chat filters: how a chat line's speaker is found, what each setting drops (clean-room answers, M18k4) |
| `client-mouse-and-q2-player.md` | The mouse (filter, acceleration, raw input), Quake II's player settings (hand, gender, visible weapons, skins, prediction, autosens), its chat filters and command spellings (clean-room answers, M18k4) |
| `client-teamplay-sets.md` | The teamplay item sets (`tp_took`, `tp_pickup`, `tp_point`), the team message filter, the regex triggers, map and sky groups, `cl_timeout`, `showpackets`, `showdrop`, rcon by setting (clean-room answers, M18k4 rounds 19 and 20) |
| `client-net-pacing.md` | How the clients pace their physics, moves and packets (`cl_physfps`, `cl_c2spps`, `cl_earlypackets`; Quake II's `cl_maxfps`, `r_maxfps`, `cl_async`, `cl_maxpackets`, `cl_packetdup`) and when the QuakeWorld server sends the view kicks (clean-room answers, M18k4 round 12) |
| `client-demo-settings.md` | Recording matches on their own (`match_auto_record`, `match_save`, the minimum length, spectating, the console log), the name patterns' codes and match types, `easyrecord` and `record`'s lines, the demo clock, `demo_jump`, the start-up demo, the third-person camera; Quake II's `autorecord`, demo snapshots, `seek`, suspend and resume (clean-room answers, M18k4 round 14) |
| `client-demo-tables.md` | The demo watcher's tables (`mvd_info` and its codes, `mvd_status`), `mvd_moreinfo`'s pickup lines, the demo playlist and its track name, `cl_demoteamplay`, `demo_dir`, `demo_benchmarkdumps`, `cl_demoplay_flash` (clean-room answers, M18k4 round 28) |
| `client-browser-and-screenshots.md` | The server browser's settings (hiding, `sb_info_filter`'s words, the columns, `sb_sortservers`' codes, `sb_status`, the query pacing, `sb_liveupdate`, `sb_autohide`, the caches) and the screenshot settings (`sshot_format`, `sshot_dir`, the names, `match_auto_sshot`, `scr_allowsnap`) (clean-room answers, M18k4 round 30) |
| `client-chat-settings.md` | Chat on the console: `cl_chatmode`, `//` and `/` lines, the chat sounds by kind (`s_chat_custom`, `s_mm1_file` ...), `msg_filter`, the notify lines, `con_timestamps`, `con_highlight` (clean-room answers, M18k4 round 24; Quake II's chat settings still open) |
| `weapon-choice.md` | The weapon command, preselection, hiding, the score keys and the servers' switch rules on a pickup (clean-room answers, M18k4) |
| `installs.md` | The nQuake and nQuake2 installs: layouts, file kinds, the bundled clients' config order and search paths (clean-room facts) |
| `loc-editing.md` | Making and editing place names in game: the commands, the file written, loading (clean-room facts) |
| `formats.md` | The file formats: pak, pk3, the search order, BSP29 lump by lump, BSP2, MDL, SPR, WAD2, LMP, WAV |
| `progs.md` | QuakeC bytecode for the VM (M18c): the file and its checks, memory and value encodings, calls, every opcode's semantics, the engine-side globals and fields, the map's entity text, allocating and freeing entities |
| `builtins.md` | The modern server's builtins by number, its extension builtins and list, the monster moves, the hooks and when they are called, client commands, the extension fields (M18p) |
| `qc-strings.md` | The extended string builtins (the string set: stov, strstrofs, str2chr, chr2str, strconv, strpad, infoadd, infoget, the comparisons) and sprintf, each edge case, from a clean-room reader (M18l) |
| `antilag.md` | Lag compensation by rewind in the QuakeWorld server, its mod's vote, its client's debug views and the Quake II modern servers: the levels, the marked traces, missiles, the target time, the history and its edges, the opt-out (clean-room facts, M18v13) |
| `entity-look.md` | An entity's look fields (scale, colormod, alpha) in QC and their bytes on the wire, the entity delta's and the player update's encodings, from a clean-room reader (M18l) |
| `sky-box.md` | A map's own sky box: its name in the worldspawn, the six pictures' paths and formats, their orientation, where the box is drawn; Quake II's rotation, from a clean-room reader (M18i12) |
| `player-skins.md` | Each player's skin (the name chosen, the picture and its cut, the fallbacks, the colours' rows, `noskins`, `baseskin`, `allskins`, 24-bit forms), from a clean-room reader (M18i15) |
| `player-figures.md` | The second game's player figures (the look "<model>/<skin>", its fallbacks to male and grunt, the carried weapons, 24-bit pictures, the downloads at connect), from a clean-room reader (M18i16) |
| `fog-alpha.md` | A map's fog (its worldspawn keys, the curve, the sky's share, the command, the reset) and see-through liquids (the alpha settings, the server's permission, the drawing), from a clean-room reader (M18i13, M18i14) |
| `movement.md` | The player's movement at the modern server's defaults: the settings, every step of the move, the clip hulls and the trace, the test vectors' format (M18o1) |
| `gameloop.md` | The game loop: the game program's hooks, entities, the area tree, the server frame, thinking, tossing, pushers, touching, the player's packet and command, client prediction (M18o2) |
| `vectors/` | Measured test vectors: `pmove.txt` (the player's move in hand-built worlds), `framemove.txt` (the original move in the same worlds), `hulltrace.txt` (single traces by the original single-player trace in the same worlds: where the end point lands at corners and slants) and `entity.txt` (tossed, bounced and flying entities), for `se-parity` |
| `spectator-tracking.md` | Spectator tracking: `ptrack` on the classic server, what a tracker is sent, the teleport, the client's lock and its recording, from a clean-room reader (M56q) |
| `protocol.md` | The classic protocol, version 28: connectionless packets, the net channel, the messages to fill with M18g |
| `campaign.md` | The single-player program's host (M18s): its layout (crc 5927), the builtin differences, spawn parms, `serverflags`, level changes, intermission and finale, the statistics, episodes and the full game, saves |
| `scoreboard.md` | The scores key's boards (M18k3): the network client's players' and teams' scoreboards, what fills each row, the scorebar, the single-player solo scorebar and the intermission's positions |
| `frame-move.md` | The original player move and the movement setting `sv_playermove` (M18s): who jumps in each mix of program and move, the single-player frame |
| `campaign-rules.md` | What a campaign program does (monsters' senses and states, the roster, the level flow), for our own campaign variant mod (M18s7) |
| `deaths.md` | Monster deaths, gibs and blood: each kind's death sequences, gib health and pieces, the gib and head throw, blood at hits, the client's bursts and model-flag trails, bouncing, the zombie, the player (M18s7) |
| `deathmatch4.md` | Deathmatch 4 in the classic match mod: the armed spawn, spawn invincibility, the items stripped and kept, backpacks and bonus powers, OctaPower, its settings and commands, spawns and scoring |
| `midair.md` | Midair in the classic match mod (clean-room answers): the toggle and the minimum height, the rockets-only spawn, when a rocket kills, the height trace, knockback, the score bands, the messages and statistics |
| `match-stats.md` | The per-player match stats the classic match mod keeps and shows when a match ends (clean-room answers): what an attack and a hit are per weapon, direct, real and virtual rocket hits, damage counted, kills per weapon, items and time held, streaks, spawn frags, the printed report and its top lists, the on-demand commands |
| `weapon-switch.md` | The weapon after a shot in the classic match mod: refire times, the switch held until the refire ends, what a dropped pack holds, what onlookers can tell, the client's switch-after-fire settings |
| `demo-smoothing.md` | Recorded games drawn between their frames: the client's one-frame-behind playback, the rules for players, the tracked view and entities, where it snaps |
| `held-weapons.md` | The first-person weapon's frames: the axe's two swings, how a swing is picked, when the frame is set, what the client draws |
| `rocket-jump-helper.md` | The classic match mod's rocket jump helpers (impulses 164 and 156): what they do in one command, their failures and settings, from a clean-room reader (M18e) |
| `jump-walk-death.md` | The jump's and the swimmer's sounds, landing sounds, walking (there is none at the modern defaults: `+speed` is a run key), the view weapon and eye at death |
| `q2.md` | Quake II (M18t2, for M18t3 and M18t4): the index of its notes, the game's shape, the demo pack measured, the game library's calls as our rules runner's |
| `q2-formats.md` | Quake II's files: the search order, contents and surface flags, the collision lumps, WAL, PCX and the palette, MD2, SP2, the sky, player models, skins and sounds |
| `arena-hybrid.md` | The hybrid-engine arena shooter (2017): its engine, its pack files as community notes describe them (encrypted directory), and why no reader is built |
| `sector-family.md` | The sector-based sprite shooter and Quake II (clean-room facts for `docs/porting-sprite-games.md`): the move, blocking, auto-aim, line and sector specials, sight and sound, randomness, demos, lockstep play, drawing rules, the held weapon, things and monsters |
| `q2-movement.md` | Quake II's player move (the packed state, the command, every step, snapping) and the brush trace and point contents it moves through |
| `q2-rules.md` | Quake II's rules: the 10 Hz frame, entities and physics, items, armour, damage, every weapon and projectile, a player's life, deathmatch and `dmflags`, the demo maps' entities |
| `q2-monsters.md` | Quake II's monsters on the demo maps: life, senses, the attack choice, the roster's numbers |
| `q2-monsters-full.md` | The full Quake II's other monsters (M18v7d3): the medic and its revival, the brains, iron maiden, gladiator, mutant, technician, icarus, shark, super tank, hornet, Jorg and the Makron; the step move; the demo roster's speeds |
| `q2-movers.md` | Quake II's map movers and triggers (M18v7c): the plain and accelerated moves frame by frame, doors, buttons, plats, trains, rotators, walls, timers, the triggers, teleporters, riders, the exit and the targets |
| `q2-demo.md` | Quake II's client recording (.dm2) and protocol 34's server messages: the framing, every message's layout, the entity and player deltas, the signon, the encodings; the demo's recording measured (M57f) |
| `demo-formats-classic.md` | Clean-room answers (M18w): the proxy's compressed client demo (.qwz: its range coder, model file and transforms), protocol 26 client recordings, record types past 2, the protocol extensions a recording may carry |
| `demo-formats-single.md` | Clean-room answers (M18w): single-player demos past protocol 15 (666, 999, the re-release's ids), ProQuake and DarkPlaces variants, Dzip (.dz) |
| `demo-formats-q2.md` | Clean-room answers (M18w): Quake II's demos past protocol 34 (35, 36, the re-release's 2022/2023), the server's recording, the multiview recording (.mvd2) and its stream, protocol 26 against 34 |
| `demo-write-client.md` | Clean-room answers (M18w): what a written protocol 28 client recording must hold for the oldest client to play it: packet and table limits, frames and sequences, command records, the signon's order, the view, what a converter drops |
| `demo-write-single.md` | Clean-room answers (M18w): what a written protocol 15 single-player demo (.dem) converted from a multiplayer match must hold for the oldest single-player client to play it: block and table limits, the signon's stages, players and colours, frames, the hide rule and the baseline delta, time, the view, what a converter drops, the end |
| `netgame-transport.md` | Clean-room answers (M57i): the single-player/netgame client's UDP connection (protocol 15): the datagram header and flags, the control packets (connect, accept, reject, queries), the port, reliable fragments and acks, sizes, timeouts and keepalives, the signon as the server sees it, the frame datagram, disconnect, the connect command |
| `q2-net.md` | Quake II's network protocol (protocol 34), the client's side of a live connection: the out-of-band connect and its refusals, challenges, the user info, the 10-byte header and the reliable channel, timeouts, the signon's string commands, the move and its checksum, the acknowledged frame, spectators, level changes and rate; measured on a stock server |
| `q2-game-library.md` | Hosting Quake II's unmodified native game library (clean-room answers, M18v10): the entry point, version 3 and where the file is looked for, both call tables in order with their x86-64 offsets, the entity, client, command, move block, trace and setting layouts measured, what link, trace, sound, the index calls, the write calls, unicast and multicast do, configstrings, the server frame and which entities a frame carries, connect, begin and level changes, spawning, area portals, and what a deathmatch host must register |
| `q2-campaign.md` | Quake II's single-player campaign (M18v7d2): the help computer and its counters, what carries across levels and units, cross-level bits, revisited levels, saves and the autosave, the remaining single-player entities, co-op, the end of the game |
| `q1-sp-gaps.md` | Quake's single player against the original (2026-11-20): the gap list from two audits, ranked, what has landed and what is next |
| `q1-qw-ledger.md` | Quake and QuakeWorld item by item (2026-11-21): every command, setting, builtin, message, menu page, entity, item, weapon and deathmatch rule of the stock releases, marked done, missing or not taken; THE gap list, its missing count only goes down |
| `q1-qw-console-facts.md` | Quake and QuakeWorld console words, the server's lines, the protocol-15 client, user info keys and settings (msg, user, fullinfo, path, port, noaim, ambience, chase, m_side, sv_highchars, zombietime, *progs, item_weapon): readers' facts for the ledger's rows |
| `qw-dm-rules.md` | QuakeWorld's stock deathmatch program in facts (2026-11-21): liquids, a player's life and frames, items, weapons, modes, map entities, the intermission; what Quake's rules on the arena are built from |
| `q1-sp-host.md` | Clean-room answers (audit A): the physics frame, movetypes, pushers, the monster-movement and sight builtins, touch, the sounds the engine starts, setmodel, cheats, each against ours |
| `q1-sp-flow.md` | Clean-room answers (audit B): episodes, the screen's tints, the intermission and finale, the death view, the centre print, pause, the console, music, the attract loop, saves, co-op joiners, each against ours |
| `q2-hud.md` | Quake II's HUD (M18v4): the layout language word by word, the digit fields and their flashing, the 18 stats the stock game fills, the two status bars as elements and positions, the scoreboard, help computer and inventory screens, the centre print and the crosshair |
| `q2-player-sounds.md` | Quake II's players' own sounds (M18v4f): every `*` sound and its event (pain by health, death, jump, falls), the pain cry's 0.7 s spacing, the water's and the drowning's sounds, how a client finds a `*` sound by the player's model and falls back to the base voice, precache, the entity events for landings |
| `localization.md` | The re-release's text keys (clean-room answers): which prints carry `$` keys, how arguments fill `{}`/`{N}`, its extra builtins by number and by name, the words file's place and format, where an engine resolves them |
| `television.md` | The classic match broadcaster (clean-room answers): its stream protocol, handshake and auth, the delay, catch-up, chat and viewer lists, the server's side, discovery, HTTP and files (M57) |
| `forward.md` | The classic packet forwarder (clean-room answers): the route in the userinfo, the connect hop by hop, forwarding and timeouts, its queries, the client's route finder (M57) |
| `proxy.md` | The classic proxy and its compressed client demos, from public documents and demo bytes only: features, commands, link compression, recording, observers, what is known of .qwz (M57) |
| `first-person-view.md` | The classic client's first-person view (clean-room answers): the view height, the bob and where it goes, the strafe roll, the damage kick and flash, the punch, step smoothing, idle sway, the dead view, the field of view and what a server forces (M26e) |
| `q2-view.md` | Quake II's first-person view (clean-room answers): the server's view offset and kick angles each 10 Hz frame, the bob by stride, the run's pitch and roll, the damage and fall kicks, the weapons' kicks, the dead view, the clamps, what the client interpolates (M26e) |
| `q2-light.md` | Quake II's light, brightness and sky as its bundled client draws them (clean-room answers): texel times luxel times the intensity, the settings and their defaults, the hue-keeping clamp, gamma, light styles, the flags, entities' light, dynamic lights, the sky's turn and brightness, a missing sky |
| `q2-render-fx.md` | Quake II's render bits and effect bits as its client draws them (clean-room answers): every bit, see-through alphas, the coloured shells and their colours, full bright and min light, the view weapon's bits, the glow pulse, each effect bit's trail and light |
| `q2-gibs.md` | The pieces a body throws in Quake II (clean-room answers): each kind's set, the throw by damage, the clamp, the spin, toss and bounce, rest, life and the gib bit |
| `voice.md` | Voice and chat targets in the voice extension: `voicetarg`, `unmuteall`, `vignore`, the chat line; no proximity routing anywhere |
| `hub.md` | The classic community's live-game and demo website (clean-room answers): its pages and API, the master and status queries, modes and match state, the broadcaster's advert, watching in a browser, the demo index (M56, `docs/hub.md`) |
| `accounts.md` | Logins and match stats on the classic servers and both games' stock clients (clean-room answers): the console login, the accounts file, the web login and its token, the status and scoreboard keys, the match stats file, `lastscores`, frag logs (M59, `docs/accounts.md`) |
| `team-messages.md` | Team messages at the modern client's extent (clean-room answers, M18k): each fun sequence's byte and the stock sheet's glyph there, the `^` rule, every percent code's text and its words' settings, the bracketed forms, field widths, hidden counts and message marks, the expansion order, each `f_` trigger's moment and gate, every canned message's text |

The recorded-game formats are built (M18h, `games/shooters_edge/client/demo.hpp`) and their note
retired. Every fact marked `verify` is checked by a lab before code depends on it.

## The rules for a note

The implementations surveyed here are GPL. A note is how their behaviour reaches this tree
without their code (`docs/shooters-edge.md` § Licensing and the clean room):

- **Facts, in our words.** Byte layouts, numbers, message orders, measured curves, what a
  command does. Never a quotation of source, never a function transcribed into prose step by
  step, never a table copied out of source that the format itself does not define.
- **Say where each fact came from**: a public format description, the language reference, a
  measurement (name the lab and its recording), or a research session that read source (named
  as such, so the building session knows not to look further).
- **Separate agents.** A reader agent that reads GPL source to answer a question writes only the
  note (or a scratch answer document) with no code. The agent that builds or verifies the
  subsystem reads the note and the measurements, never the source, not even a grep hit. The
  `clean-room` skill is the procedure: the questions, the reader's brief, the screen for code.
- **Running is fine; shipping is not.** A lab may fetch and build an existing server or client
  into its cache to measure it. Nothing of it is committed, linked or distributed.

## The shareware release

The labs and the client that need a real map take `--game=<folder>`. The folder is laid out in
one step from the shareware release 1.06 zip as published, which its licence lets anyone keep and
pass on (its SHA-256 is `kKnownZips` in `plugins/classicenhance/source.hpp`):

```sh
ge tool classicenhance -- setup https://ftp.gwdg.de/pub/misc/ftp.idsoftware.com/idstuff/quake/quake106.zip \
    --basedir=<scratch>/classic --cache=<scratch>/cache
# then --game=<scratch>/classic/id1
```

The zip is fetched once into `<scratch>/cache/downloads/` and checked against the known digest; a
second `setup` reads that copy. Nothing it writes goes into the tree.

