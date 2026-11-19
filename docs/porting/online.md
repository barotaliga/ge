# Online services: accounts, logins, the master and browser, the hub, lobbies, matchmaking

What surrounds a match for every classic game alike: one account across every game, logins on
servers, the server list from masters and the LAN, hosting from inside the window, the hub's
website of live games and demos, the lobby, ranked play, presence and invites. All of it is the
engine's; a game brings only its protocol's few lines.

## Shared

| Layer | File | What it gives |
| --- | --- | --- |
| Engine | `engine/account/` (`store.hpp`, `service.hpp`, `ticket.hpp`, `names.hpp`, `matches.hpp`, `ladder.hpp`, `matchmaker.hpp`, `friends.hpp`, `client_login.hpp`, `client_presence.hpp`) | One account across every game, names reserved by a key folded per game, tickets checked offline by a server, one-time login codes, match reports, ladders, the matchmaker, friends and invites (`docs/accounts.md`) |
| Engine | `engine/server/login.hpp`, `accounts.hpp` | Logins on a game server: the console line, its own service or its master |
| Engine | `engine/net/master.hpp`, `server_list.hpp`, `directory.hpp`, `discovery.hpp`, `status.hpp` | The master (servers, TV nodes, forwarders beat to it), the server list, LAN discovery, a node's status record |
| Engine | `engine/net/hosting.hpp`, `reach.hpp`, `rendezvous.hpp`, `portmap.hpp` | Hosting from inside a game, NAT traversal, opening a port on the router |
| Engine | `engine/net/session.hpp`, `engine/server/match.hpp`, `listen_console.hpp` | The lobby: seats, teams, ready, warmup, the countdown |
| Engine | `engine/net/report.hpp` | A finished match's report with the game's own columns |
| Engine | `engine/hub/` (`spec.hpp`, `poller.hpp`, `api.hpp`, `pages.hpp`, `watch.hpp`, `radar.hpp`, `archive.hpp`) | The hub: a game's `HubSpec`, the poller, the JSON API, the pages, the archive by hash, watching in a browser (`docs/hub.md`) |
| Engine | `engine/web/`, `engine/webtransport/` | A game's viewer in the browser; WebTransport sessions |
| Engine | `engine/forward/`, `engine/proxy/` | Forward and proxy hops (`docs/tv.md`) |
| Engine | `engine/services/` | Platform stores: achievements, stats, cloud, presence, Discord (`docs/services.md`) |
| Shell | `games/shooters_edge/net/classic_browse.hpp`, `menu/servers_page.hpp` | The classic status query and master list feeding the engine's list; the browser page |
| Shell | `games/shooters_edge/client/listen.hpp`, `lobby_room.hpp`, `lobby_console.hpp`, `host_dedicated.hpp` | The listen server, its lobby, a dedicated server started from the host page |
| Shell | `games/shooters_edge/hub/`, `web/` | The game's hub binary with its modes and words; the web viewer |

## Each game's

| | The first game | The sequel | Next |
| --- | --- | --- | --- |
| Login | `net/classic_login.hpp` (`*auth`, the line taken) | `net/sequel_login.hpp` (its key tag in a message of its own, `account-key` bit) | its login lines |
| Browse | the classic status query | its status query | its query |
| Match report | `net/classic_report.hpp`, `classic_stats.hpp` | from its rules' tally | its tally |

## Adding a game

1. Its status query in the browse layer, so the engine's list shows its servers.
2. Its login: the server says its key tag only to a client that asked (an extension bit); our window
   trades its ticket for a one-time code and sends the login line. A stock server never says a tag,
   so nothing is tried there.
3. Its match report's columns.
4. Its `HubSpec` (modes, words), its live map and radar for the hub.

## Brought and left behind

| Benefit, now every game's | From | Layer |
| --- | --- | --- |
| Accounts and stats across servers | the first game's community services | `engine/account` |
| Masters and server browsers | both games' communities | `engine/net`, each game's query |
| A website of every live game and demo | the first game's community sites | `engine/hub` |
| Ranked ladders and matchmaking | ours | `engine/account` |

| Drawback left behind | How |
| --- | --- |
| A name anyone can take on any server | names reserved per account, folded per game |
| A password typed to each server | a ticket checked offline, one-time codes |

## Labs and tests

`se-login`, `se-browser`, `se-hub`, `match-report`, `hub-viewer`, `net-session`, `net-reach`.
