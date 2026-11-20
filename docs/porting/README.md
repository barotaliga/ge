# Porting classic games, system by system

For whoever brings the next classic game into Shooter's Edge (the third classic game, the arena
game of the 46 map family; or Quake of the sector family, whose maps are a flat plan of
sectors and whose figures are pictures). Each page here takes one SYSTEM of a game (its maps, its
picture, its network, its server, a small feature such as the weapons seen in other players'
hands) and says, for that system alone:

- **What is shared, and where it lives**: the engine, the shooter kit (`plugins/shooter`), the
  classic plugin (`plugins/classic`), classic enhance (`plugins/classicenhance`), the readers
  (`plugins/brush`, `plugins/morph`, `plugins/progs`), with each file's path, so `make find` and the
  file's own `ge:` tags lead on.
- **What is each game's**: the file of the first classic game's and Quake II's beside it, which
  the next game's copies the shape of.
- **How to add a game**: the hook points a new game fills, in order, each with its test and lab.
- **What each game brought** that every game now has, and **what we left behind**: a drawback of
  the original that no game here carries.
- **The gaps** still open, and the system's labs and lessons.

The pages do not repeat the rules they stand on; they link them:

| Page | What it settles |
| --- | --- |
| [`docs/porting-games.md`](../porting-games.md) | THE LAYER RULES: the five layers, where a piece and a setting go, one name one meaning, the bot split, do not over-generalize, improvements offered to every game, remix, installs, downloads, Quake II's lessons |
| [`docs/shooters-edge.md`](../shooters-edge.md) § A third classic game: the checklist | THE ORDER of a port's thirteen steps; § The game adapter: the adapter's calls game by game |
| [`docs/porting-sprite-games.md`](../porting-sprite-games.md) | The sector family's research: the map converted to 3D, sprites, its risks |
| `clean-room` skill | How the original's behaviour is learnt without reading code we may not copy |
| `plugins` skill | The classification test for a new file's home |

## The systems

| System | Page | In one line |
| --- | --- | --- |
| The ledger | [`ledger.md`](ledger.md) | Every game's benefits taken by all, the drawbacks left behind, and how ready each system is for the next two games |
| Content | [`content.md`](content.md) | Installs, discs, packs and archives, the search path, load orders, known releases, the soundtrack, downloads |
| Maps | [`maps.md`](maps.md) | The map families into one view, collision and the trace, movers, entities, place names, the map tools |
| Picture | [`picture.md`](picture.md) | The two looks, the world's draw, light, sky, liquids, fog, weather, the effects, the quality ladder |
| Models | [`models.md`](models.md) | Figures, skins, frames and poses, items, gibs, the weapons in other players' hands |
| Held weapons | [`held-weapons.md`](held-weapons.md) | The first-person weapon: the game's own models, placed, lit, fired; the free set |
| Sound | [`sound.md`](sound.md) | Recorded sounds in space, ambience, the room's acoustics, music, menu clicks, voice |
| Movement | [`movement.md`](movement.md) | Each game's player move, prediction, the move's feel told, the aids (easy jump, blast-jump helper) |
| Network | [`network.md`](network.md) | The protocol family, each game's protocol, our extensions on the wire, interop with stock clients and servers |
| Server | [`server.md`](server.md) | The server core, the endpoints, settings in layers, admission, rates, admin, match rules, anti-cheat |
| Rules and mods | [`rules.md`](rules.md) | The rules runner, our rules in C++, the VMs and game libraries for community mods, C++ mods, campaign, co-op, saves |
| Client | [`client.md`](client.md) | The window, the adapter, controls, the console's layers, the HUD, the menus, text |
| Settings and commands | [`settings.md`](settings.md) | How a game's cvars and commands are ported: the census, the gaps, the rounds, the name check |
| Demos, TV and video | [`demos.md`](demos.md) | Recording formats, playback and seeking, conversion, highlights, TV, proxy and forward faces, video |
| Bots | [`bots.md`](bots.md) | What a bot shares across games and what each game's bot owns |
| Online services | [`online.md`](online.md) | Accounts, logins, the master and browser, the hub, voice, lobbies, matchmaking |
| Small features | [`features.md`](features.md) | A catalogue of the small things (visible weapons, cheat checks, arrivals, the live map, place names, aids, filters...): home, games, default |

## The layers

```
 Shooter's Edge shell  games/shooters_edge/  menus · the game picker · browser · remix · hud_style
        │
 game modules          each classic game's own files beside the others' (classic_*, q2_*;
        │              the next game's <game>_* beside them): profile rows, adapter, rules,
        │              protocol, demo format, status bar, poses, menus' art
        │
 classic enhance       plugins/classicenhance/   one ce_ switch per effect over the player's packs
 classic plugin        plugins/classic/          what only imported classic games need: content
        │                                        layer, protocol family, locs, item roles, looks
 readers and VMs       plugins/brush · plugins/morph · plugins/progs
 shooter kit           plugins/shooter/          what every shooter shares: move, loop, trace,
        │                                        prediction, controls, demo transport, bot parts
 engine                engine/                   any genre: render, audio, input, console, vfs,
                                                 net, server core, TV, hub, accounts, replays,
                                                 capture, overview, viewmodel, detail, mods
```

Dependencies run down only. A piece rises a layer when the next game needs it unchanged (its
improvements would be wanted by every game that has it), never before, and never with an `if` on
the game (`docs/porting-games.md` § The core rule). A port page says, for its system, which piece
sits in which layer today.

## How a game's best parts reach every game

The point of porting many games into one shell is that each game's strengths become every game's,
and none of its weaknesses do. Every port runs the same loop for every system:

1. **Census.** A clean-room reader writes the game's facts for the system in our words
   (`porting/shooters-edge/<topic>.md`, tagged ORIGINAL release or LATER fork where both exist:
   the server-gaps thread's first inventory was a modern fork's and had to be split).
2. **Map against the shared layer.** For each fact: already shared (use it, add only the game's
   wiring), shared with a difference (a field of the profile row, a table entry, a wrapper; never
   an `if`), or the game's own (a file of the game's beside the previous game's).
3. **Ledger.** What the game does better than the games before it goes in [`ledger.md`](ledger.md)
   as a BENEFIT, with the layer it is lifted to. What it does worse goes in as a DRAWBACK, with the
   shared piece that already does it better.
4. **Lift the benefits.** A benefit's mechanism goes to the lowest layer every game shares, under
   one name of ours; it is ON in the game it came from and OFF in a game that never had it, so
   every game still looks and plays as it did until the player turns it on
   (`docs/porting-games.md` § Improvements one game had). A benefit two games already have under
   two spellings is one setting of ours with each spelling a synonym in its game's layer.
5. **Leave the drawbacks.** The new game is built on the shared piece that does better: its
   limits are not reproduced unless the original's behaviour IS the game (its move's numbers, its
   weapons' timing, its protocol for stock clients). Where interop needs the limit (a stock client
   speaks only the original protocol), the limit stays on that wire and our extension lifts it for
   our own client, offered by a bit a stock end never sees (`network.md` § Our extensions).
6. **Prove it.** The earlier games' goldens and lab digests do not move when a lift only moves
   code; a lift that changes behaviour rewrites them in the same PR and says why. Each lifted
   feature gets one test per game that has it.

## Writing a page

Each page keeps this order, so an agent finds the same thing in the same place on every page:

1. **Scope** (one paragraph): what the system is and where the player meets it.
2. **Shared**: a table of layer, file, what it gives.
3. **Each game's**: a table of Quake's, Quake II's, and what the third and the sector
   family will need.
4. **Adding a game**: the hook points, in order.
5. **Brought and left behind**: the system's rows of the ledger.
6. **Gaps**, **labs and tests**, **lessons**.

Product names stay out of these pages as they stay out of every page outside `porting/`: the first
classic game, Quake II, the third classic game (the arena game), the sector family. A page names
a file by its path and a setting by its name; a path that moves is fixed in the page in the PR that
moves it (`make find` finds stale paths).

When a port teaches a lesson, it goes in the owning skill's `.lessons/` and, when it is about where
a system's pieces go, in that system's page under **Lessons**.
