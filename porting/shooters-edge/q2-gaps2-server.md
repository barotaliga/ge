# Quake II's server: the second gap analysis (clean-room facts, working note)

A clean-room reader's note, 2026-11-21: facts in the reader's own words, no code. Read: the Q2PRO
project as released under the AQtion name, master of 2026-11-16 (its server half and the stock
Quake II server behaviour it keeps), and our own tree. It extends `q2-modern-server-inventory.md`
(the full list of the original's settings and commands) and `q2-server-gaps.md` (what has landed),
and numbers each remaining server gap S1 to S41. Marks: R read, D derived, M measured.

## B1. The settings and commands an admin sees

The original's whole list (every setting with its default, flags and meaning, every command and its
answers) is already in `porting/shooters-edge/q2-modern-server-inventory.md` and the stock list in
`q2-server-inventory.md`; it was re-read against current master and nothing new was found there. The
comparison with ours follows our notes' Done table in `q2-server-gaps.md` and a re-read of our code.

Landed and still the same (not re-listed): the host name and slots, the rules' settings, the map
list, passwords, timeouts, the download switches for players, models, sounds and maps, flood
control, the rate bounds and hold, spectators and their chat, the move-time budget for our rules,
the zombie slot, the bans and ban modes, reserved slots, the per-address limit, idle kicks, speed
skew, locking, password and rcon rates, the status rate and uptime, limited rcon, name-change
limits, `sv_fps`, `sv_redirect`, `sv_mapcmd`, the command filter and blackholes, `sv_ghosttime`,
`sv_nodelta`, `sv_pingmethod`, `sv_maxents`, `sv_entpriority`, `sv_novis` (our rules),
`sv_vispatch`, `sv_entpath`, `sv_logfile`, `serverrecord` (in our file form), `gamemode`,
`listmaps`, `say` from the terminal.

Still differing, each a gap below: the status keys (S27), the classic heartbeat and `setmaster`
(S28), what rcon reaches and how its answer is cut (S29), the `sv` ban words (S30), the operator
commands `kickban`, `stuff`, `stuffall`, `stuffcvar`, `printall`, `addstuffcmd`, `dumpents`,
`killserver`, `pickclient`, the `status` letters, the full `dumpuser`, dedicated `save`/`load`
(S31), `statusx` and `rulesx` (S32), the drop and timeout broadcasts (S33), the client commands
`info`, `sinfo`, `lag` (S34), slots (S35), downloads (S36 to S39), `mvdrecord` (S40), `map` against
`gamemap` for a hosted library (S13), the move-time check for a hosted library (S12).

Asked about and not gaps for this round: `demomap` exists on the original's clients only (R); the
pak checks and the anticheat interface are build switches the release turns on, tied to an outside
service, which our notes do not take (R); flood control in the original server applies to relay
spectators only, the players' own is the game library's (R).

## B2. The protocol

### B2.1 Versions
Stock is 34. The extended server also takes 35 (the first extended client's), 36 (its own) and,
in this release, 38 (the product's own), plus 37 for multi-view recordings. The `challenge` answer
lists the protocols on offer after `p=`, and a client picks the highest it knows. (R) Ours: 34 only
(S18).

### B2.2 The channel
- Protocol 34: each packet starts with the outgoing sequence (31 bits, top bit set when a reliable
  message rides along) and the acknowledged sequence (31 bits, top bit the reliable toggle); a
  client adds a 16-bit qport. One reliable message is in flight at a time. Packets stay at 1400
  bytes. (R)
- The new channel (35 and up): the qport shrinks to 8 bits; bit 30 of the sequence flags a fragment,
  followed by a 16-bit offset whose top bit (0x8000) means more pieces follow; a message may reach
  32 KiB; the packet size is agreed at connect between 512 and 4086 bytes. (R)
- Ours: protocol 34's channel; no fragments (S18). Our channel's own ceiling is 1450 bytes (S24).

### B2.3 Compression
Protocol 36 wraps large frames and downloads in raw deflate: message 21 carries a compressed block
(its packed and unpacked lengths in 16 bits each) and message 22 a compressed download piece. The
extended messages also include 23 (the game state at once), 24 (a server setting), 25 (a stream of
configstrings) and 26 (a stream of baselines). (R) Ours: none (S18).

### B2.4 Frames and deltas
- Stock frame: the server frame number and the client's delta frame (32 bits each, -1 for none), a
  suppress count byte, the area bits with their length byte, the player state, then the packet
  entities, each with bit flags in up to four bytes and the removal bit, ending with entity number 0. (R)
- Extended frame (36): one 32-bit word holds the frame number in 27 bits and the distance back to the
  delta frame in 5 bits (0 for none); the suppress count is 4 bits; the extra flags are packed into
  the message byte and the following byte. (R)
- The delta frame must be within the last 16 frames (the frame ring); older or unknown deltas, or a
  client that asks for nodelta (by sending -1), get a full frame. The extended server limits how
  often a client may ask (`sv_allow_nodelta`). (R)
- The old path sends at most 128 entities a frame; the extended path more (the ring's room). (R)
- Ours: the stock frame, the stock delta rules, nodelta, the 16-frame ring. (R)

### B2.5 Handshake, challenge, user info, the `cmd` packet
- `getchallenge`, then `challenge <n> p=<list>`; `connect <protocol> <qport> <challenge>
  "<userinfo>"` (35 and 36 add the packet size and, for 36, the channel kind, compression and a
  minor version); `client_connect` (the extended server adds its own keys); then the client sends
  `new`, gets the server data, configstrings and baselines, sends `begin`. (R)
- The challenge: a random 32-bit number per address, in a table of 1024; the oldest is replaced. (R)
  Ours: S20.
- User info is at most 512 bytes; the server forces `ip`, cuts `name` to 15 characters with the high
  bits cleared, and sends the result to the game. (R) Ours: S10, S25.
- The `cmd` packet: a checksum byte over the packet (protocol 34), the client's last frame, then three
  moves, each a delta from the one before; the server runs only those it has not run yet, and with
  more than 2 and fewer than 20 packets lost it repeats the oldest move for each lost one. The
  extended server takes at most 32 moves and 8 string commands a packet. (R) Ours: S19.

### B2.6 Culling of entities: the visible set, the hearing set and area portals
Answer to "does a shut door cut what a client receives": yes, in the original.

- The viewer: the client entity's eye (its origin plus view offset) gives a leaf; its area and its
  fat visible set (the union over a box of 8 units round the eye) are taken. (R)
- The area bits: one bit per area, in (areas plus 7) divided by 8 bytes, each set when that area is
  in the same group as the viewer's, groups being the areas joined through open portals. A viewer
  in area 0 (inside a solid) gets every bit. (R)
- An entity is sent only when the viewer's area is joined to its first area or, for a brush entity
  that spans a portal, its second area. (R) A door sits in two areas, so the door itself is sent
  to both sides (D). Area 0 is joined to nothing in the extended server (stock: area 0 is joined to
  area 0 only). (R)
- Then: an entity whose leaves all lie outside the viewer's visible set is skipped; a beam is
  tested by its first cluster against the hearing set instead; an entity with no model that only
  makes a sound is cut beyond 400 units. (R)
- Exempt from the area and set tests: the viewer's own entity, every entity when `sv_novis` is 1, and
  the entities the game marks never-cull, on the extended protocol only. (R)
- Sounds: a positioned sound goes to a client when the client's leaf (taken from its entity's origin,
  not its eye) is in the sound point's hearing set and the two areas are joined. A sound with no
  attenuation, or one sent on a channel with the no-hearing-test bit, goes to everyone without the
  area test. A looping sound is part of its entity, so it is cut with the entity. (R)
- Multicast (temporary effects, muzzle flashes): to the visible or hearing set of the point, the
  reliable forms the same; both apply the area test; to all skips it. (R)
- The game's own tests in-visible-set and in-hearing-set also apply the area test. (R)
- Ours: S1 to S5.

### B2.7 Baselines and configstrings
Baselines and configstrings go at load; the stock server packs them into several reliable messages
(the piece size was not settled); the extended server streams them (messages 25 and 26) on 36. A
configstring longer than its slot or past the total room is an error in stock; the extended server
lets a game declare more room. (R) Ours: the stock load, the same limits; overflow of an index is
S14.

## B3. Hosting game libraries

### B3.1 The calls the server gives a game (imports), as a mod sees them
Prints: `bprintf` (to everyone, at a level), `dprintf` (the console), `cprintf` (one client, at a
level), `centerprintf`. Sound: `sound`, `positioned_sound`. Indexes and strings: `configstring`,
`modelindex`, `soundindex`, `imageindex`, `setmodel`. Errors: `error` (stops the level with the
text). World: `trace`, `pointcontents`, `inPVS`, `inPHS`, `SetAreaPortalState`, `AreasConnected`,
`linkentity`, `unlinkentity`, `BoxEdicts`, `Pmove`. Messages: `multicast`, `unicast`, `WriteChar`,
`WriteByte`, `WriteShort`, `WriteLong`, `WriteFloat`, `WriteString`, `WritePosition`, `WriteDir`,
`WriteAngle`. Memory: `TagMalloc`, `TagFree`, `FreeTags`. Settings and commands: `cvar`,
`cvar_set`, `cvar_forceset`, `argc`, `argv`, `args`, `AddCommandString`. Debugging: `DebugGraph`. (R)

What the original does that ours does not, call by call: `bprintf`/`cprintf` respect the client's
`msg` level (S6); `sound`/`positioned_sound` refuse a bad volume, attenuation or offset (S16), leave
the position out to follow the entity (S5) and test the areas (S4); `modelindex` and the other index
calls stop the server on overflow (S14); `multicast` tests the areas (S4); `BoxEdicts` returns the
tree's order (S15); `AddCommandString` runs any command (S11). The rest were checked and behave the
same in what a library can see. (R)

### B3.2 The calls a game gives the server (exports)
`apiversion` (3), `Init`, `Shutdown`, `SpawnEntities` (map name, entity text, spawn point),
`WriteGame`, `ReadGame`, `WriteLevel`, `ReadLevel`, `ClientConnect` (may refuse with a reason in
the `rejmsg` user info key), `ClientBegin`, `ClientUserinfoChanged`, `ClientDisconnect`,
`ClientCommand`, `ClientThink`, `RunFrame`, `ServerCommand` (the `sv` console word), and the entity
array with its entry size, count in use and maximum. (R)

Ours calls them all; where it differs: `ClientConnect` late (S9), `ClientUserinfoChanged` only for the
name in a game (S8), the ping given in each client is 0 (S7). (R)

The extended interface an extended library may ask for: S17.

### B3.3 The order of a server frame
Original, each server frame: time out silent clients; read every waiting packet, running each
client's moves (`ClientThink`) and string commands as they arrive, under the move-time budget; when
the frame's time has come: work out the pings and give them to the game, give each client its
budget (1800 ms every 16 frames), run the game's frame (`RunFrame`), send each client its frame
(subject to the rate), record the frame to a running server demo or multi-view recording, send a
heartbeat when due, clear the entities' one-frame events, advance the frame. Commands the game
queued with `AddCommandString` run at the end of the frame. (R)

Ours: the moves also run as packets arrive, then the game's frame, then the send (R). Differences:
the pings (S7), the queued commands (S11), the move budget for a hosted library (S12). An earlier
round noted our frame clock jittering by about 13 ms against the original's steady 100 ms; it was
not measured again this round, a stock client smooths it, and it is not listed as a gap.

### B3.4 Save and load
`save <name>` (refused in deathmatch with `Can't savegame in a deathmatch`, outside a running game,
and in single player while the player is dead) writes a folder `save/<name>/` holding the server's file (a comment, the map command and
the latched settings), the game's file (`WriteGame`) and, per level visited, the level's entities
(`WriteLevel`) and its configstrings with the portals' state; `save/current` holds the levels of the
unit in play, copied over at each save. `load <name>` copies the folder back, restores the
settings, runs the map with the load flag, and the game reads the game and level files. A level
change within a unit writes the leaving level's files and reads the entering level's if present, and
a new unit clears them. The extended server also allows saving on a dedicated server (S31). (R)
Ours: the same model for a hosted library (R; our notes count it landed); our own rules save in our
own form.

### B3.5 The server demo and multi-view recording
Stock `serverrecord <name>` writes `demos/<name>.dm2` from the server's view of the world (the
whole entity set each frame, no player view), which a stock client plays back; `serverstop` ends
it. The extended server's `mvdrecord` writes a multi-view recording (`.mvd2`: a 4-byte magic, then
blocks each led by a 16-bit length, beginning with the game state and then one block per frame with
every player's state and every entity), with limits by size, time and maps (`sv_mvd_maxsize`,
`sv_mvd_maxtime`, `sv_mvd_maxmaps`), a dummy client that holds the scoreboard, and a live relay to
viewers over TCP (`sv_mvd_enable` 2). (R) Ours: S40.

## The gaps

Marks: R read in the original (Q2PRO as AQtion, master of 2026-11-16) or in our tree, D derived from
what was read, M measured. Rank: 1 noticed at once in normal play, 2 noticed sometimes, 3 rare or
cosmetic. Areas: frame (what a client is sent each frame), host (hosting a game library), net (the
protocol and the channel), admin (settings, commands, queries), downloads, records, cinematics.
Rows our notes call landed are left out unless our code still differs.

### Frame: what a client is sent

**S1 (rank 2, frame). A shut door does not cut the entities a client is sent.**
Original: an entity in the potentially visible set is still left out when the viewer's area is not
joined to either of the entity's two areas through open portals (§ B2.6 has the whole rule). So a
monster, a player or an item in the next room behind a shut door is not sent, and neither is its
looping sound, which the client plays only for entities in its frame. (R)
Ours (2026-11-22, batch 12): `q2_sight_keeps` adds the area test for our rules and a hosted
library. Before: the frame was cut by the visible set alone; areas were not consulted, for our rules
and for a hosted library alike. Entities behind a shut door within the visible set are sent, so their looping
sounds (a generator's hum, a lift's drone, a monster's idle loop that is a looping sound) are heard
through the shut door, and a modified client sees players behind it. (R ours, D the effect)

**S2 (rank 3, frame). The area bits always mark every area.**
Original: each frame carries one bit per area (the area count rounded up to whole bytes), set for
the areas joined to the viewer's area; a viewer inside a solid or in area 0 gets every bit. The
stock client skips world leaves whose area bit is clear, so rooms behind a shut door are not drawn. (R)
Ours (2026-11-22, batch 12): `q2_write_area_bits` writes the joined areas' bits. Before: every
frame wrote 32 bytes, all ones, for our rules and a hosted library. The client draws
what lies behind shut doors; doors are opaque, so the player sees nothing different, only the cost
and the length (32 bytes against a few). (R ours, D the effect)

**S3 (rank 2, frame). Our rules' sounds and temporary effects go to every client.**
Original: a positioned sound goes to the clients whose hearing set holds the sound's point and
whose area is joined to the sound's area; a sound with no attenuation, or with the channel bit
that asks for no hearing test, goes to all. Temporary effects are multicast to the visible or the
hearing set of their point, with the same area test, or to all. (R)
Ours (2026-11-22, batch 12): `q2_sight_hears` sends our rules' sounds and effects to a viewer whose
origin is in the point's hearing set with the areas joined; a sound of no attenuation to everyone.
Effects use the hearing set, the wider one, as their multicast kind is not kept (D). Before: every
sound and effect event of our rules went to everyone; no hearing set and no area test.
A client hears sounds through walls up to their attenuation range (normal attenuation reaches about
1000 units) and is sent every explosion and trail on the map. (R ours, D the range)

**S4 (rank 3, frame). A hosted library's multicast and sounds skip the area test.**
Original: as S3, the area test applies to multicast to the visible or hearing set and to sounds. (R)
Ours (2026-11-22, batch 12): `send_to` adds the area test, and area 0 is joined to nothing.
Before: a hosted library's multicast and sound calls tested the visible or hearing set by cluster, but
not the area. Its point tests (in-visible-set, in-hearing-set, areas-joined) do apply the areas.
Our area test treats an area number out of range as joined, so a point in area 0 counts as joined
to everything, where the original never joins area 0. (R)

**S5 (rank 3, frame). A sound's position never follows its entity.**
Original: a sound from an entity carries the entity number; the position is left out when that
entity is in the client's frame (the extended server) or when the sound was given no explicit
origin and the entity is not a brush model (stock), so the client places it on the entity as it
moves. (R)
Ours (2026-11-22, batch 12): the position is left out when the client's frame holds the entity (not
a brush model, no explicit point), per client. Before: every sound carried a fixed position. A sound from a moving entity (a rocket's fly-by, a
train) stays where it began. (R ours, D the effect)

**S6 (rank 3, frame). The client's message level is ignored.**
Original: the user info key `msg` sets the lowest print level a client is sent (0 pickups and up,
1 obituaries and up, 2 chat only, 3 none); broadcast and client prints below it are not sent. (R)
Ours: every print reaches every client whatever its `msg`. (R)

### Host: running a game library

**S7 (rank 2, host). Every client's ping reads 0.**
Original: before each game frame the server works out each client's ping (by `sv_calcpings_method`,
default the least of the last 16 frames' latency) and hands it to the game, which shows it on the
scoreboard. (R)
Ours: a hosted library is told 0 for every client every frame; the stock deathmatch scoreboard shows
0 for everyone. (R)

**S8 (rank 2, host). User info changed in a game is not passed on.**
Original: a client's `userinfo` change (skin, model, hand, fov, gender, spectator, rate, msg)
goes to the game's user info call at once. (R)
Ours: with a hosted library, only a name change reaches it in a game; a new skin, hand, fov or a
spectator switch does not. (R)

**S9 (rank 3, host). The game hears of a client late.**
Original: the game's connect call runs when the connect packet is accepted, before the client
loads; a refusal reason it gives goes back as the refusal text. (R)
Ours: the connect call runs at `begin`, after loading. A refusal therefore comes only after the
player loaded the map. (R)

**S10 (rank 3, host). The `ip` key can be supplied by the client.**
Original: the server writes the client's address into the `ip` user info key on connect and on
every change, so a game library can trust it. (R)
Ours: the key is added only when missing, so a client that sends its own `ip` keeps it. A library
that bans by `ip` can be fooled. (R)

**S11 (rank 2, host). Commands a library asks the server to run are mostly dropped.**
Original: text a game hands the server for its console runs as console commands at the end of the
frame: any command, `kick`, `exec`, `set`, `sv`, the map commands, waits. (R)
Ours: only map changes, the load-game menu's line and assignments to existing settings run; any other
line is logged as not run. A mod's vote that runs `kick` or `exec` does nothing. (R)

**S12 (rank 3, host). The move-time check is off for a hosted library.**
Original: `sv_enforcetime` 1 (the extended default) holds every client to 1800 ms of moves per 16
frames, whatever library runs. (R)
Ours: `sv_speedcheck` applies to our rules only; a hosted library's clients run unchecked. (R)

**S13 (rank 2, host). `map` with players in keeps their inventory.**
Original: `map` always restarts the game library's level state for a new game (`sv_allow_map` 0
refuses it in a running game unless forced; 2 makes it `gamemap`), so items and scores are lost. (R)
Ours: the library is restarted only when latched settings wait and nobody is in; otherwise `map`
acts as `gamemap`. (R)

**S14 (rank 3, host). An overflowing index is not an error.**
Original: a model, sound or image index past its 256 slots stops the server with an error naming
the kind. (R)
Ours: it returns 0 with a note in the log; the item is drawn or played as nothing. (R)

**S15 (rank 3, host). Area-box queries return entities in another order.**
Original: the box query walks the area tree's nodes, so entities come back in the tree's order. (R)
Ours: in entity number order. A library that takes the first hit (touch order, telefrag order) can
differ. (R ours, D the effect)

**S16 (rank 3, host). A sound with bad volume or attenuation is clamped, not refused.**
Original: volume outside 0 to 1, attenuation outside 0 to 4 or a time offset outside 0 to 0.255 s
stops the server with an error. (R)
Ours: clamped into range. (R)

**S17 (rank 3, host). The extended game interface is absent.**
Original: an extended game library may ask for the extended interface (extra calls: custom user
info handling, entity flags such as never-cull, extra configstring room, and more); without it the
stock interface is used. (R)
Ours: the stock interface only. A library built for the extended one falls back or refuses. (R)

### Net: the protocol and the channel

**S18 (rank 3, net). Only protocol 34; no compression.**
Original: accepts 34, 35 and 36 (the challenge answer lists them), and compressed frames and
downloads for 36 (zlib, messages 21 and 22). (R)
Ours: 34 only; `challenge N p=34`. Modern clients fall back to 34: 1400-byte packets, no fragments,
at most 128 entities a frame on the old path. (R ours, D the effect)

**S19 (rank 2, net). No repeat of the last move under loss.**
Original (stock and extended): a client's `cmd` packet carries the last three moves; when the
server finds more than two packets lost (but fewer than 20) it runs the oldest move again for each
missing one, so a player does not freeze under loss. (R)
Ours: moves lost beyond the three carried are not run again. (R)

**S20 (rank 3, net). Challenges are weak and kept forever.**
Original: a random 32-bit challenge per address in a fixed table of 1024 entries; the oldest is
replaced; it must match on connect. (R)
Ours: a 15-bit value from a simple generator, kept per address with no limit. (R)

**S21 (rank 3, net). A reconnect from the same address and port is ignored.**
Original: a connect from an address and port that already hold a slot frees that slot and connects
again, after a short guard that ignores a duplicate arriving too soon (its exact length and text
were not settled). (R)
Ours: ignored while the slot is held, so a client that restarted must wait for the timeout. (R)

**S22 (rank 3, net). Refusal texts differ.**
Original: stock answers `Server is protocol version 34.`; the extended server names the protocols
it takes and has its own lines for a bad challenge, a full, locked or password-held server and the
game's refusal (`Server is locked.` is one; the rest are in `q2-modern-server-inventory.md`). (R)
Ours: `Server is protocol version 34.` for any other protocol; `Server is locked.` and the address
limit's lines now stand alone as the original's do (2026-11-22, batch 9); the password lines are the
stock library's. (R ours)

**S23 (rank 2, net). A large reliable message drops the whole frame.**
Original: when the reliable data and the frame do not fit, the stock server writes the frame
anyway up to the buffer (overflowing clients are dropped); the extended server first drops sounds
and effects of low priority, then entities in order of priority, and sends what fits. (R)
Ours (2026-11-22, batch 12): the frame is sized to the packet's room after the reliable part;
entities fill first in number order, then effects and sounds; a hosted library's datagram whole or
not at all; no per-sound priority. Before: the unreliable part was dropped whole when the packet
would pass 1450 bytes, so a frame was
lost (D: a stutter when a long configstring or print arrives). (R ours)

**S24 (rank 3, net). Packets may pass the stock client's buffer.**
Original: the stock client reads into a 1400-byte buffer; the server keeps each packet at 1400. (R)
Ours: the channel's limit is 1450 bytes. Whether a packet over 1400 is ever sent was not settled
(the frame writer budgets its own room as 1400 minus 202 bytes, but the reliable part is added on
top). Open. (R ours)

**S25 (rank 3, net). Names are not cut.**
Original: the server cuts `name` to 15 characters and clears each character's high bit. (R)
Ours: not cut by the server. (R)

**S26 (rank 3, net). A disconnect is not repeated by unicast.**
Original: on a drop the server sends the disconnect again outside the reliable stream so a client
that lost it still leaves; the extended one also keeps the slot as a zombie that answers with the
disconnect. (R)
Ours: the zombie slot resends (landed); the extra out-of-channel copies are not sent. (R)

### Admin: settings, commands and queries

**S27 (rank 2, admin). `status` lacks most server info keys.**
Original: the server info carries every server-info setting: `dmflags`, `fraglimit`, `timelimit`,
`cheats`, `deathmatch`, `coop`, `maxclients`, `maxspectators`, `hostname`, `mapname`, `version`,
`gamename`, `gamedir`, `protocol`, `uptime` when set, `needpass`, and any key an admin adds with
`set <key> <value> s`. (R)
Ours: settled 2026-11-22 (batch 9): the rules' live `deathmatch`, `coop`, `dmflags`, `fraglimit`,
`timelimit` and `cheats`, the build's `version`, `gamename`, `gamedir` (and `game` for a mod), `port`
and every key an administrator added with `sets` (`net/q2_admin.cpp`, `q2_status_keys`). The
engine's `serverinfo` word still lists our own names. (R)

**S28 (rank 2, admin). No classic master heartbeat.**
Original: `public` 1 sends heartbeats every 300 s to the masters in `setmaster` (and the default
master), with the status and player lines; `listmasters` shows them. (R)
Ours: our own listing only; classic server browsers never see the server. (R)

**S29 (rank 2, admin). rcon reaches only part of the server.**
Original: rcon runs any console command and returns the output in packets of at most 1384 bytes
each. (R)
Ours: rcon reaches Quake II's admin words and the rules' console only; not the engine console
(`ban`, `serverrecord`, `serverinfo`), nor `listmaps` and `gamemode` (terminal only). Its answer is
one print packet, not cut into pieces. (R)

**S30 (rank 2, admin). `sv addip` and the other `sv` words do not reach the ban list.**
Original: the stock game's `sv addip`, `sv removeip`, `sv listip`, `sv writeip` and the
`listip.cfg` that `exec`s them. (R)
Ours: settled 2026-11-22 (batch 9): the `sv` forms reach the engine's ban list in the game's own
words (`net/q2_admin.cpp`, `sv_filter`), and a `listip.cfg` of `sv addip` lines and `set filterban`
is read back at the start with its mode. (R)

**S31 (rank 2, admin). Missing operator commands.**
Original (extended server): `kickban`, `stuff`, `stuffall`, `stuffcvar`, `printall`,
`addstuffcmd` (and its list), `dumpents`, `killserver`, `pickclient`, `status` with its letter forms
(`status l`, `status t` and so on), `dumpuser` with the extra fields, and `save`/`load` on a
dedicated server. (R; the full forms are in `q2-modern-server-inventory.md`)
Ours: none of these. (R)

**S32 (rank 3, admin). The extended queries.**
Original: `statusx` and `rulesx` out-of-band queries. (R)
Ours: not built (noted in `q2-server-gaps.md`). (R)

**S33 (rank 2, admin). No broadcast when a player times out or is dropped.**
Original: `<name> timed out` and `<name> was dropped` (and kick's `<name> was kicked`) to everyone. (R)
Ours: a note on the console only. (R)

**S34 (rank 3, admin). The client commands `info`, `sinfo` and `lag`.**
Original: `info` returns the server info to the asking client; the extended server adds `sinfo`
(its features) and `lag` (the client's own ping and loss). (R)
Ours: none. (R)

**S35 (rank 3, admin). Slots capped at 32; default 8 not taken.**
Original: up to 256 slots, stock and extended; the extended server's `maxclients` default is 8. (R)
Ours: 32 slots; the default is 8, and a deathmatch server of one slot or fewer is given 8
(2026-11-22, batch 9); coop's 4 and single player's 1 are not applied, the slots being fixed when
the server starts. (R)

### Downloads

**S36 (rank 2, downloads). No HTTP download address.**
Original: `sv_downloadserver` tells clients a web address to fetch content from. (R)
Ours: open (`sv_downloadurl` in `q2-server-gaps.md`). (R)

**S37 (rank 3, downloads). Download kinds and the size limit.**
Original: `allow_download_textures`, `allow_download_pics`, `allow_download_others`, and
`sv_max_download_size` (bytes; larger files refused). (R)
Ours: players, models, sounds and maps only; no size limit. (R)

**S38 (rank 3, downloads). The "File size differs" message.**
Original: a resume offset past the file's size answers with a print telling the client the file
size differs and starting again from 0. (R)
Ours: refused with no such message. (R)

**S39 (rank 3, downloads). Piece size and pace.**
Original: stock sends 1024 bytes a frame; the extended one sends asynchronously, each piece the
packet's room less 4 bytes (about 1386), several a frame within the rate. (R)
Ours: 1024 bytes a piece. (R)

### Records

**S40 (rank 3, records). No multi-view recording; no stock server demo.**
Original: `mvdrecord` writes a multi-view recording (`.mvd2`) of every player; `sv_mvd_enable` 2
serves it live over TCP to relay viewers; stock `serverrecord` writes a `.dm2` that stock clients
play. (R)
Ours: `serverrecord` writes our own replay file (`.ged`); a multi-view writer exists but no
`mvdrecord` command, and no TCP relay. (R)

### Cinematics

**S41 (rank 2, cinematics). Cinematic and picture levels are not served.**
Original: a map name ending `.cin`, `.pcx` or `.dm2` (the units' intro and end sequences) puts the
server in a cinematic or picture state; the client plays it and sends `nextserver`, which runs the
next level in the chain (`+` names it). In coop the units' end pictures play. (R)
Ours: the cinematic states and `nextserver` are not served to stock clients; such levels are
skipped. (R)

### Count

| Area | Rank 1 | Rank 2 | Rank 3 | All |
| --- | --- | --- | --- | --- |
| frame | 0 | 2 | 4 | 6 |
| host | 0 | 4 | 7 | 11 |
| net | 0 | 2 | 7 | 9 |
| admin | 0 | 6 | 3 | 9 |
| downloads | 0 | 1 | 3 | 4 |
| records | 0 | 0 | 1 | 1 |
| cinematics | 0 | 1 | 0 | 1 |
| all | 0 | 16 | 25 | 41 |

### Checked and the same

- Protocol 34's frame: the delta from a client-chosen frame, nodelta (frame number -1), the entity
  bit flags in up to four bytes, the player state delta, the removal bit, the baselines sent at
  load, configstrings sent at load and later as reliable messages. (R ours, R original)
- The channel of protocol 34: 31-bit sequences with the reliable bit, the acknowledgement with its
  reliable bit, the 16-bit qport from the client, one reliable message in flight. (R)
- The handshake: `getchallenge`, `challenge`, `connect <protocol> <qport> <challenge> "<userinfo>"`,
  `client_connect`, then `new`, the configstrings, the baselines, `begin`. (R)
- The visible set cut with the fat set (a box of 8 units around the eye), beams tested by the
  hearing set from their first cluster, and the sound-only entities cut beyond 400 units. (R)
- `sv_novis`, `sv_maxents` and their order of priority (our rules), the rate check over 10 frames,
  the zombie slot, `timeout`, the bans and the ban modes, reserved slots, `sv_iplimit`, the password
  and rcon rates, limited rcon, `sv_fps` for our rules, `sv_mapcmd`, `sv_redirect`, `sv_logfile`
  (all landed per our notes; spot checks agree). (R)
- Save and load of a single-player game for a hosted library: the level files and the game file
  written at each level change and on `save`. (R)
- The point tests a library calls (in-visible-set, in-hearing-set, areas-joined) apply the
  areas and the portal state the library sets. (R)
