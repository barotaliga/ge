# Accounts and stats: one player, every game, every server

A player makes ONE account and uses it on every server of every game on the engine: the name is
theirs everywhere, they log in from our own clients and from the classic games' stock clients, and
every match they play adds to their stats, kept per game and shown on the hub. Row M59
([`goals/M59.md`](goals/M59.md)). This page is the design; nothing is built yet.

The account lives with the MASTER (`engine/net/master.hpp`), the service every server already
beats to and every hub already reads, so a community runs one more thing in a process it already
runs, not a new one. The service M17j planned (`ge-service`: accounts, friends, presence, the
matchmaker) keeps its friends and presence; its accounts are this page's.

What the classic games' own servers do about logins and stats, read in the clean room, is the
working note `porting/shooters-edge/accounts.md`.

## What a player gets

- **One name, reserved.** Signing up takes a name no one else holds in any game. On a server that
  reserves names, a guest who takes it is renamed and told how to log in; it is never shown beside
  someone else's frags.
- **One account across games.** The same account logs in to a Shooter's Edge server whichever
  classic game it plays (the first classic game, the sequel, a later third), and to Fall Line. The
  game is part of every stat's key: frags in one game never add to another's.
- **Logging in from any client.** Our own clients log in once and carry a signed ticket to every
  server. A stock classic client types one line in its console (§ Logging in from a classic
  client).
- **Stats on the hub.** A Player page per account: matches, frags, deaths, the game's own columns
  (a weapon's accuracy, damage given and taken), totals per game and mode, the recent matches with
  their demos; a Match page per finished match; Leaders per game, mode and column.

## The games

Every game names itself to the account service by a GAME ID (the protocol id `packet.hpp` already
gives each game) and says three things through its `ServerGame`:

| What | Why it is the game's | The first classic game | The sequel | A third, later |
| --- | --- | --- | --- | --- |
| How a name is folded for the reservation (`fold_name`) | Each game writes names its own way | lower case, the high bit stripped (red text), `0`→`o`, `1`→`l`, look-alikes folded | lower case, look-alikes folded | the same plus its colour codes stripped |
| The longest name it can show | A name must fit every game's player list | 31 | 15 | to be read |
| How a stock client is asked to log in and answers | Each protocol prints and forwards commands its own way | a console print; the player's line comes as chat (`say`) or as `cmd login`, from connecting on | a console print of at most 38 characters; a word the client does not know is forwarded only once in the game, so the seat enters as a guest first, or types `cmd login` while connecting | to be read |
| Where the login is shown | The game's own scoreboard can show it | the `*auth` user info key, which the community's client draws as the login name with a marker | the name alone | to be read |

An account's name is checked against every game the master serves: 2 to 15 characters of letters,
digits, `_`, `-` and `.` (no space: a console line splits on it), so it fits the strictest game's
list as it is. Nothing in the account, the ticket or the report is shaped by one game: adding a game is a game id, a folding rule
and its prompt.

## The parts

```text
 our client ──sign in (sealed)──▶ MASTER + accounts ◀──login ask, reports (sealed)── SERVER
      │                              │      ▲                                      ▲   │
      │ ticket (signed)              │      └── sign up, codes ─────────────── browser
      └──────── connect + ticket ──────────────────────────────────────────────────┘   │
 classic client ── "login niclas K7QF-29MD" (console line, plain) ─────────────────────┘
                                     │ reports, totals
                                     ▼
                                    HUB: Player, Match, Leaders, Account pages; /v1/players ...
```

| Part | Home | What it is |
| --- | --- | --- |
| The account store | `engine/account` (new module) | Accounts, names and their folded keys, Argon2id hashes, one-time codes, the throttle; an append-only log on disk replayed at start, compacted into a snapshot. Pure functions over a clock passed in, so a lab runs it on the simulated network |
| The ticket | `engine/account` | What a logged-in player carries: account id, name, issued and expiry (24 h), the master's public key (good for every game); signed with the master's Ed25519 key. Any server checks it with the master's public key, offline |
| The account service | `engine/account`, run by the master's process (the hub's `--master`, or a master alone), or by a server that is its own master | Sign up, sign in, tickets, a server's login ask, codes, the reports and each account's totals, over a sealed channel (X25519 to the master's published key, then the network's XChaCha20-Poly1305) |
| The login seam | `engine/server` | A seat's login state (guest, asked, logged in as an account), the prompt's timer, the reserved-name rule, the settings below; the game supplies its words and how its clients answer |
| The match report | `engine/net/report.hpp` | The record of one finished match, generic (§ The match report); the replay file's closing summary carries the same record |
| The pages | `engine/hub` | Player, Match, Leaders and Account; the JSON API beside them |

## The account

| Field | What |
| --- | --- |
| id | 64 bits, never reused |
| name, folded name | as the player wrote it; the key the reservation is held by |
| password | Argon2id (Monocypher), 64 MiB, 3 passes, a 16-byte salt; only the hash is kept |
| recovery code | shown once at sign-up, kept as a hash: the way back to an account with no e-mail |
| created, last seen, blocked | |

No e-mail address, no real name, no address kept past the throttle's window. Deleting an account
frees its name after 30 days and turns its reports' entries into "a deleted player".

**The throttle.** Wrong secrets for an account hold it for 1 s, then twice as long each time up to
15 minutes, and the same per address. The classic servers block an account for good after 10
failures, which lets anyone lock anyone out; a hold that runs out does not.

## Logging in

### From our own client

The client signs in to the master once (the name and password over the sealed channel, never in
the clear) and gets a TICKET. The game CLI's `account signin` keeps it in the player's account
file (`<state>/account.txt`, readable by its owner alone; `engine/account/account_file.hpp`) with
the master's address and key; the game's own client reads the same file.

On the classic protocol the ticket never goes to the game server, since the protocol encrypts
nothing and a ticket read off the wire would log anyone in until it runs out. Instead:

1. A server that keeps logins puts `*accountkey` in its server info: the first 16 hex digits of
   the key its tickets are signed with (its own service's, or its master's once known).
2. Our client, once in the game on a server whose tag is its file's master's, asks that master
   over the sealed channel, on a socket of its own, to trade the ticket for a one-time login code
   (`ticket_code`, `engine/account/client_login.hpp`).
3. It sends the server `login <name> <code>`, the line a player would type, and the server logs it
   in as it logs in a classic client. The code works once, so reading it off the wire is no use.

The player types nothing and sees `Logging in as Niclas.`, then the server's `Niclas logged in.`
A server of another master, or one that keeps no logins, is joined as a guest; a ticket that has
run out is said on the console (`Your sign-in has run out. Sign in again.`). The game CLI's
`account code` uses the same trade, so a player who signed in once gets a code for a stock client
without the password.

A future protocol of our own that seals its connect can carry the ticket itself: the server checks
the signature with the master's public key (`sv_accountkey`) and the expiry, and logs the seat in
before it spawns, with no round trip to the master.

### From a classic client

A stock client of either classic game cannot carry a ticket, so it logs in through the console:
the server prints the prompt, the player types a line, and the server asks the master over its own
sealed channel. The answer is a ticket the server keeps for the seat. Until then the seat is a
guest. The line the server takes, in each game:

- **The first classic game.** The community's client sends a console line whose first word it does
  not know as chat, from connecting on. So `login niclas K7QF-29MD` typed in the console reaches the
  server as a `say`, and `cmd login niclas K7QF-29MD` as a command; both work. The classic servers'
  own two-step answer (the login on one line, `Enter password for <login>:`, the secret on the next)
  works too, so a player's habit carries over. A seat that must log in sends no chat until it has.
- **The sequel.** Its stock client forwards a word it does not know only once the player is in the
  game, and its stock game turns an unknown word into public chat. So the seat enters as a guest
  (a spectator where `sv_login` asks it to log in), the server takes `login ...` before the game
  sees it, and while connecting the player types `cmd login ...`. Its prompt lines are at most 38
  characters, the width its console shows.

On the first classic game a logged-in seat gets the `*auth` user info key (its login), which the
community's client already draws on its scoreboard as the login name with a marker; the classic
servers set it only for logins through their central web site.

**The secret travels in plain text**: the classic protocols encrypt nothing, so the line can be
read on the path and is seen by the server's operator. That is why a classic client never logs in
with the account's PASSWORD, nor with any secret it could use twice: it logs in with a ONE-TIME
CODE (`K7QF-29MD`), got from the hub's Account page, our client or the game CLI (`account code`),
good for one login within 15 minutes. A code read on the path is already spent (the maintainer's
choice, 2026-11-12: codes only, no reusable game password).

The community's client of the first classic game can log in without a secret on the wire: it
answers a challenge through its central web site with a token file. That site's address is fixed in
the client, so it cannot point at a hub of ours; it is not used.

The prompt says this in the game's own words, for example `This server keeps stats for
registered players. Log in with: login <name> <code>` and `Get a code at <hub address>`. Wrong
secrets, a held account, a name in use elsewhere on the server, and success each get a line a
player reads like a game's (`docs/architecture.md` § Player text). The line is kept out of chat,
logs, demos and broadcasts: the server takes it before anything prints or records it, and the
player's own console does not echo a line it sent as chat. A login line still counts toward the
chat flood guard.

### On the server

`engine/server/login.hpp` is the seam every game's server logs players in through, whatever its
protocol: the game hands it its seats and the lines its players type, and it answers with what to do
(a line to the player or to everyone, the seat's name, its login key, a drop). A login line is the
login's before the lobby, the rules or a recording see it; a `login` sent as a command counts toward
the chat flood rule as chat does. The dedicated process (`engine/server/main.cpp`) makes the
accounts the logins ask: with `sv_login` on and no `sv_accountmaster`, the server's own service over
`accounts.log` and `account.key` in its game folder, on `sv_accountport` (27901), its key printed at
the start; with a master, a socket of its own to ask it on. With `sv_accountkey` unset, the key the
master answers with first is believed and printed for the owner to set. A server with `sv_login 0`
and no master keeps no logins. Shooter's Edge wires it in `games/shooters_edge/net/classic_login.hpp`.
A player under a registered name who has not logged in yet plays under the game's default name and
is told to log in to use it.

### A server is its own master

A server whose `sv_accountmaster` is empty is its OWN master (the maintainer, 2026-11-12): it keeps
the store itself (`accounts.log` in its game folder, the same store and file a master keeps), signs
its own tickets with a key of its own (made at the first start and kept beside the store, never in
the tree), and answers the account service on its own port: sign-up, sign-in, tickets and codes,
over the same sealed channel. Our client and the game CLI reach it by the server's address, as
they would a master, and a hub pointed at it shows its stats; the console's `account` commands
manage it. Its accounts are its own: a player who also wants them elsewhere signs up with a
community's master, and a server joins that master by setting `sv_accountmaster`.

### The service on the wire

`engine/account/service.hpp` holds the bytes; this is what they promise.

- **One published key.** A master (or a server that is its own) has one Ed25519 key pair, grown
  from the seed in `account.key` beside `accounts.log`. Its public key is `sv_accountkey`: it checks
  the master's tickets, and the same key converted to X25519 is what every ask is sealed to. A
  client that has no key yet asks for it (an unsealed `key` datagram) and keeps the first answer;
  a program with `sv_accountkey` set refuses any other.
- **Every ask sealed.** An ask draws a new exchange key pair, seals its body (XChaCha20-Poly1305
  under BLAKE2b of the exchange and both public keys) and is answered under the same key, so a
  name, password, code or ticket is never on the wire in the clear and only the asker reads the
  answer. The ask is padded to 512 bytes and no answer is bigger, so the service cannot be used
  to flood a third party.
- **Sent again, answered the same.** An ask is sent again every half second until it is answered
  (six tries). The service keeps its last 1024 answers by the ask's exchange key and answers an ask
  it has seen with the answer it gave: an account is never made twice, nor a code spent twice, by
  one ask; a replayed ask's answer opens only for its asker. The `accounts` lab runs 40 players
  through sign-up, sign-in, a code and a server's login at 0, 10 and 30% loss.
- **The ops.** `create` (name, password: the recovery code), `sign_in` (a ticket), `code` (the
  password: a login code), `login` (asked by a game server for a classic client: the name, the
  code and the player's address; a ticket for the seat) and `recover` (the recovery code and a new
  password). The throttle's address for `login` is the server's and the player's together, so one
  player's wrong codes do not hold the server's other players.
- **Hosted.** `account::Host` runs the service on a UDP socket (port 27901 by default) over the
  store and key in a folder: the hub's `--accounts=<dir>` (beside `--serve-master`), and a server
  with no `sv_accountmaster` (M59c). The slow hash runs on the service's own loop: a sign-in costs
  one Argon2id hash (about 100 ms at 64 MiB), held back by the throttle.

## Names

`sv_reservednames 1` (the default when `sv_login` is on) reserves every account's name on the
server: a guest whose folded name is an account's plays under `sv_defaultname` (with a number when it
is taken) and is told the name is registered and to log in to use it; a logged-in player is always shown by their account's name when
`sv_loginname` is 1. A player may not take a name while another seat holds it logged in.

A server whose master is elsewhere does not hold the accounts, so it asks the master (`name`: the
name folded by the game's rules, answered with the account that reserves it). The name is let go
while the master answers, a fraction of a second; if an account holds it, the guest is renamed
then and told why (a player still connecting is told on joining). The answer is kept while the
server runs (up to 256 names), so a name is asked once, and a name the master did not answer is
asked again the next time it is used.

## On the window's own game

A game's own window serving others (a match on `sv_port`, or a coop campaign opened to others)
keeps logins as a dedicated server does: `sv_login`, `sv_logintime`, `sv_loginwelcome`,
`sv_accountmaster` and `sv_accountkey` in the window's console, taking effect with the next map.
With no master the window is its own: its accounts are kept in the game's own folder
(`<prefs>/accounts`) and served on `sv_accountport`. The window's player is never asked to log in.
The engine half is `server/accounts.hpp`, shared with the dedicated process.

## The settings

This row owns the classic login settings the server-settings work (M58) held out. Ours, with the
classic names as synonyms (`docs/servers.md` § A game's own names):

| Ours | Classic | What |
| --- | --- | --- |
| `sv_login` | `sv_login` | 0: no logins asked (a ticket still logs a player in); 1: players must log in, spectators may watch as guests; 2: everyone must |
| `sv_logintime` | | seconds a seat that must log in has before it is dropped (60) |
| `sv_loginname` | `sv_forcenick` | 1: a logged-in player's name is the account's, and a rename is refused |
| `sv_loginwelcome` | `sv_registrationinfo` | the line shown before the prompt (where to get a code) |
| `sv_accountmaster` | covers `sv_login_web` | the account master's address; empty (the default): the server is its own master (§ A server is its own master) |
| `sv_accountkey` | | the master's public key tickets are checked with; with no master, the server's own |
| `sv_reservednames` | | 1: guests may not use an account's name |
| `sv_accountport` | | the port a server that is its own master answers the account service on (27901) |
| `sv_stats` | | 1: send each finished match's report to the master |
| covered | `sv_hashpasswords` | secrets are always Argon2id; nothing is stored in the clear |
| covered | `sv_www_address`, `sv_www_authkey` | the master the server beats to and the server's key |
| `account create/remove/list/block/unblock` | `acc_create`, `acc_remove`, `acc_list`, `acc_block`, `acc_unblock` | the console's commands on the server's own store, when it is its own master |

## The match report

What a finished match leaves, generic in `engine/net/report.hpp`, and the same record as the replay
file's closing summary, so a demo's stats and the hub's are one thing, joined by the demo's SHA-256.
The game fills its part when a match ends (`ServerGame::match_report`, asked after every frame and
true once per match) and `engine/server` adds its own (the id, the game and its version, the
server's name and address, the start in Unix seconds); the server keeps its last 16 on
`Server::reports`, newest last. The bytes refuse a report past their limits (64 players, 16 teams,
64 columns) rather than cut it, since a report that says less than the match is worse than none.

Shooter's Edge tallies a deathmatch off the rules' world every frame
(`games/shooters_edge/net/classic_report.hpp`), since the rules keep only the frags: a player whose
health falls to nothing died; one whose frags fall on that frame killed themself; one whose frags
fall while they live killed a team mate. A player who leaves stays in the report with the seconds
they played. With a lobby only its match counts (the warmup is not a match), and a restart starts the
tally again. The match ends where its limit was reached (the time limit's moment, or the frag that
reached the frag limit), not when the map changes after the intermission; then the end is a limit,
a forfeit when fewer than two sides are left, or broken off. The `match-report` lab plays a
free-for-all and a team match of bots and draws each report as the hub's scoreboard.

| Part | Fields |
| --- | --- |
| The match | id (the server's key and a counter), game id and version, server name and address, mode, map, start (Unix seconds), duration, how it ended (a limit, a forfeit, broken off), the demo's SHA-256 if recorded |
| A team | name, score, colour |
| A player | name, account id (0: a guest), team, score, frags, deaths, suicides, team kills, seconds played, bot |
| The game's own | named integer columns per player (Shooter's Edge: each weapon's hits, shots and kills, damage given and taken, armour and health taken, a power-up's seconds); the game labels them, their units and their sort in its `HubSpec`, as it does the score's column |

The replay file's trailer meta already carries the first of it (`duration_ms`, a `score` per player,
a `team_score` per team; `docs/hub.md` § The archive), and the hub's archive files demos by those
keys. The report is the structured form of that summary: a recorder writes the report into the
trailer and keeps those keys readable, so an archive of older demos still reads (or the archive's
reader moves to the report in the same pull request). The hub's new answers live in their own files
beside `engine/hub/api.cpp`.

Each player also carries the lowest, average and highest ping the server saw over the match.

**Sending it** (`sv_stats 1`, the default; `engine/server/accounts.hpp`). A server that is its own
master takes its reports in process and counts them as its own. A server with `sv_accountmaster`
signs each report with a key of its own (`server.key` in its game folder, made at the first start;
the start prints its public half for the master's owner) and sends it on a socket of its own,
again every 2 s until the master's signed answer comes (a minute at most). It needs the master's
key (`sv_accountkey`) to check that answer, and without it sends nothing and says so at the start.
The datagrams are the account service's (`account/service.hpp`): the report is not sealed, since it
is no secret, and the answer is smaller than the report, so the service amplifies nothing. A report
too big for one datagram (a full server's weapon stats) goes in up to 16 pieces the master joins
before it checks the signature; it keeps at most 32 reports' pieces at once, each for a minute.

### The weapon stats

Shooter's Edge reads each player's weapon stats off the QC server every frame
(`games/shooters_edge/net/classic_stats.hpp`, changing nothing there) and sends them as the
report's columns. They are what a classic match's statistics keep (the facts in
`porting/shooters-edge/match-stats.md`), plus mid-airs (the maintainer, 2026-11-13):

| Columns | What |
| --- | --- |
| `<w>_attacks`, `<w>_hits`, `<w>_kills`, `<w>_deaths` for axe, sg, ssg, ng, sng, gl, rl, lg | attacks (pellets, nails, projectiles, pulses, swings), hits (a direct touch on a player, team mates too), kills by the final blow's weapon, deaths to it |
| `rl_splash`, `gl_splash`, `rl_damage` | enemies a rocket's or grenade's blast hurt, one each (the shown rl and gl percentages), and the rockets' damage summed (its average per splash hit) |
| `midair`, `midair_height`, `midair_height_max` | rocket blows on an enemy off the ground and out of the water, their heights over the floor below |
| `dmg_given`, `dmg_taken`, `dmg_team`, `dmg_self`, `dmg_ewep` | health and armour taken, to and from enemies, to team mates, to oneself, and given to an enemy carrying the rocket launcher or the arc |
| `ga`, `ya`, `ra`, `mh`, `quad`, `pent`, `ring`, `quad_s` | armour, mega healths and power-ups taken; the seconds the quad was held |
| `spawn_frags`, `streak_max`, `quad_run_max` | kills within 2 s of the victim's spawn, the longest run of kills between deaths, the most kills of one quad |

When the match ends every player reads them in the console: their frags and efficiency, each
weapon's accuracy, the rockets' direct hits, average damage and mid-airs, armour and power-ups,
damage, the streaks and spawn frags. The `match-report` lab draws them as a match page.

**At the master** (`account/matches.hpp`). A report is taken once by its server's key and id, so
one sent again counts once. **Which reports count**: only those from servers whose key the master's
owner has listed (the hub's `--trusted-servers=<file>`, one key a line, and the service's own key);
any other server's matches are kept and shown but add to no totals, since a server can say anything
about its own matches. A match with bots is kept and counted for its logged-in players; a bot and a
guest add to no totals. The reports are kept in `reports.log` beside the store and read back at
start, so the totals come back by the trust the master has then; the last 256 are kept for the hub.

**Totals** are kept per account, game, mode, map and ping band (the player's average ping in the
match: under 40, 80, 120 or 200 ms, or more), and summed under a filter (`TotalsFilter`): every
game or one, every map or one, chosen modes or every mode but some (all but ffa), and only matches
played under a ping. So a player reads their stats whole, on one map, in one mode, or at a good
ping, from the same records (the maintainer, 2026-11-13). The game's own columns are summed by name
beside the common ones, but for one whose name ends `_max` (the longest streak, the highest mid-air),
which keeps its largest.

The classic servers already on the internet report nothing to us; the hub's classic adapter reads
their recent-stats answer into the same record, best effort and without accounts.

## The hub's pages

| Page | What |
| --- | --- |
| Player | An account: totals per game and mode, the game's columns, the recent matches with their demos |
| Match | One report: the scoreboard, each player's columns, the demo |
| Leaders | Per game, mode and column, over a period |
| Account | `/account`, on a hub run with `--accounts` (`engine/hub/account_page.hpp`): sign up (the recovery code shown once), a one-time login code with the line a classic client types, a new password by the recovery code. Plain HTML forms posted to `/account/...`, no script, no cookie, every answer `no-store`. A password is taken only over HTTPS (the operator's proxy says so with `X-Forwarded-Proto: https`) or from the hub's own machine; otherwise the forms are shown disabled and a post is refused. The throttle counts by the proxy's `X-Forwarded-For` |

The JSON API: `/v1/players/{name}`, `/v1/matches?game=&mode=&player=&page=`, `/v1/matches/{id}`,
`/v1/leaders?game=&mode=&stat=&days=`. The hub's line that it has no accounts and no login
(`docs/hub.md`) holds for every page but Account.

## The plan

| Row | What | Labs |
| --- | --- | --- |
| M59a | `engine/account`: the store, names and folding per game, Argon2id, the throttle, codes, the log and its snapshot | `accounts` |
| M59b | The account service in the master's process: sign up, sign in, tickets, the server's login ask, over the sealed channel (built) | `accounts` |
| M59b2 | The game CLI's `account` group (built) | `accounts` |
| M59b3 | The hub's Account page (built) | `hub-pages` |
| M59c | The login seam in `engine/server` (`server/login.hpp`), the settings above, a dedicated server that is its own master; the first classic game's prompt and `login` line (built) | `se-login` |
| M59c2 | Our client logs itself in by its ticket; reserved names on a server whose master is elsewhere (a name ask); logins on a listen server and in a coop campaign | `se-login` |
| M59d | The sequel's prompt and `login` line, one account logged in on both games' servers | `se-login` |
| M59e | The match report, the replay file's closing summary, the master's totals, trusted servers | `match-report` |
| M59f | The hub's Player, Match and Leaders pages and their API; the classic adapter's recent stats | `hub-players` |

Friends, presence and invites stay M17j's, on these accounts. A store's sign-in (Steam) linked to
an account comes with M20c.

## Decisions

| Decision | Taken | Considered and not taken |
| --- | --- | --- |
| Where accounts live | With the master, which every server and hub already talks to | A separate account service (M17j's first plan): one more process to run; each server's own accounts file only: no account across servers |
| A server with no master | Its own master: the same store and service in the server's process (the maintainer, 2026-11-12) | No accounts without a master: a server alone could not keep names or stats |
| One account or one per game | One, with the game in every stat's key | One per game: the person asked for one across games |
| How a server trusts our player | A ticket signed by the master, checked offline | Asking the master at every connect: a server could not log anyone in while the master is down |
| A classic client's secret | One-time codes only (the maintainer, 2026-11-12) | The password typed in the console: the classic link is plain text, and a server's operator would see it. A reusable game password for a config's `login` line: read once on the path, it logs anyone in until changed. The first game's client's challenge through a token file: its web site's address is fixed in the client |
| Failed logins | A hold that doubles to 15 minutes | Blocked for good after 10, as the classic servers do: anyone could lock anyone out |
| Whose stats count | Servers the master's owner trusts | Every server: anyone could post a match that never happened |
| The report's format | One generic record, the game's columns named in its `HubSpec`, the same as the demo's closing summary | The classic mod's stats file: one game's shape |
| Names | 2 to 15 characters every game can show, reserved by a folded key per game | Each game's own rule: a name valid in one could not be shown in another |

## Labs

| Lab | Row | What it shows |
| --- | --- | --- |
| `accounts` | M59a, M59b | Sign-ups, a taken and a look-alike name refused, logins, a wrong password held, codes spent, tickets checked, the store read back after a restart; on the simulated network |
| `se-login` | M59c, M59d | A first-game and a sequel server: our client by ticket, a stock client's prompt and `login` line, a guest renamed off a registered name, the timeout; the console as the player sees it |
| `match-report` | M59e | Bots' matches on both games reported to a master, the totals, an untrusted server's report kept out of them |
| `hub-players` | M59f | The Player, Match and Leaders pages drawn in Chromium |
