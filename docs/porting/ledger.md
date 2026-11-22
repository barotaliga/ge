# The ledger: what each game gave, what we left behind, what is ready

Why Shooter's Edge ports many games into one shell: every game is better at something, and once
its better part is in a shared layer, every game has it. Every game is also worse at something,
and that part is never carried into another game. This page is the running account. A port adds
its rows here in the PR that lifts or leaves each thing; each system's page has the details.

## What each game gave every game

### The first classic game (with its later clients, servers and community tools)

| Benefit | System | Now in |
| --- | --- | --- |
| Client prediction, other players run forward | movement | `plugins/shooter/predict.hpp`; Quake II's own predictor |
| Extensions negotiated by bit, stock ends untouched | network | `net/qw_ext.hpp` and the `ge` set in both games |
| Downloads, chunked, kept in the archive they came from | content, network | `engine/vfs/partial.hpp`, `plugins/classic/content/downloads.hpp` |
| Multiview recordings, TV with a delay, relays, forward | demos | `engine/tv`, `engine/forward`, each game's face |
| Seeking, speed, highlights, autotrack, the radar, the live map | demos | `plugins/shooter/demo_transport.hpp`, `engine/replay`, `engine/overview` |
| Place names and team chat codes | features | `plugins/classic/locs.hpp`, `client/team_chat.hpp` |
| The quick weapon, weapon groups, preselection | features | `plugins/shooter` |
| Chat ignore lists, console scripting | client | `engine/console` |
| Rates, bans, rcon, flood rules, overtime, timeouts, votes, prewar, the home map | server | `engine/server` |
| Lag compensation for hitscan and missiles: levels, a cap, the box rewound, teleports never blended, a player's opt-out | network | `net/qw_antilag.hpp`, both games' servers |
| Gib, body and flame filters | models | `client/model_filters.hpp` |
| Stand-in pictures by name, alpha and all | content | the shared adapter calls |
| Accounts, masters, a website of live games | online | `engine/account`, `engine/hub` |

### Quake II (with its later fork)

| Benefit | System | Now in |
| --- | --- | --- |
| Coloured light | picture | `plugins/brush/app/bsp_draw.hpp` for any family with RGB lightmaps |
| A power's shell, an item's pulse | picture | `r_powershells`, `r_itempulse`; on in Quake II, off in Quake |
| The landing dip | movement | `engine/camera` (`cl_landdip`) |
| Collision exact to the authored brush | maps | the one trace (`MoveWorld`'s `brushes`) |
| Weapons seen in other players' hands | models | `plugins/morph/carry.hpp`; Quake's figure gets one built from its pack |
| Entity loop sounds | sound | the shared beds |
| Its fork's server settings: VIP slots, idle drops, speed-skew kicks, rename limits | server | `engine/server` |
| Footsteps | sound | Quake II; Quake in remix only (it ships no step sounds) |
| Faster rules frames (`sv_fps`) | server | Quake II's rules; the old beat kept for its world |
| Quake II's multiview recorder served live to relays over TCP (`mvdrecord`, the access lists with hits) | server, demos | Quake II's endpoint; the address list shared (`plugins/classic/protocol/addr_list.hpp`), the streaming deflater the engine's |
| Quake II's multiview relay: other servers' streams and `.mvd2` files as channels, the Waiting Room, a chase view per viewer, chat with its flood rule | server, demos | Quake II's endpoint; the conversion's fork (`client/demo_convert.hpp`) and the TV face's moves shared with any relay of a classic game |
| Quake II's later protocols (35, 36): the newer channel's pieces, a deflated signon, batched moves, a client's no-gibs and no-footsteps asks | net, server | Quake II's endpoint; the channel's pieces and the deflater shared (`plugins/classic/protocol/channel.hpp`, `engine/core/deflate.hpp`) |
| The modern server's operator words (status tables, stuff lists, partial names, kickban) and player queries (`lag`, `sinfo`) | server | `engine/server/operator.hpp`, Quake II's endpoint; Quake's to mount |
| Stock maps' visibility mended, a map's entities from a file (`sv_vispatch`, `sv_entpath`) | maps, server | Quake II's server (`server/q2_mapfix.hpp`) |
| Shots from the gun onto the crosshair's point (`aimfix`), a level machine gun (`g_machinegun_norecoil`), footsteps by pace (`g_footsteps`) | rules | Quake II's rules (`server/q2_fight.cpp`) |
| The modern client's console words for every game: `_echo`, `text`, `complete`, `setu`, `sets`, `resetall`, `writeconfig`, `toggle` over a list, the clock macros, `draw` strings, local `play` and `stopsound`, `sizeup` and `sizedown`, Alt+Enter | client | `engine/console`, `engine/hud/draw_strings.hpp`, `engine/platform/window_cvars.hpp`, the shell |
| The connection's macros (`$cl_ping`, `$cl_lag`, the rates), `serverstatus`, the files' words (`path`, `dir`, `fdir`, `whereis`), `dns`, `net_stats` for every game | client | `engine/net/rates.hpp`, `engine/net/net_commands.hpp`, `engine/vfs/search_commands.hpp`, the shell's join |

### Ours, offered to every game

Classic enhance's switches (textures, light, liquids, sky, shadows, effects, models, sounds, room
acoustics); voice chat and `say_near`; the easy jump, the blast-jump helper, aim help; co-op
companions; saves of the whole running game; C++ mods on any rules; bots that learn from people's
recordings; one config for the series; ranked ladders and matchmaking; a demo as a video with sound;
the campaign's episodes opened one after another (`sv_episodes_in_order`, off by default: the
original opens them all).

## What we left behind

| Drawback of the original | Game | Left behind by |
| --- | --- | --- |
| Game logic in a scripting language, or a native library that can do anything | both | our rules in C++; the VMs and the library host only for the community's mods |
| A fixed 8-bit picture | both | the modern look; the classic look kept by choice |
| Other players seen at the server's 10 Hz | Quake II | moves between frames and run forward, for our client |
| Figures stepping at 10 Hz | Quake II | eased in the window, blended between frames |
| A server's own move mispredicted | both | the move's feel told by `move-feel` |
| A speed cheat gaining time | both | cut to the time that passed, scored, named, banned by account |
| A demo watched only from the start | both | keyframes and seek |
| One format a game, unreadable elsewhere | both | conversion; replay file v2 as the shared form |
| Downloads written loose, lost on a drop | both | partial archives |
| A disc in the drive for music | both | the soundtrack ripped once to FLAC |
| One config a game, a name two ways | both | one series config, synonyms per game, `own` meanings |
| A newer game's look leaking into an older one by a shared name | both | the cvar layers |
| Collision only on fixed hull sizes | Quake | hulls from brushes, one trace |
| Waypoints hand-made per map | both | nav proven by the move |
| A name anyone can take | both | names reserved per account |

What is NEVER a drawback to fix: a game's own numbers (its move, its weapons' damage and timing,
its monsters), its look in the classic look, and its protocol on the wire to a stock end. Those are
the game, and a remix or a switch changes them only when the player asks.

## Ready for the next games

What a port to the third classic game (the arena game, map family 46) and to the first sector-family
game finds already built. READY: built and tested for another game and expected to serve it
unchanged; PART: a piece exists; NONE: nothing yet.

| System | The third classic game | The sector family |
| --- | --- | --- |
| Content | PART: zips in name order (`engine/vfs`); no content row | PART: its archive and palette read (`engine/vfs/wad.hpp`, `palette.hpp`); no content row |
| Maps | PART: 46 read with patches; collision on patches NONE | NONE: the conversion to 3D is research (`docs/porting-sprite-games.md`) |
| Picture | PART: the world renderer takes any family; its shaders NONE | NONE: sector light, sprites in the world |
| Models | PART: its model family read; tags joined NONE | NONE: sprites in eight rotations |
| Held weapons | NONE | NONE: a 2D path |
| Sound | READY for WAV; other formats NONE | NONE: its sound lumps |
| Movement | NONE: a reader's facts first | NONE |
| Network | NONE: its protocol | NONE: lockstep exists in `engine/net` |
| Server | READY: the core and every shared rule | READY: the core |
| Rules and mods | PART: its mods' compiled modules run (`plugins/progs/qvm.hpp`, `server/qvm_server.hpp`); our rules NONE | NONE |
| Client | PART: the adapter's column is written (`docs/shooters-edge.md` § The game adapter) | NONE |
| Settings | PART: one name's meaning settled (`cg_shadows`); no census | NONE |
| Demos and TV | READY: the shared transport, TV, replay file v2; its format NONE | as the third |
| Bots | READY: the kit's shared parts | PART: nav over the converted map |
| Online | READY | READY |
| Small features | per row of `features.md` | per row |
