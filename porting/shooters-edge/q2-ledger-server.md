# Quake II ledger: the server half, against upstream Q2PRO (2026-11-21)

A clean-room reader's checklist, in the reader's own words with no code. **The reference is upstream
Q2PRO (github.com/q2pro/q2pro) at commit 601a8df**: its `src/server`, `src/common` and the protocol
and game-interface headers the server is built against. This replaces the earlier server ledger,
which was read against the Action Quake fork of Q2PRO (the wrong reference): every row of that
ledger was checked again here against upstream (names, defaults, exact strings), rows that exist
only in the fork were dropped, and upstream things that ledger lacked were added.

**Against the fork ledger: 31 rows dropped as fork-only, 23 rows added as upstream-only (or
missed there).**

- Dropped (fork-only, 31): the `listsounds` command and its console lines; the console words `mul`, `div`, `setenv`
  (upstream registers no `setenv`); the settings `sv_status_ext`, `g_view_predict`, `g_view_low`,
  `g_view_high`, `sv_load_ent`, `steamid`, `steamcloudappenabled`, `steamclouduserenabled`; the
  messages 29 to 32 (game HUD updates, extended, user statistic, setting sync) and protocol 38;
  the client message 50 (setting sync answer), the `cvarsync` client command, the `statusx` and
  `rulesx` queries; the bots API, the `Client_GetVersion`, `Client_GetProtocol`, game-HUD and
  `CvarSync_Set` extensions and the fork's entity-for-client and setting-sync hooks; the
  `cvarsync:` player lines; the `==== Extended Protocol ====` console block and the fake-client
  console lines.
- Added (23): the `game` folder-name refusal; the `nextserver` setting; the dedicated server's
  forced deathmatch; the level-start autosave into `save0`; `No server running.`; the kick line a
  cvar ban or filtered command gives; the `client_connect` answer's keys; the map change's stuffed `changing` and
  `reconnect`; `download http`; the second game interface version (3302); the ten game feature
  bits read from `g_features`; the shutdown lines players see; upstream's extension console lines;
  the entity-file and override console lines.
- Changed against the fork ledger (kept rows): `sv_downloadserver` defaults to empty; the
  challenge offers `p=34,35,36` and `info` answers versions 34 to 36 only; the setting message
  carries two kinds (player updates, frame rate), not four; the game interface accepts version 3 or
  3302; the edict-size and edict-count errors carry no number; an index overflow reads
  `<call>(<name>): overflow`; `delcvarban` says `Removed <n> cvarban.` or `cvarbans.`; `status`
  has no bots table; the save folders are `.current`, `save0` and `save1`; `autosave` is the
  save into `save1`.

Every line is one thing a player, a server admin or a game library can see, marked against our
tree: `games/shooters_edge/net/q2_*` (the protocol endpoint), `games/shooters_edge/server/q2_*`
(our rules and the hosted game library), `engine/server` (every game's server core),
`engine/console`. Notes cross-referenced: `porting/shooters-edge/q2-gaps2-server.md` (gaps S1 to
S41; it was read against the fork, so S32's extended queries do not apply upstream),
`q2-server-gaps.md` (the Done table and the names agreed), `q2-modern-server-inventory.md`.
Names in the first column are the reference's; ours is in the last column when it differs.

Marks: `done` ours does it as a player or admin would see it (our own words for the same meaning
count); `missing` not in our tree or different where it matters (ranked in its note); `not-taken` a note or doc of ours decided not to take it
(named). Rows marked "debug build only" exist only in the reference's debug builds.

**Regraded 2026-11-23** (the owner's decision): the `partial` grade is gone, as in the Quake ledger. Each partial row became `done` (it works: its setting landed since, it was checked by name, or it says the same thing in our own words, which the player-text rule makes the intended ones), `not-taken` (our own design on purpose, noted "Our design:"; or the tail the owner chose not to build: cosmetic details, mission-pack and later effects, debug, developer and admin extras, hosted game libraries' edge cases, noted "Tail (decided 2026-11-23)"), or `missing` with a rank (rank 1 gameplay and daily use; rank 2 settings' Quake II names and the base game's effects seen on a joined server or in a demo). A missing row whose note says LIFT has a counterpart in the Quake code already: build it there once for both games (the settings lift). The missing count rose once by the regrade, because hidden gaps became rows; from here it only goes down.

## Totals

| kind | done | missing | not-taken | all |
| --- | --- | --- | --- | --- |
| Server console commands (the operator's words) | 59 | 0 | 17 | 76 |
| General console commands and macros the server's console has (common) | 50 | 0 | 7 | 57 |
| Server settings (cvars) with their defaults | 113 | 0 | 57 | 170 |
| Server-to-client messages (svc) and protocol extensions the server writes | 47 | 0 | 6 | 53 |
| What the server accepts from clients | 45 | 0 | 12 | 57 |
| The game library interface | 66 | 0 | 22 | 88 |
| Messages a player sees (refusals, drops, prints), quoted exactly | 63 | 0 | 9 | 72 |
| Messages an admin sees on the server console, quoted exactly | 30 | 0 | 6 | 36 |
| Server features beyond single words | 45 | 0 | 11 | 56 |
| **all** | 518 | 0 | 147 | 665 |

## 1. Server console commands (the operator's words)

done 59, missing 0, not-taken 17

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| cmd.heartbeat | `heartbeat` | Sends a heartbeat to every master at the next frame. | done | `plugins/classic/protocol/masters.cpp` |
| cmd.setmaster | `setmaster <addr>...` | Up to 8 masters (port 27900 by default), sets `public` 1, forces a heartbeat; names re-resolved at map changes. Refused off a dedicated server: `Only dedicated servers use masters.` | done | `plugins/classic/protocol/masters.cpp` |
| cmd.listmasters | `listmasters` | Table `num hostname lastmsg address`; `There are no masters.` | done | `listmasters` in games/shooters_edge/net/q2_game.cpp (`masters_list_text`, plugins/classic/protocol/masters.hpp) |
| cmd.kick | `kick <id>` | Drops a client by slot or exact name (part of a name with `sv_enhanced_setplayer`); everyone in the game and the client read `<name> was kicked`. | done | games/shooters_edge/net/q2_admin.cpp: slot, exact name, partial with `sv_partialnames` (`pick_player`, engine/server/operator.hpp); `<name> was kicked` to all |
| cmd.kickban | `kickban <id>` | Kick plus a ban of the address (a /32 for IPv4, a /64 for IPv6, no comment). | done | games/shooters_edge/net/q2_admin.cpp: kick, then the address on the endpoint's list until the server stops (IPv4) |
| cmd.status | `status` | `Current map: <map>`, then the table `num score ping name lastmsg address rate pr fps` (ping or ZMBI, ASGN, DNLD, HTTP, CNCT, PRIM), or `No UDP clients.`; then the recorder's TCP viewers. | done | games/shooters_edge/net/q2_admin.cpp (`q2_status_table`): the modern columns and ping words (ZMBI, ASGN, DNLD, HTTP, CNCT) |
| cmd.status-letters | `status d\|l\|p\|s\|t\|v` | Other tables: downloads, lag (loss both ways, RTT, dup, scale), protocols (major, minor, msglen, zlib, chan), settings (option letters G B R I F P L, upd, fps), idle and connect time, versions; anything else prints `Usage: status [d\|l\|p\|s\|t\|v]` then the normal table. | done | games/shooters_edge/net/q2_admin.cpp (`q2_status_table`): every letter; `s` shows no options (a stock client sends none) |
| cmd.serverinfo | `serverinfo` | `Server info settings:` then the server-info keys. | not-taken | Our design: the engine console's `serverinfo` lists our server-info settings under our own names; the status answer carries the game's keys (games/shooters_edge/net/q2_admin.cpp, `q2_status_keys`) |
| cmd.dumpuser | `dumpuser <id>` | `userinfo` block, then `miscinfo`: version, protocol (maj/min), maxmsglen, zlib, netchan type, ping, movement fps, update rate, RTT, loss both ways, packetdup, timescale, connection time. | done | games/shooters_edge/net/q2_admin.cpp: userinfo and the misc block (`q2_misc_lines`) |
| cmd.stuff | `stuff <id> <text>` | Makes one client run text. | done | games/shooters_edge/net/q2_admin.cpp (`line_rest`, engine/server/operator.hpp) |
| cmd.stuffall | `stuffall <text>` | Makes every client run text. | done | games/shooters_edge/net/q2_admin.cpp |
| cmd.stuffcvar | `stuffcvar <id> <var>...` | Asks a client for settings; answers print `<name>[<addr>]: "<var>" is "<value>"`. | done | games/shooters_edge/net/q2_admin.cpp, games/shooters_edge/net/q2_net_server.cpp (`q2_net_ask_setting`) |
| cmd.printall | `printall <text>` | Prints text to every client. | done | games/shooters_edge/net/q2_admin.cpp |
| cmd.map | `map <name> [force]` | New game on a map; mid-game refused with `sv_allow_map` 0 unless forced or settings are latched (2 makes it `gamemap`); prefixes `!` (stop the server if the map is missing), `*` end of unit, `$spot`, `+next` chained level. | not-taken | Our design: `map` with players in the game acts as `gamemap` (S13), the leading `!` read (`server/q2_server.cpp`); the refusal is not taken (`q2-server-gaps.md` row 15). |
| cmd.gamemap | `gamemap <name>` | Level change keeping the game (and the unit's saves); with `sv_recycle` set, a full restart (2: quit and tell clients to reconnect). | done | `games/shooters_edge/server/q2_server.cpp`, `q2_native.cpp` (`changelevel` too); the recycle path is not taken (row 15) |
| cmd.demomap | `demomap <file>` | Loads a demo or cinematic as the server's level; on a client build it plays the demo instead. | not-taken | `q2-gaps2-server.md` B1 |
| cmd.dumpents | `dumpents <file>` | Writes the map's entity text to `entdumps/<file>.ent`; `No map loaded.`, `Dumped entity string to <path>`. | done | games/shooters_edge/net/q2_game.cpp; held by `tests/shooters_edge_q2_wire_census_test.cpp` |
| cmd.killserver | `killserver` | Stops the server; clients read `Server was killed.` and are disconnected. | done | games/shooters_edge/net/q2_admin.cpp, games/shooters_edge/net/q2_game.cpp: `No server running.` when killed already; the masters sent `shutdown`; then only rcon answered, no frames or heartbeats until the next `map` |
| cmd.sv | `sv <command>` | Passes the words to the game library's server command; `No game loaded.` | done | Hosted library: games/shooters_edge/server/q2_native.cpp; our rules: `sv test` and the filter words in the game's own forms (games/shooters_edge/net/q2_admin.cpp, `sv_filter`) |
| cmd.pickclient | `pickclient <addr:port>` | Sends `passive_connect` to a waiting client; `Single player server running.`, `Bad client address: <addr>`, `Please specify client port explicitly.` | done | games/shooters_edge/net/q2_admin.cpp; held by `tests/shooters_edge_q2_server_sweep_test.cpp` |
| cmd.addban | `addban <addr[/mask]> [comment]` | Adds a ban entry. | not-taken | Our design: the engine list's `addip` (`engine/server/bans.hpp`, `games/shooters_edge/net/q2_admin.cpp`); the word `addban`, masks as `/bits` and the comment: verify |
| cmd.delban | `delban <addr\|id\|all>` | Removes ban entries. | not-taken | Our design: `removeip`; `delban` not read; by index or `all`: verify |
| cmd.listbans | `listbans` | Lists bans with hits and last hit. | not-taken | Our design: `listip`; no hits / last hit columns; `listbans` not read |
| cmd.addblackhole | `addblackhole` | An address whose every packet is ignored. | done | The engine's blackholes (`blackhole_addip`), the modern word read (`games/shooters_edge/net/q2_admin.cpp`) |
| cmd.delblackhole | `delblackhole` | Removes a blackhole. | done | `blackhole_removeip`, modern word read |
| cmd.listblackholes | `listblackholes` | Lists blackholes. | not-taken | Our design: `blackhole_listip`, modern word read; listing columns (hits, last hit) not compared |
| cmd.addstuffcmd | `addstuffcmd connect\|begin <text>` | Text every client runs on connect or on entering. | done | engine/server/operator.hpp (`stuff_list_command`), run in games/shooters_edge/net/q2_net_server.cpp |
| cmd.delstuffcmd | `delstuffcmd <list> <id\|cmd\|all>` | Removes one. | done | engine/server/operator.hpp |
| cmd.liststuffcmds | `liststuffcmds <list>` | Lists them. | done | engine/server/operator.hpp |
| cmd.addlrconcmd | `addlrconcmd <prefix>` | A command prefix allowed through limited rcon. | done | `engine/server/lrcon.hpp` (`lrcon_add`; the modern word read) |
| cmd.dellrconcmd | `dellrconcmd` | Removes one. | done | `lrcon_del` |
| cmd.listlrconcmds | `listlrconcmds` | Lists them. | done | `lrcon_list` |
| cmd.addfiltercmd | `addfiltercmd <cmd> [ignore\|log\|print\|stuff\|kick] [comment]` | A client command the server refuses, with an action. | done | `engine/server/cmdfilter.hpp` (`filtercmd_add`; modern word read) |
| cmd.delfiltercmd | `delfiltercmd` | Removes one. | done | `filtercmd_remove` |
| cmd.listfiltercmds | `listfiltercmds` | Lists them. | done | `filtercmd_list` |
| cmd.checkcvarbans | `checkcvarbans` | Asks clients for their settings again. | done | `games/shooters_edge/net/q2_admin.cpp` (`cvarban_check`) |
| cmd.addcvarban | `addcvarban <var> <match> [action] [comment]` | A client setting value refused. | done | `engine/server/settingban.hpp`, facts in `q2-cvarbans.md` |
| cmd.delcvarban | `delcvarban` | Removes them (`Removed <n> cvarban.` / `cvarbans.`). | done | same (wording in log.cvarbans) |
| cmd.listcvarbans | `listcvarbans` | Lists them. | done | same |
| cmd.adduserinfoban | `adduserinfoban` | A user info value refused. | done | same |
| cmd.deluserinfoban | `deluserinfoban` | Removes them. | done | same |
| cmd.listuserinfobans | `listuserinfobans` | Lists them. | done | same |
| cmd.mvdrecord | `mvdrecord [-z] <name>` | Multi-view recording of every player to `demos/<name>.mvd2` (gzip with `-z`); on a relay, records the channel. | done | games/shooters_edge/net/q2_mvd_server.cpp (`q2_mvd_command`): `-h`, `-z` (gzip), the refusals and lines; frames from games/shooters_edge/server/q2_mvd2_live.hpp; a relay channel's recording comes with the relay |
| cmd.mvdstop | `mvdstop` | Ends it. | done | games/shooters_edge/net/q2_mvd_server.cpp |
| cmd.say | `say <text>` | Dedicated server only: `console: <text>` to everyone at chat level. | done | `games/shooters_edge/server/q2_server.cpp` |
| cmd.mvdstuff | `mvdstuff <text>` | Runs text as the recorder's dummy client. | done | games/shooters_edge/net/q2_mvd_server.cpp: the text (outer quotes off) added to the spectator's commands; the refusal without one |
| cmd.addgtvhost | `addgtvhost` | A relay viewer address allowed without password. | done | plugins/classic/protocol/addr_list.hpp, mounted in games/shooters_edge/net/q2_mvd_server.cpp |
| cmd.delgtvhost | `delgtvhost` | | done | plugins/classic/protocol/addr_list.hpp |
| cmd.listgtvhosts | `listgtvhosts` | | done | plugins/classic/protocol/addr_list.hpp: id, address/mask, hits, last hit, comment |
| cmd.addgtvban | `addgtvban` | A relay viewer address refused. | done | plugins/classic/protocol/addr_list.hpp, mounted in games/shooters_edge/net/q2_mvd_server.cpp |
| cmd.delgtvban | `delgtvban` | | done | plugins/classic/protocol/addr_list.hpp |
| cmd.listgtvbans | `listgtvbans` | | done | plugins/classic/protocol/addr_list.hpp |
| cmd.save | `save <dir>` | Saves the game (`You must be in a game to save.`, `Game does not support enhanced savegames.`, `Can't savegame in a deathmatch.`, `Can't savegame while dead!`, `Bad savedir.`, `Game saved.`); the library may refuse through its own can-save call. | done | games/shooters_edge/net/q2_game.cpp (`save_words`): `Usage: save <directory>`, `Bad savedir.`, `Game does not support enhanced savegames.`, the rules' own refusals, `Game saved.`; kept as `saves/<dir>.save` in the game's folder as the window keeps it; tests/shooters_edge_q2_mapfix_test.cpp |
| cmd.load | `load <dir>` | Loads a save (`No such savegame: <name>`, `Bad savedir.`). | done | games/shooters_edge/net/q2_game.cpp (`save_words`): `No such savegame: <name>`, `Bad savedir.`; the save's map started with the game read back (a hosted library's or our rules') |
| cmd.autosave | `autosave` | A save into the fixed slot `save1`, with the same refusals as `save`. | done | games/shooters_edge/net/q2_game.cpp (`save_words`): into `save1`, with the save's refusals |
| cmd.mvdplay | `mvdplay [-l n] [-n name] [-r chan] <file>...` | Relay: plays recordings to relay viewers. | done | games/shooters_edge/net/q2_mvd_relay.cpp: files from demos/ (gzip too), `-l`, `-n`, `-r`, a 10 Hz pace, snapshots for the seek |
| cmd.mvdconnect | `mvdconnect [-n] [-u] [-p] <addr>` | Relay: connects to a server's recorder over TCP. | done | games/shooters_edge/net/q2_mvd_relay.cpp: `-n`, `-u`, `-p`, the hello (no deflate asked), drops and destroys in the server's words, the backoff |
| cmd.mvdisconnect | `mvdisconnect [-a]` | Relay: drops the upstream. | done | games/shooters_edge/net/q2_mvd_relay.cpp: one or `-a` |
| cmd.mvdkill | `mvdkill` | Relay: kills a channel. | done | games/shooters_edge/net/q2_mvd_relay.cpp |
| cmd.mvdspawn | `mvdspawn` | Relay: starts serving viewers. | done | games/shooters_edge/net/q2_mvd_relay.cpp and games/shooters_edge/net/q2_game.cpp: relay mode takes the port, the game's clients told to reconnect |
| cmd.mvdchannels | `mvdchannels` | Relay: lists channels. | done | games/shooters_edge/net/q2_mvd_relay.cpp: both tables (`r` the recordings) |
| cmd.mvdservers | `mvdservers` | Relay: lists upstreams. | done | games/shooters_edge/net/q2_mvd_relay.cpp: the ratio is always 100% (the relay asks no deflate) |
| cmd.mvdcontrol | `mvdcontrol` | Relay: a channel's settings. | done | games/shooters_edge/net/q2_mvd_relay.cpp: `-n` renames, `-l` checked only |
| cmd.mvdpause | `mvdpause` | Relay: pauses a played recording. | done | games/shooters_edge/net/q2_mvd_relay.cpp |
| cmd.mvdskip | `mvdskip` | Relay: skips in a played recording. | done | games/shooters_edge/net/q2_mvd_relay.cpp |
| cmd.mvdseek | `mvdseek` | Relay: seeks in a played recording. | done | games/shooters_edge/net/q2_mvd_relay.cpp: relative, absolute and percent; back through the reader forked every `mvd_snaps` s |
| cmd.svaclist | `svaclist` | Anticheat: lists clients' anticheat state. | not-taken | `q2-gaps2-server.md` B1; docs/goals.md M17l (no client scanning) |
| cmd.svacinfo | `svacinfo` | Anticheat: one client's details. | not-taken | same |
| cmd.svacupdate | `svacupdate` | Anticheat: reloads the lists. | not-taken | same |
| cmd.svacinvalidate | `svacinvalidate` | Anticheat: invalidates clients. | not-taken | same |
| cmd.addacexception | `addacexception` | Anticheat exemption by address. | not-taken | same |
| cmd.delacexception | `delacexception` | | not-taken | same |
| cmd.listacexceptions | `listacexceptions` | | not-taken | same |
| cmd.addacrequirement | `addacrequirement` | Anticheat required for an address. | not-taken | same |
| cmd.delacrequirement | `delacrequirement` | | not-taken | same |
| cmd.listacrequirements | `listacrequirements` | | not-taken | same |

## 2. General console commands and macros the server's console has (common)

Our dedicated server's console is the engine console (`engine/console`), fed by `--commands` and
`+commands`; rcon reaches only Quake II's admin words and the rules' console (S29).

done 50, missing 0, not-taken 7

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| con.cmdlist | `cmdlist` | Lists commands (with a filter). | done | `engine/console/console_builtins.cpp` |
| con.macrolist | `macrolist` | Lists the `$` macros. | done | engine/server/macros.hpp: the name in 16 columns, the value cut to 63, the filter a wildcard, `<n> of <m> macros`; listed by name, not newest first |
| con.exec | `exec <file>` | Runs a config file. | done | engine console |
| con.echo | `echo <text>` | Prints a line. | done | engine console |
| con._echo | `_echo [-e] [-n] <text>` | Echo with backslash escapes and without the newline. | done | engine/console/console_more.cpp (every console, the dedicated one too) |
| con.alias | `alias <name> <cmds>` | Defines an alias (also lists). | done | engine console |
| con.unalias | `unalias` | Removes an alias. | done | engine console |
| con.wait | `wait` | Defers the rest of the buffer a frame. | done | engine console |
| con.text | `text <command>` | Runs its argument as command text (used inside aliases). | done | engine/console/console_more.cpp |
| con.complete | `complete <command>` | Declares a name for completion. | done | engine/console/console_more.cpp |
| con.trigger | `trigger <command> <match>` | Runs a command when printed text matches. | done | engine/console/re_triggers.hpp (#1248), on every console |
| con.untrigger | `untrigger` | Removes a trigger. | done | engine/console/re_triggers.hpp (#1248) |
| con.if | `if <expr> <op> <expr> [then] <cmd> [else <cmd>]` | Conditional. | done | `engine/console/script.cpp` |
| con.set | `set <var> <value> [u\|s]` | Sets a setting; `u`/`s` add the user/server-info flag; `Usage: set <variable> <value> [u / s]`. | done | Checked 2026-11-23: `setu` and `sets` exist in the engine console (`engine/console/console_more.cpp`). |
| con.setu | `setu` | Set with the user-info flag. | done | engine/console/console_more.cpp |
| con.sets | `sets` | Set with the server-info flag (an admin's own status key). | done | engine/console/console_more.cpp: the key joins the server info; which keys a status answer shows is S27's |
| con.seta | `seta` | Set and archive. | done | engine console |
| con.cvarlist | `cvarlist [-a -c -h -l -m -n -r -s -t -u -v]` | Lists settings, filtered by flag, with flag letters; `<i> of <n> cvars`. | done | engine console (its own letters) |
| con.toggle | `toggle <var> [values]` | Flips or cycles values (`"<v>" is "<x>", can't toggle` / `can't cycle`). | done | `engine/console/script.cpp` |
| con.inc | `inc <var> [value]` | Adds to a number. | done | engine |
| con.dec | `dec <var> [value]` | Subtracts. | done | engine |
| con.reset | `reset <var>` | Back to the default. | done | engine |
| con.resetall | `resetall` | Every setting back to its default. | done | engine/console/console_more.cpp |
| con.show-var | typing a setting's name | Prints `"<name>" is "<value>"`, then `  default: "<d>"` and `  latched: "<l>"` when they differ. | done | Same meaning in our words: the engine prints the value. |
| con.z_stats | `z_stats` | Memory zone statistics. | not-taken | The original's memory zones by kind; the engine has none (the C++ allocator), and the profiler's allocation counts (engine/prof) answer how much a server holds |
| con.lasterror | `lasterror` | Prints the last error. | done | engine/console/console_more.cpp: the last error line, or an empty line |
| con.quit | `quit [reason]` | Stops the server; clients read `Server quit` or `Server quit: <reason>` (msg.shutdown). | done | engine/server/main.cpp (`quit [reason]`, the words joined by spaces) and games/shooters_edge/net/q2_game.cpp (`quit`): every player and relay viewer told at the high level and disconnected, the stream relays sent their disconnect, the masters `shutdown`; exit 0 |
| con.recycle | `recycle [reason]` | As quit, but clients read `Server restarted: <reason>` and are told to reconnect. | done | engine/server/main.cpp, games/shooters_edge/net/q2_game.cpp: as quit with `Server restarted` and the reconnect; the process exits 0 for its supervisor to start again |
| con.path | `path` | Shows the search path. | done | plugins/classic/content/files.hpp (`search_path_text` with the totals), games/shooters_edge/net/q2_game.cpp: the PAK and PKZ footers when not zero |
| con.fdir | `fdir` | Lists files by filter. | done | plugins/classic/content/fs_words.hpp: `*`, `?`, `\` ignoring case against the whole path, several by `;`, folders 8 deep; tests/classic_content_fs_words_test.cpp |
| con.dir | `dir` | Lists a directory. | done | plugins/classic/content/fs_words.hpp: the folder's files from every archive and folder, sorted ignoring case and each once, extensions as suffixes; tests/classic_content_fs_words_test.cpp |
| con.fs_stats | `fs_stats` | File system statistics (debug build only). | not-taken | docs/porting/server.md: debug build only, not taken |
| con.whereis | `whereis <file>` | Which pack or folder holds a file. | done | plugins/classic/content/fs_words.hpp: the first or every place with its size, the links' lines, the refusals, the 63-character pack limit, the lower-case retry; tests/classic_content_fs_words_test.cpp |
| con.link | `link` | Adds a file link. | done | plugins/classic/content/fs_words.hpp (files.hpp `FileLinks`): a prefix rewrite for reading, the list, the usage; tests/classic_content_fs_words_test.cpp |
| con.unlink | `unlink` | | done | plugins/classic/content/fs_words.hpp: `-a`, `-h`, the missing and unknown names |
| con.softlink | `softlink` | | done | plugins/classic/content/fs_words.hpp: tried only when the path itself is not found |
| con.softunlink | `softunlink` | | done | plugins/classic/content/fs_words.hpp |
| con.fs_restart | `fs_restart` | Re-reads the file system (and tells an extended library). | done | games/shooters_edge/net/q2_game.cpp: the folders opened again with the links kept, the banner and the listing; no hosted library has the extended restart call, so none is told |
| con.net_restart | `net_restart` | Re-opens the sockets. | done | engine/server/net_restart.hpp, engine/server/main.cpp: the socket reopened on net_ip and net_port (a latched value taken), the last valid port, `Couldn't open server UDP port.`; games/shooters_edge/net/q2_game.cpp reopens the multiview TCP listener; tests/server_net_restart_test.cpp |
| con.net_stats | `net_stats` | Socket counters and rates. | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: engine/server/main.cpp: sent, received and refused counts; no uptime, per-second averages, error split or current rates lines |
| con.showip | `showip` | The host's addresses. | done | engine/net/udp_socket.hpp (`host_addresses`, `showip_text`), engine/server/main.cpp, games/shooters_edge/net/q2_game.cpp (the stream listener's `Server TCP` line) |
| con.dns | `dns <name>` | Resolves a name. | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: engine/server/main.cpp: resolves; the answer reads "<name> resolved to <address>", not the Hostname and IP lines with reverse names |
| con.bsplist | `bsplist` | Loaded maps. | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: games/shooters_edge/net/q2_game.cpp: the held map, its size and `Total resident`; with a word its counts, clusters and checksum (texinfo, visibility and the features line not counted) |
| con.tests | `error`, `errordrop`, `freeze`, `crash`, `doublefree`, `printjunk`, `bsptest`, `wildtest`, `normtest`, `infotest`, `snprintftest`, `modeltest`, `imagetest`, `soundtest`, `activate`, `mdfourtest`, `mdfoursum`, `extcmptest`, `nextpathtest`, `extract` | Developer tests (debug build only). | not-taken | Developer tests in debug builds of the original (crashes, junk prints, format checks); the engine's own tests are its test suite |
| macro.com_date | `$com_date` | Today's date by `com_date_format`. | done | engine/console/clock_macros.hpp, on the dedicated console (engine/server/main.cpp) |
| macro.com_time | `$com_time` | The time by `com_time_format`. | done | engine/console/clock_macros.hpp, on the dedicated console |
| macro.com_uptime | `$com_uptime` | Uptime, short. | done | engine/console/clock_macros.hpp: `MM.SS`, `H:MM.SS`, `D+H:MM.SS`; on the dedicated console |
| macro.com_uptime_long | `$com_uptime_long` | Uptime, long. | done | engine/console/clock_macros.hpp, on the dedicated console |
| macro.random | `$random` | One digit 0 to 9. | done | engine/console/clock_macros.hpp, on the dedicated console |
| macro.com_maplist | `$com_maplist` | The maps found. | done | engine/server/macros.hpp, games/shooters_edge/net/q2_game.cpp (`maps`): sorted ignoring case, each once, by spaces |
| macro.net_uprate | `$net_uprate` | Outgoing rate. | done | engine/server/main.cpp: bytes a second sent over three-second windows (engine/net/rates.hpp) |
| macro.net_dnrate | `$net_dnrate` | Incoming rate. | done | engine/server/main.cpp: bytes a second received over three-second windows |
| con.refusals | `<name> is read-only.`, `<name> is cheat protected.`, `<name> may be set from command line only.`, `<name> will be changed for next game.`, `<name> is not a variable` | A setting's refusals. | done | Same meaning in our words (the player-text rule makes our words the intended ones). engine says `<name> will be "<v>" after a restart (it is latched)` and its own words for the rest (`engine/console/console_builtins.cpp`) |
| con.info-refusals | `Info cvars should not contain '\', ';' or '"' characters.`, `Info cvars should be less than 64 characters long.` | Server/user info values checked. | done | Same meaning in our words: the hosted library's `cvar` refuses unsafe info values (`info_safe`, `server/q2_native_calls.cpp`). |
| con.game-check | `'game' should contain characters [A-Za-z0-9_-] only.` | A bad game folder name is refused and the setting reset; the base folder's own name reads as empty. | done | plugins/classic/content/fs_words.hpp (`game_folder_check`), games/shooters_edge/net/q2_game.cpp: the base folder's name read as empty, a bad name refused with the line and the base folder played |
| con.startup-cfgs | `default.cfg`, `config.cfg` (`q2config.cfg` on Windows), `autoexec.cfg` (base folder, then game folder), `postexec.cfg`, `postinit.cfg`, then the alias `dedicated_start` | Startup order of configs. | not-taken | Our design: the server core's config layers (`engine/server/config.hpp`, docs/servers.md); the classic file names and `dedicated_start` are not taken. |
| con.rcon-before-map | rcon answered before any game | Only `rcon` is answered while no game runs. | done | `sesv` opens its socket with its first map at start |

## 3. Server settings (cvars) with their defaults

Defaults are upstream's; flags as registered. Our Quake II spellings that are synonyms are listed in
`games/shooters_edge/net/q2_game.cpp` (`kSettingNames`).

done 113, missing 0, not-taken 57

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| cvar.protocol | `protocol` = `34` (server info, read-only) | Rewritten each map: `36` when the library asks for protocol extensions, else `34`. | not-taken | `q2-server-gaps.md` "Not worth building" (informational keys); our status sends `protocol 34` |
| cvar.skill | `skill` = `1` (latched) | Difficulty. | done | Rules setting (`games/shooters_edge/server/q2_server.hpp`) |
| cvar.deathmatch | `deathmatch` = `1` (server info, latched) | Deathmatch mode; the modern default is 1; a dedicated server forces it on unless `coop` is set (feat.dedicated-dm). | done | Rules setting |
| cvar.coop | `coop` = `0` (latched; not server info) | Coop mode; with `deathmatch` also set it is turned off (`Deathmatch and Coop both set, disabling Coop`). | done | Rules setting |
| cvar.cheats | `cheats` = `0` (server info, latched) | Allows cheat commands; also gates `timescale`, `fixedtime`, `timedemo`. | done | Rules setting (the gated diagnostics are not taken) |
| cvar.dmflags | `dmflags` = `16` (server info) | Deathmatch rule bits; the default is 16 (instant powerups). | not-taken | Our design: ours defaults to 0 (`games/shooters_edge/net/q2_game.cpp` kRules), the stock library's default |
| cvar.fraglimit | `fraglimit` = `0` (server info) | Frags that end the level. | done | Rules setting |
| cvar.timelimit | `timelimit` = `0` (server info) | Minutes that end the level. | done | Rules setting |
| cvar.maxclients | `maxclients` = `8` (server info, latched) | Slots, clamped 1 to 256; deathmatch at 1 or less becomes 8, coop 4, neither 1. | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `sv_maxclients` (the stock name read); 1 or less becomes 8 in deathmatch (games/shooters_edge/net/q2_game.cpp, `players`); capped at 32 (S35); coop's 4 and single player's 1 are not applied, the slots being fixed when the server starts |
| cvar.sv_reserved_slots | `sv_reserved_slots` = `0` (latched) | Slots kept for the reserved password; clamped below `maxclients`. | done | Engine `sv_vipslots` (synonym; `engine/server/admission.hpp`) |
| cvar.hostname | `hostname` = `noname` (server info, archive) | Server name. | done | Engine `sv_hostname` (the stock name read) |
| cvar.timeout | `timeout` = `90` | Seconds of silence before a drop. | done | The endpoint's `timeout`, 90 (`games/shooters_edge/net/q2_net_server.hpp`) |
| cvar.zombietime | `zombietime` = `2` | Seconds a dropped slot stays a zombie. | done | `sv_zombietime` (synonym) |
| cvar.sv_ghostime | `sv_ghostime` = `6` | Seconds a challenged slot may stay silent; also the unreachable-notice drop delay. | not-taken | Our design: `sv_ghosttime` (synonym); the unreachable drop not taken (`q2-server-gaps.md` row 17) |
| cvar.sv_idlekick | `sv_idlekick` = `0` | Seconds of no activity before a drop with reason `idling`. | done | Quake II's own name for `sv_idletime` (`own`: the lobby's unready kick set aside, `engine/server/server.hpp` SettingName); `shooters_edge_q2_net_test` |
| cvar.sv_enforcetime | `sv_enforcetime` = `1` | Move-time budget, 1800 ms per 16 frames. | not-taken | Our design: `sv_speedcheck` (synonym), our rules only; a hosted library runs unchecked (S12) |
| cvar.sv_timescale_time | `sv_timescale_time` = `16` | Window of the time-skew check. | done | `sv_speedwindow` (synonym; `engine/server/skew.hpp`) |
| cvar.sv_timescale_warn | `sv_timescale_warn` = `0` | Logs a skew over the ratio. | done | `sv_speedwarn` (synonym) |
| cvar.sv_timescale_kick | `sv_timescale_kick` = `0` | Drops a client over the ratio (`time skew too high`). | not-taken | Our design: `sv_speedkick` (synonym); the drop's words differ (msg.drop.time-skew) |
| cvar.sv_allow_nodelta | `sv_allow_nodelta` = `1` | 0 drops a client asking for too many full frames. | done | `sv_nodelta` (synonym) |
| cvar.sv_fps | `sv_fps` = `10` (latched) | Server frame rate, used when the library sets the variable-fps bit. | done | Rules setting `sv_fps` (`games/shooters_edge/net/q2_game.cpp`); monsters, items and hands keep 10 Hz by design |
| cvar.sv_force_reconnect | `sv_force_reconnect` = `` (latched) | Clients must reconnect to this address and bring back random values first. | done | ours `sv_forcereconnect` (games/shooters_edge/net/q2_game.cpp), latched to the next map; held by `tests/shooters_edge_q2_server_sweep_test.cpp` |
| cvar.sv_show_name_changes | `sv_show_name_changes` = `0` | 1 announces renames of players in the game, 2 of everyone. | not-taken | Our design: `sv_shownames` (synonym); our line ends with a full stop (`engine/server/names.hpp`) |
| cvar.sv_airaccelerate | `sv_airaccelerate` = `0` (latched) | Deathmatch air acceleration written into the map's value. | done | Rules setting |
| cvar.sv_qwmod | `sv_qwmod` = `0` (latched) | The alternative move for extended clients. | not-taken | `q2-server-gaps.md` (names agreed 2026-11-19: the alternative move dropped, remix physics has it) |
| cvar.public | `public` = `0` (latched) | Heartbeats to masters. | done | Quake II's spelling of `sv_public` (`net/q2_game.cpp`): 1 makes the server public from the map's start, beating to the `setmaster` list (`classic/protocol/masters.cpp`); the original's built-in default master is long offline, so none is named |
| cvar.sv_password | `sv_password` = `` (private) | Server password (user info `password`); refusals `Please set your password before connecting.`, `Invalid password.` | not-taken | Our design: engine `sv_password` (`none` means none); refusal words are the stock library's `Password required or incorrect.` (msg.oob.password) |
| cvar.sv_reserved_password | `sv_reserved_password` = `` (private) | Reserved slots' password. | done | `sv_vippassword` (synonym) |
| cvar.sv_locked | `sv_locked` = `0` | Refuses new connections. | not-taken | Our design: engine `sv_locked`; refusal adds `Connection refused.` (msg.oob.locked) |
| cvar.sv_novis | `sv_novis` = `0` | Sends every entity unculled. | done | Rules setting (`games/shooters_edge/net/q2_game.cpp`) |
| cvar.sv_downloadserver | `sv_downloadserver` = `` (empty) | HTTP download address given at connect (`dlserver=`). | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_net_server.cpp: ` dlserver=<url>` in the connect answer, as given |
| cvar.sv_redirect_address | `sv_redirect_address` = `` | Full: send newcomers to this address (`!` first: everyone but reserved-password holders). | done | `sv_redirect` (synonym; `engine/server/admission.hpp`) |
| cvar.sv_debug | `sv_debug` = `0` | Debug prints (debug build only). | not-taken | docs/porting/server.md: debug build only, not taken |
| cvar.sv_pad_packets | `sv_pad_packets` = `0` | Pads packets with no-ops (debug build only). | not-taken | docs/porting/server.md: debug build only, not taken |
| cvar.sv_lan_force_rate | `sv_lan_force_rate` = `0` (latched) | LAN clients unlimited. | done | `sv_lanrate` (synonym) |
| cvar.sv_min_rate | `sv_min_rate` = `15000` (latched) | Lowest client rate. | done | `sv_minrate` (synonym; `engine/server/rate.hpp`) |
| cvar.sv_max_rate | `sv_max_rate` = `60000` (latched) | Highest client rate. | done | `sv_maxrate` (synonym) |
| cvar.sv_calcpings_method | `sv_calcpings_method` = `2` | Ping as none, average or minimum of 16 frames. | done | `sv_pingmethod` (synonym; `take_trip` in `games/shooters_edge/net/q2_net_server.cpp`) |
| cvar.sv_changemapcmd | `sv_changemapcmd` = `` | Command run after each map loads. | done | `sv_mapcmd` (synonym; engine) |
| cvar.sv_max_download_size | `sv_max_download_size` = `8388608` | Largest file sent. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_net_server.cpp: bytes, 0 or past 128 MiB and 4 KiB the cap; a larger file refused |
| cvar.sv_max_packet_entities | `sv_max_packet_entities` = `0` | Most entities a frame (0: 128 old clients, 512 extended). | not-taken | Our design: `sv_maxents` (synonym), our rules only; a hosted library's frames uncapped |
| cvar.sv_trunc_packet_entities | `sv_trunc_packet_entities` = `1` | Cuts an oversize frame short for old clients. | not-taken | `q2-server-gaps.md` row 18 (our frames stop at the packet's room) |
| cvar.sv_prioritize_entities | `sv_prioritize_entities` = `0` | Which entities first when over the cap. | not-taken | Our design: `sv_entpriority` (synonym), our rules only |
| cvar.sv_strafejump_hack | `sv_strafejump_hack` = `1` (latched) | Removes the post-landing jump hold. | done | `sv_landjump` (synonym) |
| cvar.sv_waterjump_hack | `sv_waterjump_hack` = `1` (latched) | Removes the fixed water-jump lift. | done | `sv_waterjump` (synonym) |
| cvar.sv_packetdup_hack | `sv_packetdup_hack` = `0` | Most duplicate packets a client may ask for. | done | ours `sv_packetdup` (games/shooters_edge/net/q2_game.cpp), the reference's name a synonym |
| cvar.sv_allow_map | `sv_allow_map` = `0` on a dedicated server, `1` otherwise | Refuses `map` mid-game; 2 makes it `gamemap`. | not-taken | `q2-server-gaps.md` row 15 |
| cvar.sv_cinematics | `sv_cinematics` = `1` | Serves cinematic levels. | done | games/shooters_edge/net/q2_game.cpp (`take_shows`): 0 skips a movie with something after it; `.dm2` names no level here |
| cvar.sv_recycle | `sv_recycle` = `0` | Next `gamemap` a full restart (1) or a quit-and-reconnect (2); reset at each new game. | not-taken | `q2-server-gaps.md` row 15 |
| cvar.sv_enhanced_setplayer | `sv_enhanced_setplayer` = `0` | Player words match part of a name. | done | ours `sv_partialnames` (games/shooters_edge/net/q2_game.cpp), the reference's name a synonym |
| cvar.sv_iplimit | `sv_iplimit` = `3` | Connections per address or IPv6 /48. | done | Engine `sv_iplimit` |
| cvar.sv_status_show | `sv_status_show` = `2` | What `status` answers (0 nothing, 1 info, 2 also players). | done | `sv_status` (synonym) |
| cvar.sv_status_limit | `sv_status_limit` = `15` | Status answers allowed. | done | `sv_statusrate` (synonym) |
| cvar.sv_uptime | `sv_uptime` = `0` | `uptime` key in status (1 short, 2 long). | done | Engine `sv_uptime` (`engine/server/status.hpp`) |
| cvar.sv_auth_limit | `sv_auth_limit` = `1` | Failed password tries allowed. | done | `sv_passwordrate` (synonym) |
| cvar.sv_rcon_limit | `sv_rcon_limit` = `1` | Failed rcon tries allowed. | done | `sv_rconrate` (synonym) |
| cvar.sv_namechange_limit | `sv_namechange_limit` = `5/min` | Renames allowed. | done | `sv_namelimit` (synonym) |
| cvar.sv_allow_unconnected_cmds | `sv_allow_unconnected_cmds` = `0` | 0 keeps commands of clients not in the game from the library. | not-taken | Our design: ours always behaves as 0 (`string_command` passes only active clients' lines); no setting |
| cvar.lrcon_password | `lrcon_password` = `` (private) | Limited rcon password. | done | Engine `lrcon_password` |
| cvar.sv_features | `sv_features` (read-only) | The engine's feature bits for the library. | done | games/shooters_edge/server/q2_native.cpp: read-only, proper in-use, all disconnects, enhanced saves, extra user info, IPv6, index overflow (29706) |
| cvar.g_features | `g_features` = `0` (read-only) | The library's feature bits (section 6, api.gmf.*); reset to 0 when the library unloads. | done | games/shooters_edge/server/q2_native.cpp: read-only `0`, set by the game's force-set, back to 0 as the game restarts |
| cvar.sv_mvd_enable | `sv_mvd_enable` = `0` (latched) | Multi-view recorder: 1 local, 2 also TCP viewers. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_server.cpp: 1 recordings, 2 the TCP stream on the game port |
| cvar.sv_mvd_maxclients | `sv_mvd_maxclients` = `8` (latched) | TCP viewers. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_server.cpp: the TCP slots |
| cvar.sv_mvd_bufsize | `sv_mvd_bufsize` = `2` (latched) | Viewer send buffer. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_server.cpp: 32 KiB units unsent before `overflowed` |
| cvar.sv_mvd_password | `sv_mvd_password` = `` (private) | Viewer password. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_server.cpp |
| cvar.sv_mvd_maxsize | `sv_mvd_maxsize` = `0` | Recording size limit. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_server.cpp; held by `tests/shooters_edge_q2_mvd_server_test.cpp` |
| cvar.sv_mvd_maxtime | `sv_mvd_maxtime` = `0` | Recording time limit (minutes). | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_server.cpp; held by `tests/shooters_edge_q2_mvd_server_test.cpp` |
| cvar.sv_mvd_maxmaps | `sv_mvd_maxmaps` = `1` | Map changes before a recording stops. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_server.cpp |
| cvar.sv_mvd_noblend | `sv_mvd_noblend` = `0` (latched) | Leaves screen blends out. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/server/q2_mvd2_live.cpp |
| cvar.sv_mvd_nogun | `sv_mvd_nogun` = `0` (latched) | Leaves the held weapon out. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/server/q2_mvd2_live.cpp |
| cvar.sv_mvd_nomsgs | `sv_mvd_nomsgs` = `1` (latched) | Leaves text messages out. | done | games/shooters_edge/server/q2_mvd2_live.cpp: with a spectator, a print to anyone else left out and the stream's flag set; tests/shooters_edge_q2_mvd_server_test.cpp |
| cvar.sv_mvd_begincmd | `sv_mvd_begincmd` = `wait 50; putaway; wait 10; help;` | The dummy's commands on entering. | done | games/shooters_edge/net/q2_mvd_dummy.cpp: added to the spectator's commands at each seating |
| cvar.sv_mvd_scorecmd | `sv_mvd_scorecmd` = `putaway; wait 10; help;` | Run when the scoreboard has been gone a while. | done | games/shooters_edge/net/q2_mvd_dummy.cpp: added when its scoreboard (its layouts stat, or a new layout) has not been seen for 9 s while the stream plays |
| cvar.sv_mvd_autorecord | `sv_mvd_autorecord` = `0` (latched) | Local recordings started by itself. | done | games/shooters_edge/net/q2_mvd_dummy.cpp: the spectator made at every level (the pause off), its `record <name>` and `stop` taken with their lines |
| cvar.sv_mvd_capture_flags | `sv_mvd_capture_flags` = `5` | Which players are recorded. | done | games/shooters_edge/server/q2_mvd2_live.cpp (`captured`): 1 no bots, 2 not the unseen, 4 every seen one, then frozen or not predicted left out |
| cvar.sv_mvd_disconnect_time | `sv_mvd_disconnect_time` = `15` | Minutes. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_server.cpp |
| cvar.sv_mvd_suspend_time | `sv_mvd_suspend_time` = `5` | Minutes. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_server.cpp: an empty block to the relays, a fresh game state at the resume |
| cvar.sv_mvd_allow_stufftext | `sv_mvd_allow_stufftext` = `0` (latched) | Runs text the game sends the dummy. | done | games/shooters_edge/net/q2_mvd_dummy.cpp, games/shooters_edge/net/q2_mvd_server.cpp: above 0 the game's stuffed lines run as the spectator's; below 0 relays' commands run as its own (the string-commands flag kept); 0 neither |
| cvar.sv_mvd_spawn_dummy | `sv_mvd_spawn_dummy` = `1` | When the scoreboard dummy is spawned. | done | games/shooters_edge/net/q2_mvd_dummy.cpp: 0 never, 1 when the rules know it (ours do), 2 always |
| cvar.sv_noreload | `sv_noreload` = `0` | Skips reloading a visited level. | done | Rules setting |
| cvar.mvd_shownet | `mvd_shownet` = `0` | Relay debug prints. | not-taken | docs/porting/server.md: debug build only, not taken |
| cvar.mvd_timeout | `mvd_timeout` = `90` | Relay upstream timeout. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_relay.cpp |
| cvar.mvd_suspend_time | `mvd_suspend_time` = `5` | Minutes. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_relay.cpp |
| cvar.mvd_wait_delay | `mvd_wait_delay` = `20` | Seconds of buffering before play. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_relay.cpp |
| cvar.mvd_wait_percent | `mvd_wait_percent` = `50` | Buffer fill before play. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_relay.cpp |
| cvar.mvd_buffer_size | `mvd_buffer_size` = `8` | Relay buffer. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_relay.cpp |
| cvar.mvd_username | `mvd_username` = `unnamed` | Name sent upstream. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_relay.cpp |
| cvar.mvd_password | `mvd_password` = `` (private) | Upstream password. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_relay.cpp |
| cvar.mvd_snaps | `mvd_snaps` = `10` | Seek snapshots. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_relay.cpp |
| cvar.mvd_admin_password | `mvd_admin_password` = `` (private) | `!mvdadmin` password. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_relay_viewers.cpp |
| cvar.mvd_part_filter | `mvd_part_filter` = `0` | Join/part messages in the relay. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_relay_viewers.cpp |
| cvar.flood_msgs | `flood_msgs` = `4` | Relay spectators' chat flood count (the server's; players' flood is the library's). | done | The relay's chat (games/shooters_edge/net/q2_mvd_relay_viewers.cpp), read from the rules' own setting in games/shooters_edge/net/q2_game.cpp; players' flood is the library's |
| cvar.flood_persecond | `flood_persecond` = `4` | Relay chat flood window. | done | same |
| cvar.flood_waitdelay | `flood_waitdelay` = `10` | Relay chat flood wait. | done | same |
| cvar.flood_mute | `flood_mute` = `0` | Relay spectators may not talk. | done | same: non-admin viewers may not talk |
| cvar.mvd_filter_version | `mvd_filter_version` = `0` | Rewrites `!version` in relay chat. | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp (`filter_version`): each `!version` in a chat-level print from the recorded server made `!versi0n` |
| cvar.mvd_default_map | `mvd_default_map` = `q2dm1` (latched) | Map shown while waiting. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_relay_viewers.cpp: the map's intermission spot, its checksum; q2dm1 when it does not load |
| cvar.mvd_stats_score | `mvd_stats_score` = `0` | Which stat is the chase score. | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp (`touch`): 0 the frags shown as 0, 1 the channel's number, else the followed player's |
| cvar.mvd_stats_hack | `mvd_stats_hack` = `0` | Dummy's stats copied to chasers. | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp (`stats_hack`): the masked stats from the spectator's to a chaser, before `mvd_stats_score` |
| cvar.mvd_freeze_hack | `mvd_freeze_hack` = `1` | Frozen dummy means intermission. | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp (`intermission`): a frozen spectator is the intermission: everyone follows it with the scoreboard, no target changes, each back to his player after |
| cvar.mvd_chase_prefix | `mvd_chase_prefix` = `xv 0 yb -64` | Layout prefix of the chase text. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_mvd_relay_viewers.cpp |
| cvar.sv_anticheat_required | `sv_anticheat_required` = `0` (latched) | Anticheat module required. | not-taken | `q2-gaps2-server.md` B1; docs/goals.md M17l |
| cvar.sv_anticheat_server_address | `sv_anticheat_server_address` = `anticheat.r1ch.net` (latched) | The service. | not-taken | same |
| cvar.sv_anticheat_error_action | `sv_anticheat_error_action` = `0` | When the service is down. | not-taken | same |
| cvar.sv_anticheat_message | `sv_anticheat_message` = `This server requires the r1ch.net anticheat module. Please see http://antiche.at/ for more details.` | Shown to refused players. | not-taken | same |
| cvar.sv_anticheat_badfile_action | `sv_anticheat_badfile_action` = `0` | On a modified file. | not-taken | same |
| cvar.sv_anticheat_badfile_message | `sv_anticheat_badfile_message` = `` | | not-taken | same |
| cvar.sv_anticheat_badfile_max | `sv_anticheat_badfile_max` = `0` | | not-taken | same |
| cvar.sv_anticheat_show_violation_reason | `sv_anticheat_show_violation_reason` = `0` | | not-taken | same |
| cvar.sv_anticheat_client_disconnect_action | `sv_anticheat_client_disconnect_action` = `0` | | not-taken | same |
| cvar.sv_anticheat_disable_play | `sv_anticheat_disable_play` = `0` | | not-taken | same |
| cvar.map_visibility_patch | `map_visibility_patch` = `1` | Repairs three stock maps' visibility. | done | `sv_vispatch` (synonym; facts in `q2-vispatch.md`) |
| cvar.map_noareas | `map_noareas` = `0` | Ignores area portals. | not-taken | `q2-server-gaps.md` "Not worth building" |
| cvar.map_override_path | `map_override_path` = `` | Folder of entity files and binary override files (feat.map.entfile, feat.map.override). | done | `sv_entpath` (synonym) for the entity files |
| cvar.nextserver | `nextserver` (made on demand) | The command the `nextserver` client word runs after a cinematic or picture level (`gamemap "!<next>"`); `gamemap "!*base1"` after the coop victory picture. | done | games/shooters_edge/net/q2_game.cpp (`start_show`): `gamemap "!<what follows>"`, `gamemap "!*base1"` after co-op's victory picture, else empty; tests/shooters_edge_q2_mapfix_test.cpp |
| cvar.z_perturb | `z_perturb` = `0` | Memory debugging. | not-taken | A check of the original's own memory zones; the engine allocates with the C++ allocator, whose faults the sanitizer builds catch |
| cvar.host_speeds | `host_speeds` = `0` | Frame timing prints. | not-taken | Exists only in builds that hold the client; the dedicated server holds none (the client's ledger has it) |
| cvar.developer | `developer` = `0` | Developer prints. | not-taken | `q2-server-gaps.md` "Not worth building" |
| cvar.timescale | `timescale` = `1` (cheat) | Game speed. | not-taken | same |
| cvar.fixedtime | `fixedtime` = `0` (cheat) | Fixed frame time. | not-taken | same |
| cvar.timedemo | `timedemo` = `0` (cheat) | Time a demo; reset at each map. | not-taken | same |
| cvar.logfile | `logfile` = `0` | 1 new log, 2 append. | done | engine/server/console_log.hpp (`con_logfile`, Quake II's spelling a synonym): 0 off, 1 a new file at each opening, 2 added to the end; every line the terminal prints |
| cvar.logfile_flush | `logfile_flush` = `0` | Flush per line / unbuffered. | done | engine/server/console_log.hpp (`con_logfile_flush`): 0 buffered, 1 each line flushed, 2 unbuffered; a change closes and opens the log with `Closing console log.` and `Logging console to <path>`; tests/server_console_log_test.cpp |
| cvar.logfile_name | `logfile_name` = `console` | Log at `logs/<name>.log`. | done | engine/server/console_log.hpp (`con_logfile_name`): `logs/<name>.log` in the game's folder |
| cvar.logfile_prefix | `logfile_prefix` = `[%Y-%m-%d %H:%M] ` | Time stamp per line. | done | engine/server/console_log.hpp (`con_logfile_prefix`): `<?>`, `@` and the local time's `%` patterns as `console_prefix` reads them |
| cvar.console_prefix | `console_prefix` = `` | Prefix on console lines. | not-taken | Our design: engine/server/server.cpp, main.cpp: before each line the console prints, `<?>` the syslog level, `@` its letter, `%` the time; the process's start-up errors on stderr carry none |
| cvar.dedicated | `dedicated` = `1` (read-only) on the dedicated program; `0` (no-set) otherwise | Whether dedicated. | not-taken | Tail (decided 2026-11-23), hosted game libraries only. |
| cvar.cl_running | `cl_running` = `0` (read-only) | State report (client builds). | not-taken | A client build's state; the dedicated server holds no client (the client's ledger has it) |
| cvar.cl_paused | `cl_paused` = `0` (read-only) | State report (client builds). | not-taken | A client build's state; the dedicated server holds no client (the client's ledger has it) |
| cvar.sv_running | `sv_running` = `0` (read-only) | State report. | done | games/shooters_edge/net/q2_game.cpp: answered read-only (2 while a map runs, 0 after `killserver`) |
| cvar.sv_paused | `sv_paused` = `0` (read-only) | State report. | done | games/shooters_edge/net/q2_game.cpp: answered read-only, 0 (a dedicated server never pauses) |
| cvar.com_date_format | `com_date_format` = `%Y-%m-%d` | For `$com_date`. | done | engine/console/clock_macros.hpp |
| cvar.com_time_format | `com_time_format` = `%H:%M` (`%H.%M` on Windows) | For `$com_time`. | not-taken | Our design: engine/console/clock_macros.hpp: `%H:%M` on every system (Windows' own default is `%H.%M`) |
| cvar.com_debug_break | `com_debug_break` = `0` | Debugger break (debug build only). | not-taken | docs/porting/server.md: debug build only, not taken |
| cvar.com_fatal_error | `com_fatal_error` = `0` | Fatal error mode. | done | engine/server/main.cpp (`com_fatal_error`, `ServerGame::drop_error` and `crash`): a game's recoverable error (a hosted library's error call, `Game Error: <text>`) prints the stars and `ERROR:` and drops every client with `Server crashed: <text>`, the server idle until the next map; with the setting the clients get `Server fatal crashed: <text>`, the console log `FATAL: <text>`, the stars go to the error stream (`<3>` before each with a `<?>` console prefix) and the process ends with 1; games/shooters_edge/net/q2_game.cpp |
| cvar.version | `version` (server info, read-only) | Version text. | not-taken | `q2-server-gaps.md` "Not worth building" |
| cvar.allow_download | `allow_download` = `0` on a dedicated server, `1` otherwise (archive) | Master download switch. | not-taken | Our design: `sv_download` (synonym) defaults to 1 (`games/shooters_edge/net/q2_game.cpp`) |
| cvar.allow_download_players | `allow_download_players` = `1` (archive) | `players/`. | done | `sv_download_players` (synonym) |
| cvar.allow_download_models | `allow_download_models` = `1` (archive) | `models/` and `sprites/`. | done | `sv_download_models` (synonym): models/ and sprites/ |
| cvar.allow_download_sounds | `allow_download_sounds` = `1` (archive) | `sound/`. | done | `sv_download_sounds` (synonym) |
| cvar.allow_download_maps | `allow_download_maps` = `1` (archive) | `maps/`. | not-taken | Our design: `sv_download_maps` (synonym); a map inside a pack is sent only to our clients, which keep it in its archive (docs/porting/content.md, downloads kept in the archive they came from) |
| cvar.allow_download_textures | `allow_download_textures` = `1` (archive) | `textures/` and `env/`. | done | `sv_download_textures` (synonym; games/shooters_edge/net/q2_game.cpp): textures/ and env/ |
| cvar.allow_download_pics | `allow_download_pics` = `1` (archive) | `pics/`. | done | `sv_download_pics` (synonym) |
| cvar.allow_download_others | `allow_download_others` = `0` | Anything else. | done | `sv_download_others` (synonym), off by default: any other folder |
| cvar.rcon_password | `rcon_password` = `` (private) | Remote console password. | done | Engine `rcon_password`; `games/shooters_edge/net/q2_admin.cpp` |
| cvar.sys_forcegamelib | `sys_forcegamelib` = `` (no-set) | Game library path tried first. | not-taken | Tail (decided 2026-11-23), hosted game libraries only: the library is loaded from a path the server is given (`server/q2_native.hpp`). |
| cvar.sys_allow_unsafe_savegames | `sys_allow_unsafe_savegames` = `0` (no-set) | Allows saves with libraries lacking the enhanced-savegame bit. | done | A rule (games/shooters_edge/net/q2_game.cpp) passed to the hosted library at start; a later set prints `sys_allow_unsafe_savegames may be set from command line only.` and is ignored (`native_set`, `kUnsafeSavesSetting`) |
| cvar.sys_history | `sys_history` | Command history size. | done | engine/server/terminal.hpp (`con_history`, Quake II's spelling a synonym): the last 128 lines typed kept, that many saved to `.syshistory` in the base game folder and read back, 0 neither, empty lines left out; tests/server_terminal_test.cpp |
| cvar.fs_autoexec | `fs_autoexec` = `1` | Rerun configs when `game` changes. | done | engine/server/main.cpp (`server_switch_mods`): a mod switch (our change of `game`) reads every layer's server.cfg again; 0 changes the folders alone |
| cvar.fs_debug | `fs_debug` = `0` | File debug prints. | not-taken | `q2-server-gaps.md` "Not worth building" |
| cvar.fs_fuzz_factor | `fs_fuzz_factor` = `0` | File fuzzing (debug). | not-taken | docs/porting/server.md: debug build only, not taken |
| cvar.fs_fuzz_filter | `fs_fuzz_filter` = `*` | File fuzzing filter (debug). | not-taken | docs/porting/server.md: debug build only, not taken |
| cvar.game | `game` = `` (latched, server info, not archived) | Game folder; empty is the base game. | not-taken | Our design: `sesv -game 2` and the mods (`sv_mod`); no `game` setting. |
| cvar.com_completion_mode | `com_completion_mode` = `1` | Tab completion. | done | engine/console/tab_list.hpp, shared with the player's console; on the server `con_completion_mode` with Quake II's spelling a synonym: 0 one a line, 1 columns, 2 by kind under their headings |
| cvar.com_completion_treshold | `com_completion_treshold` = `50` | Completion list size. | done | engine/console/tab_list.hpp: past it the first Tab asks `Press TAB again to display all <n> possibilities.`; `con_completion_threshold` on the server, Quake II's spelling a synonym |
| cvar.showpackets | `showpackets` = `0` | Packet prints (debug). | not-taken | `q2-server-gaps.md` "Not worth building" |
| cvar.showdrop | `showdrop` = `0` | Drop prints (debug). | not-taken | same |
| cvar.qport | `qport` = random | Client's qport. | not-taken | same |
| cvar.net_maxmsglen | `net_maxmsglen` = `1390` | Largest packet a client may ask for. | done | ours `sv_maxmsglen` (games/shooters_edge/net/q2_game.cpp), the reference's name a synonym; read by `q2_msglen` (`q2_msglen`, games/shooters_edge/net/q2_net_ext.cpp) |
| cvar.net_chantype | `net_chantype` = `1` | Channel kind (new or old). | not-taken | A client's own setting the server never reads; the server's half (the client's choice taken for protocol 36, `Invalid netchan type.`, the ` nc=` answer) is games/shooters_edge/net/q2_net_ext.cpp |
| cvar.net_ip | `net_ip` = `` | IPv4 bind address. | done | Engine `net_ip` |
| cvar.net_ip6 | `net_ip6` = `` | IPv6 bind address. | done | Engine `net_ip6` (engine/server/server.cpp): the IPv6 socket's address with `net_enable_ipv6` 2 (`open_ipv6`, engine/server/net_restart.hpp; `open_udp6_beside`, engine/net/transport.hpp); not fatal when it cannot be had; `showip` lists `Server UDP6` |
| cvar.net_port | `net_port` = `27910` | Port. | done | Engine `net_port`, Quake II's default port |
| cvar.net_clientport | `net_clientport` = any | Client's port. | not-taken | A client's own port; the dedicated server has no client (the client's ledger has it) |
| cvar.net_dropsim | `net_dropsim` = `0` | Simulated loss. | not-taken | Loss is simulated by the engine's simulated network in tests and labs (engine/net, net-link lab), not by a setting on a live server |
| cvar.net_log_enable | `net_log_enable` = `0` | Packet log. | done | engine/net/packet_log.hpp on every dedicated server (`net_log`, Quake II's spelling a synonym): each datagram in and out, `UDP`/`TCP` `recv`/`send`, the time in ms, the peer, the length, 16-byte hex rows with the letters; 1 a new file, 2 added to; the open, close and failure lines, a failure setting it back to 0; tests/net_packet_log_test.cpp |
| cvar.net_log_name | `net_log_name` = `network` | Packet log file. | done | engine/net/packet_log.hpp: `logs/<name>.log` in the game's folder, `.log` added unless given, odd letters made `_` |
| cvar.net_log_flush | `net_log_flush` = `0` | Packet log flush. | done | engine/net/packet_log.hpp: 0 buffered, 1 each entry flushed, 2 unbuffered; a change reopens the log |
| cvar.net_enable_ipv6 | `net_enable_ipv6` = 1 when the system has IPv6 | 2 also listens on IPv6. | done | Engine `net_enable_ipv6` 0..2 (default 1): 2 opens an IPv6 socket on `net_ip6` and `net_port` beside the IPv4 one, IPv6 only, both read by one transport; `net_restart` opens it again |
| cvar.net_ignore_icmp | `net_ignore_icmp` = `0` | Ignores unreachable notices. | not-taken | `q2-server-gaps.md` row 17 (our transport hears no ICMP) |

## 4. Server-to-client messages (svc) and protocol extensions the server writes

done 47, missing 0, not-taken 6

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| svc.muzzleflash | message 1 | A player's weapon flash (light and sound) or login/logout flash, by entity and kind. | done | Our rules send every gun's flash (`q2_fight.cpp` muzzle_flash, `q2_monsters.cpp`), written by `q2_wire.cpp`; the shot's sound is in the flash, never sent twice |
| svc.muzzleflash2 | message 2 | A monster's muzzle flash by entity and flash number. | done | Our rules send every gun's flash (`q2_fight.cpp` muzzle_flash, `q2_monsters.cpp`), written by `q2_wire.cpp`; the shot's sound is in the flash, never sent twice |
| svc.temp_entity | message 3 | Temporary effects (explosions, trails, sparks, beams). | done | `games/shooters_edge/server/q2_wire.cpp` (`write_temp_entity`, `kTeNames`); our names without a protocol code are dropped |
| svc.layout | message 4 | The scoreboard or help layout text. | done | `games/shooters_edge/server/q2_wire.cpp` |
| svc.inventory | message 5 | The 256 inventory counts. | done | `games/shooters_edge/server/q2_wire.cpp` |
| svc.nop | message 6 | Padding (debug builds pad packets with it). | done | Not needed to be sent; readers skip it |
| svc.disconnect | message 7 | Tells the client to disconnect; at a drop, and twice to every client at shutdown. | done | games/shooters_edge/net/q2_net_server.cpp (`q2_net_shutdown`): twice to every client at shutdown, killserver and quit |
| svc.reconnect | message 8 | Tells clients to reconnect, at a full restart (`Server restarted`) or `recycle`. | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: sent at `recycle` and when relay mode takes the port (`Server restarted`); a `map` restarting the game in place still moves clients by stuffed `changing` and `reconnect` (proto.mapchange) |
| svc.sound | message 9 | A sound with optional volume, attenuation, time offset, entity and channel, and position; a 16-bit index under protocol extensions. | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `games/shooters_edge/server/q2_wire.cpp`: always carries a position (S5); our rules' sounds go to everyone (S3); a library's bad volume clamped (S16) |
| svc.print | message 10 | A text line at a level (pickup, medium, high, chat). | done | the client's `msg` level honoured: a print below it is not sent (`server/q2_wire.cpp`), the rule shared with Quake's server (`classic/protocol/info.hpp`'s `print_reaches`, chat always reaching there) |
| svc.stufftext | message 11 | Text the client runs. | done | `games/shooters_edge/net/q2_net_server.cpp`, `games/shooters_edge/server/q2_wire.cpp` |
| svc.serverdata | message 12 | Protocol, server count, attract flag, game folder, player number (-1 on a cinematic or picture), level name; 35 adds an enhanced flag, the minor, an advanced-delta flag and the strafe-fix byte; 36 adds the minor, the server state, then a flag word (minor 1024 and up) or three move bytes. | done | games/shooters_edge/server/q2_wire.cpp; the 35 and 36 tails in games/shooters_edge/server/q2_wire_ext.cpp (`q2_server_data_tail`): no movement hacks, so a client predicts our move |
| svc.configstring | message 13 | One configstring by index. | done | `games/shooters_edge/server/q2_wire.cpp`; index overflow is not an error (S14) |
| svc.spawnbaseline | message 14 | One entity's baseline. | done | `games/shooters_edge/server/q2_wire.cpp` |
| svc.centerprint | message 15 | Centred text. | done | `games/shooters_edge/server/q2_wire.cpp` |
| svc.download | message 16 | A download piece with size and percent. | done | games/shooters_edge/net/q2_net_download.cpp: pieces fill the packet (1386 bytes on 1390), one a `nextdl`, paced by the rate; the piece rule shared with Quake's server (classic/protocol/download_pace.hpp; Quake keeps its rate-sized pieces). Held by tests/shooters_edge_q2_download_pace_test.cpp |
| svc.playerinfo | message 17 | The player state delta. | done | `games/shooters_edge/server/q2_wire.cpp` |
| svc.packetentities | message 18 | The entity deltas of a frame. | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: 35 and 36 send the list with no id byte and long solids (games/shooters_edge/server/q2_wire_ext.cpp), the extended limits' forms under them (q2_wire_limits.cpp) |
| svc.deltapacketentities | message 19 | Unused by the server (defined only). | done | Not sent by either |
| svc.frame | message 20 | Frame number, delta frame, suppress count, area bits. | done | 34 and the later frame top (games/shooters_edge/server/q2_wire_ext.cpp, `q2_write_frame_top_ext`: extra flags, the 27-bit number and back offset, frame flags); the area bits mark the areas joined to the viewer's (`q2_write_area_bits`, every area from area 0) |
| svc.zpacket | message 21 | A deflate-compressed block of messages (35 and 36 with zlib). | done | games/shooters_edge/net/q2_net_server.cpp (`packed`, engine/core/deflate.hpp): a signon message deflated when it comes out smaller |
| svc.zdownload | message 22 | A compressed download piece straight from a pack (36 minor 1021 and up). | done | A `.pkz` file stored deflated goes from its start to a 36 client at minor 1021 or later with zlib as the entry's raw deflate stream in pieces of message 22 (`Q2NetConfig::read_deflated`, `served_deflated`, `vfs::deflated`); its size and `sv_max_download_size` are the stream's; pieces of 1024 bytes as for message 16 |
| svc.gamestate | message 23 | The whole game state at once (36). | done | games/shooters_edge/server/q2_wire_ext.cpp (`q2_signon_streams`), for 36 below minor 1024 |
| svc.setting | message 24 | A server setting: index and value; two kinds, 0 player updates and 1 frame rate. | not-taken | Sent only by builds with the variable frame rate, which the reference's default build leaves out; our own clients get the frame rate by the `*ge_ext` bit |
| svc.configstringstream | message 25 | Configstrings streamed (36). | done | games/shooters_edge/server/q2_wire_ext.cpp (`q2_signon_streams`), 36 from minor 1024 |
| svc.baselinestream | message 26 | Baselines streamed (36). | done | games/shooters_edge/server/q2_wire_ext.cpp (`q2_signon_streams`), 36 from minor 1024 |
| svc.ge-own | messages 119, 121 to 124, 126, 127 | Ours only, to our own clients that ask by `*ge_ext`: item back times, voice, move feel, download archive, others' moves, jump share, account key. | done | `games/shooters_edge/net/q2_net_server.cpp` (not in the reference; listed so the ledger accounts for every byte ours writes) |
| proto.34 | protocol 34 | The stock protocol. | done | `games/shooters_edge/net/q2_net.hpp`, `games/shooters_edge/net/q2_net_server.cpp` |
| proto.35 | protocol 35 | First extended client protocol: 8-bit qport, larger packets, minor 1903 to 1905. | done | games/shooters_edge/net/q2_net_server.cpp, games/shooters_edge/net/q2_net_ext.cpp: the byte qport, the asked packet length, the compact move from 1904, long solids from 1905 |
| proto.36 | protocol 36 | The extended protocol: compression, streams, packed frames, new or old channel, minor 1015 to 1026 (1024 and 1025 extended limits, 1026 player fog). | done | games/shooters_edge/net/q2_net_server.cpp, games/shooters_edge/net/q2_net_ext.cpp, games/shooters_edge/server/q2_wire_ext.cpp: compression, streams, the packed frame, either channel; player fog is never sent (no game of ours sets it); the extended limits are their own row |
| proto.37 | protocol 37 | The multi-view recording's protocol, minor 2009 to 2013; never on UDP. | done | games/shooters_edge/server/q2_mvd2.hpp written live by games/shooters_edge/net/q2_mvd_server.cpp to a file and over TCP |
| proto.challenge-list | `challenge <n> p=34,35,36` | Lists the protocols on offer. | done | games/shooters_edge/net/q2_net_server.cpp: `challenge <n> p=34,35,36` |
| proto.challenge-table | challenges | A random 31-bit number per address, 1024 kept, oldest replaced, used once. | done | games/shooters_edge/net/q2_net_server.cpp (`challenge_key`): a new 31-bit number per address and port, 1024 kept, the oldest replaced, used up on admission |
| proto.connect-answer | `client_connect[ nc=0\|1][ ac=1][ dlserver=<url>] map=<map>` | The admission answer: channel kind for 36, anticheat, HTTP download address, the map name. | done | games/shooters_edge/net/q2_net_server.cpp: ` nc=` for 36, ` dlserver=` when set, ` map=` always (no anticheat build, so no ` ac=`) |
| proto.mapchange | map change | Every client is stuffed `changing map=<map>` before the level loads and `reconnect` after. | done | Added (upstream); our level changes stuff `changing` and `reconnect` |
| proto.fragments | the new channel's fragments | A message larger than a packet in pieces; packet size agreed by `net_maxmsglen`. | done | plugins/classic/protocol/channel.cpp (`channel_read`, `newer_transmit`): 30-bit sequences, pieces with a fragment word, a 32768-byte reliable message; pieces held to our 1450-byte datagram |
| proto.extended-limits | protocol extensions (36 minor 1024 and up) | When the library sets the protocol-extensions bit: `protocol` key 36, up to 8192 entities and more configstrings, a 16-bit sound index; old clients refused. | done | The table both sides read (plugins/classic/protocol/q2_limits.hpp); a hosted library that sets the feature gets the extended layout (13630 strings), up to 8192 entities, models, sounds and images by the longer lists (model 255 kept for the players), its entities' extension block read (more effects, alpha, scale, loop volume and attenuation), a sound past 255 as a word (flag 32), and every client the extended forms: the server data's flags word, the entity and player deltas (games/shooters_edge/server/q2_wire_limits.cpp, q2_wire_ext.cpp), the streams ended by the extended count; the second level (23-bit coordinates, 64 stats, the damage blend, word move timers, positions the game writes) for a library of the newer interface (`NativeHost::new_interface`, api.newgame). The refusal and the `protocol` key: msg.oob.extended-limits. tests/shooters_edge_q2_native_test.cpp `a_library_with_protocol_extensions_gets_the_extended_limits`, tests/shooters_edge_q2_wire_limits_test.cpp, tests/classic_q2_limits_test.cpp |
| proto.version-probe | version probe at `new` | The client is made to report its `version`; one that has not by `begin` is dropped silently; the console logs it. | done | games/shooters_edge/net/q2_net_server.cpp (`probe`): asked after the server data, so a recording still opens with it |
| proto.force-reconnect | forced reconnect | Random values the client must bring back from a reconnect (`sv_force_reconnect`). | done | games/shooters_edge/net/q2_net_server.cpp (`first_new`); our client runs the stuffed settings (`q2_client.cpp`); fixed 2026-11-23: our client lost the stuffed settings when it connected again, so it never came back with the value and was dropped (`net/q2_client.cpp` keeps them across the redirect); held by `tests/shooters_edge_q2_server_sweep_test.cpp` |
| proto.areabits-cull | areas cut the frame | An entity behind a shut door (areas not joined) is not sent. | done | `q2_sight_keeps`: the viewer's area joined to the entity's first or second area (`Q2SightLinks::area1`, `area2`), for our rules (`map_area_groups`) and a hosted library (`NativeHost::area_groups`); area 0 joined to nothing |
| proto.pvs-cull | the visible set cuts the frame | Entities outside the fat visible set skipped; beams by the hearing set; sound-only entities beyond a distance cut. | done | `cut_to_view` (`games/shooters_edge/server/q2_wire.hpp`) |
| proto.phs-sounds | sounds by the hearing set and areas | Positioned sounds and multicasts only to clients that can hear them. | done | Hosted library: `send_to` tests the client's origin in the point's set and the areas; our rules: `q2_sight_hears` cuts the frame's effects and sounds (no attenuation: everyone); effects by the hearing set, the wider one (D) |
| proto.entity-follow | a sound follows its entity | Position left out so the client places it on the moving entity. | done | The position is left out when the client's frame holds the entity (not a brush model, no explicit point): `c_positioned_sound` per client by its visible set and areas, our rules by the frame just written (`follows`) |
| proto.overflow-priority | frame overflow | Low-priority sounds and effects then entities dropped to fit. | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: the frame is sized to the packet's room after the reliable part (`channel_unreliable_room`, `Q2Wire::room`): entities fill first in number order (players first), then effects and sounds while they fit; a hosted library's datagram whole or not at all; no per-sound priority |
| proto.move-repeat | lost moves repeated | With some packets lost, the oldest move is run again for each. | done | games/shooters_edge/net/q2_net_server.cpp: the last command run again for each lost packet (frame flag 4 past what the packet repeats) |
| proto.rate-hold | rate holds frames | A frame is held when recent messages pass the rate; suppress count. | done | `games/shooters_edge/net/q2_net_server.cpp` |
| proto.client-setting-msg | client settings (message 5) | A client's index and value: no gun, no blend, recording, player updates, frame rate, no gibs, no footsteps, no prediction, no flares. | done | games/shooters_edge/net/q2_net_server.cpp: every index read and kept (`asked`), `status s` shows them; no gun, no blend, recording, no gibs, no footsteps and no prediction shape the frames (games/shooters_edge/server/q2_wire_ext.cpp) |
| proto.nogibs | client setting 10 | Server stops sending gibs to that client. | done | games/shooters_edge/server/q2_wire_ext.cpp (`q2_entity_for_ext`) |
| proto.nofootsteps | client setting 11 | Server clears footstep events for that client. | done | games/shooters_edge/server/q2_wire_ext.cpp (`q2_entity_for_ext`) |
| proto.packet-limits | per-packet limits | At most 8 string commands, 8 user info updates, 32 moves per frame, 4 frames; an oversize string command drops (`oversize stringcmd`). | not-taken | Tail (decided 2026-11-23): one move a packet read (`read_messages`); the other limits not matched. |
| proto.name-cut | the name cut to 15 characters, high bits cleared | | done | games/shooters_edge/net/q2_net_server.cpp (`cut_name`) |
| proto.ip-key | `ip` user info key forced to the address | On connect and every change. | done | games/shooters_edge/net/q2_net_server.cpp: forced at the connect |
| proto.userinfo-mvdspec | `mvdspec` and `password` keys removed from user info | So a library never sees them. | done | games/shooters_edge/net/q2_net_server.cpp |

## 5. What the server accepts from clients: clc messages, string commands, out-of-band words

done 45, missing 0, not-taken 12

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| clc.nop | message 1 | Nothing. | done | `games/shooters_edge/net/q2_net_server.cpp` |
| clc.move | message 2 | Checksum byte, last frame, three delta-coded moves. | done | Read (`read_q2_move`; 35 and 36 `read_q2_move_ext` in games/shooters_edge/net/q2_net_ext.cpp); lost moves repeated |
| clc.userinfo | message 3 | A new user info. | not-taken | Tail (decided 2026-11-23), hosted game libraries only: read; with a hosted library only the name reaches it in a game (S8) |
| clc.stringcmd | message 4 | A string command. | done | `string_command` |
| clc.setting | message 5 | A client setting (extended). | done | games/shooters_edge/net/q2_net_server.cpp |
| clc.move_nodelta | message 10 | A move asking for a full frame (extended). | done | games/shooters_edge/net/q2_net_ext.cpp (`read_q2_batch`), run in games/shooters_edge/net/q2_net_server.cpp |
| clc.move_batched | message 11 | Batched moves (extended). | done | games/shooters_edge/net/q2_net_ext.cpp (`read_q2_batch`), run in games/shooters_edge/net/q2_net_server.cpp |
| clc.userinfo_delta | message 12 | A user info change as a delta (extended). | done | games/shooters_edge/net/q2_net_server.cpp (`userinfo_delta`) |
| ucmd.new | `new` | Starts the signon. | done | `string_command` |
| ucmd.begin | `begin` | Enters the game (refused silently before the client is primed or on a cinematic). | not-taken | Tail (decided 2026-11-23), hosted game libraries only: done; the library's connect call runs here, not at the connect packet (S9) |
| ucmd.baselines | `baselines` | Signon step (stock prompt). | done | `string_command` |
| ucmd.configstrings | `configstrings` | Signon step (stock prompt). | done | `string_command` |
| ucmd.nextserver | `nextserver` | After a cinematic or picture level, runs `nextserver` (or stops the server when it is empty). | done | games/shooters_edge/net/q2_net_server.cpp, games/shooters_edge/net/q2_game.cpp (`next_server`): the level's count, once, a movie or a co-op picture; the setting run and emptied, `killserver` when empty |
| ucmd.disconnect | `disconnect` | Leaves; the server's own drop line is silent, the library announces it (stock: `<name> disconnected`). | done | `string_command` drops; our rules say `<name> disconnected` (`games/shooters_edge/server/q2_server.cpp`) |
| ucmd.info | `info` | The server info to the asking client. | done | games/shooters_edge/net/q2_net_server.cpp (`player_query`, `q2_info_lines`) |
| ucmd.sinfo | `sinfo` | The client's own connection details (the miscinfo block). | done | games/shooters_edge/net/q2_net_server.cpp (`q2_misc_lines`) |
| ucmd.download | `download <file> [offset]` | Starts or resumes a download. | done | games/shooters_edge/net/q2_net_download.cpp (`q2_start_download`): the name made plain, every kind, the size limit, the size-differs message, a resume at the end one empty piece at 100; the first piece owed by the command itself |
| ucmd.download-http | `download http` | Marks the client as fetching by HTTP, so `status` shows HTTP. | done | games/shooters_edge/net/q2_net_server.cpp; shown as HTTP in `status` and `status d` |
| ucmd.nextdl | `nextdl` | Next piece (stock clients). | done | `string_command` |
| ucmd.stopdl | `stopdl` | Stops a download. | done | games/shooters_edge/net/q2_net_server.cpp; held by `tests/shooters_edge_q2_server_sweep_test.cpp` (a download half sent stopped) |
| ucmd.cvar-answer | the hidden setting answer (`<DEL>c`) | A client's answer to a setting query (version, cvar bans, `stuffcvar`, `connect` check, anticheat token, `console`). | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: setting bans' answers read (`games/shooters_edge/net/q2_net_server.cpp`); version, connect, actoken and console answers not asked |
| ucmd.nogamedata | `nogamedata` | Toggles game frames being sent to the client. | done | games/shooters_edge/net/q2_net_server.cpp |
| ucmd.lag | `lag [player]` | Lag statistics for self or a player. | done | games/shooters_edge/net/q2_net_server.cpp (`q2_lag_lines`); the player found by a part of a name always |
| ucmd.packetdup | `packetdup <n>` | Ask the server for duplicate packets. | done | games/shooters_edge/net/q2_net_server.cpp |
| ucmd.aclist | `aclist` | Anticheat list for clients. | not-taken | `q2-gaps2-server.md` B1 |
| ucmd.acinfo | `acinfo` | Anticheat details for clients. | not-taken | same |
| ucmd.say-idle | `say`, `say_team` count as activity | For the idle kick. | done | `idle_chat` in `string_command` |
| ucmd.filtered | a filtered command | `addfiltercmd`'s action instead of the command. | done | `filtered` in `games/shooters_edge/net/q2_net_server.cpp` |
| ucmd.forward | any other word | Passed to the game library (after cvar bans and filters). | done | `rules->client_command` |
| oob.ping | `ping` | Answers `ack`. | done | `read_oob` |
| oob.ack | `ack` | From a master: notes the time; otherwise silent. | done | Ignored |
| oob.status | `status` | `print`, the server info (slots less the reserved), then `<frags> <ping> "<name>"` lines; rate limited. | done | games/shooters_edge/net/q2_net_server.cpp, games/shooters_edge/net/q2_admin.cpp (`q2_status_keys`): the rules' live keys, the build's, `port` and the administrator's own; tests/shooters_edge_q2_admin_test.cpp |
| oob.info | `info <version>` | `<hostname> <map> <n>/<slots>` for versions 34 to 36; ignored with 1 slot or another version. | not-taken | Our design: games/shooters_edge/net/q2_net_server.cpp: versions 34 to 36 answered; others get `<hostname>: wrong version` (the stock answer); reserved slots counted |
| oob.getchallenge | `getchallenge` | Answers `challenge <n> p=34,35,36`. | done | games/shooters_edge/net/q2_net_server.cpp |
| oob.connect | `connect <proto> <qport> <challenge> "<userinfo>" [...]` | Admits or refuses a client. | not-taken | Our design: protocols 34, 35 and 36 (games/shooters_edge/net/q2_net_server.cpp); a reconnect from the same address and port is ignored (S21); refusal words in section 7 |
| oob.rcon | `rcon <password> <line>` | Runs any console command, the answer cut into packets; macros expanded. | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: `q2_rcon` (`games/shooters_edge/net/q2_admin.cpp`): admin words and the rules' console only, one answer packet (S29) |
| oob.passive | `passive_connect` | Sent by `pickclient` to a waiting client. | done | games/shooters_edge/net/q2_admin.cpp |
| oob.blackhole | blackholed addresses | Every packet ignored. | done | Engine blackholes |
| oob.before-game | before a game starts only `rcon` is answered | | done | games/shooters_edge/net/q2_net_server.cpp: after `killserver` (the one way to have no game) only `rcon` is answered |
| mvdcmd.mvdadmin | `!mvdadmin <password>` | Relay: become an admin (`[MVD] Granted admin status.`, `[MVD] Lost admin status.`, `[MVD] Invalid password.`). | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp |
| mvdcmd.fwd | `fwd <cmd>` | Relay: forward a command upstream. | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp: the string command up when the server took the flag |
| mvdcmd.say | `say`, `say_team` | Relay chat, `[MVD] <name>: <text>`. | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp: the channel's viewers, high level in the bright font, the flood rule |
| mvdcmd.follow | `follow`/`chase [id]` | Relay: chase a player (`[MVD] Chasing <name>.`; targets q, i, r, b, p). | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp: slots, names, `!q` `!i` `!r` `!b` `!p` `!!`; a new target is a new signon (the view reconnects) |
| mvdcmd.autofollow | `autofollow`/`autochase add\|rm\|set\|list\|on\|off` | Relay: automatic chase list. | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp |
| mvdcmd.observe | `observe`, `spectate`, `observer`, `spectator`, `obs`, `spec` | Relay: free fly. | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: games/shooters_edge/net/q2_mvd_relay_viewers.cpp: back to the view's own choice (the lowest playing); no free flight |
| mvdcmd.inven | `inven`/`menu` | Relay: main menu. | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp: the Main Menu's rows |
| mvdcmd.invnext | `invnext` | Relay menu next. | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp |
| mvdcmd.invprev | `invprev` | Relay menu previous. | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp |
| mvdcmd.invuse | `invuse` | Relay menu select. | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp: every row, `set uf <n> u` |
| mvdcmd.help | `help`/`score` | Relay: the scoreboard. | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: games/shooters_edge/net/q2_mvd_relay_viewers.cpp: a scoreboard of the players by frags (no recorder's dummy to send its own) |
| mvdcmd.oldscore | `oldscore(s)`/`lastscore(s)` | Relay: the last map's scores. | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: games/shooters_edge/net/q2_mvd_relay_viewers.cpp: the last level's scoreboard, kept at the next game state |
| mvdcmd.putaway | `putaway` | Relay: close the layout. | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp |
| mvdcmd.channels | `channels` | Relay: list channels. | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp |
| mvdcmd.clients | `clients`/`players` | Relay: list players. | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: games/shooters_edge/net/q2_mvd_relay_viewers.cpp: the list and its pages; the round trip shows 0 |
| mvdcmd.join | `join [id]` | Relay: enter a channel. | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp |
| mvdcmd.leave | `leave` | Relay: back to the waiting room. | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp |
| mvdcmd.commands | `commands` | Relay: the list of these commands. | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp |

## 6. The game library interface: what the server gives a library (imports) and calls in it

Ours: the native host (`games/shooters_edge/server/q2_native.hpp`, `q2_native_calls.cpp`,
`q2_native_abi.hpp`, `q2_native_save.cpp`).

done 66, missing 0, not-taken 22

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| api.bprintf | `bprintf(level, fmt)` | Print to everyone whose `msg` level allows it (and to the recorder). | done | each player sent only what its `msg` level lets through, the event kept for a recording (`server/q2_native_calls.cpp`) |
| api.dprintf | `dprintf(fmt)` | Print to the server console. | done | `c_dprintf` |
| api.cprintf | `cprintf(ent, level, fmt)` | Print to one client (or the console when no entity). | done | held back below the player's `msg` level (`server/q2_native_calls.cpp`'s `native_print`) |
| api.centerprintf | `centerprintf(ent, fmt)` | Centred print to one client. | done | `c_centerprintf` |
| api.sound | `sound(ent, channel, index, vol, atten, timeofs)` | A sound from an entity. | not-taken | Tail (decided 2026-11-23), hosted game libraries only: `c_sound`: the hearing set and areas (`send_to`), the position left out for a client whose frame holds the entity; bad values clamped not refused (S16) |
| api.positioned_sound | `positioned_sound(origin, ent, ...)` | A sound at a point. | not-taken | Tail (decided 2026-11-23), hosted game libraries only: same |
| api.configstring | `configstring(index, text)` | Sets a configstring. | done | `c_configstring` |
| api.error | `error(fmt)` | Stops the level with `Game Error: <text>`. | done | Same meaning in our words (the player-text rule makes our words the intended ones). our host fails the level with its own words: verify the console line |
| api.modelindex | `modelindex(name)` | Index of a model, adding it; full: `<call>(<name>): overflow` stops the level unless the library set the index-overflow bit (then 0). | not-taken | Tail (decided 2026-11-23), hosted game libraries only: ours always returns 0 (S14) |
| api.soundindex | `soundindex(name)` | Index of a sound. | not-taken | Tail (decided 2026-11-23), hosted game libraries only: S14 |
| api.imageindex | `imageindex(name)` | Index of an image. | not-taken | Tail (decided 2026-11-23), hosted game libraries only: S14 |
| api.setmodel | `setmodel(ent, name)` | Sets an entity's model and its box for a brush model. | done | `c_setmodel` |
| api.trace | `trace(start, mins, maxs, end, pass, mask)` | Box trace through the world and entities. | done | `c_trace` |
| api.pointcontents | `pointcontents(p)` | Contents at a point. | done | `c_pointcontents` |
| api.inPVS | `inPVS(a, b)` | Visible-set test with areas. | done | `c_in_pvs` (area 0 joined to nothing) |
| api.inPHS | `inPHS(a, b)` | Hearing-set test with areas. | done | `c_in_phs` |
| api.SetAreaPortalState | `SetAreaPortalState(portal, open)` | Opens or shuts an area portal. | done | `c_set_area_portal` (the frame does not use it, S1) |
| api.AreasConnected | `AreasConnected(a, b)` | Whether two areas are joined. | done | `c_areas_connected`; area 0 joined to nothing, as the extended server |
| api.linkentity | `linkentity(ent)` | Places an entity in the world's tree. | done | `c_linkentity` |
| api.unlinkentity | `unlinkentity(ent)` | Removes it. | done | `c_unlinkentity` |
| api.BoxEdicts | `BoxEdicts(mins, maxs, list, max, type)` | Entities in a box. | not-taken | Tail (decided 2026-11-23), hosted game libraries only: entity-number order, not the tree's (S15) |
| api.Pmove | `Pmove(pm)` | The player move (the old or the new move by the library's version). | done | `c_pmove` (Quake II's move, `plugins/classic`) for version 3 |
| api.multicast | `multicast(origin, to)` | Sends the built message to all, the visible set or the hearing set (reliable or not). | done | `c_multicast`: to all, or by the client's origin in the point's visible or hearing set with the areas joined (`send_to`) |
| api.unicast | `unicast(ent, reliable)` | Sends the built message to one client. | done | `c_unicast` |
| api.WriteChar | `WriteChar` | Message byte, signed. | done | |
| api.WriteByte | `WriteByte` | | done | |
| api.WriteShort | `WriteShort` | | done | |
| api.WriteLong | `WriteLong` | | done | |
| api.WriteFloat | `WriteFloat` | Not implemented: stops with `PF_WriteFloat not implemented`. | not-taken | Tail (decided 2026-11-23), hosted game libraries only: ours writes the float. |
| api.WriteString | `WriteString` | | done | |
| api.WritePosition | `WritePosition` | Wider positions under protocol extensions with the new interface. | done | Version 3 only |
| api.WriteDir | `WriteDir` | | done | |
| api.WriteAngle | `WriteAngle` | | done | |
| api.TagMalloc | `TagMalloc(size, tag)` | Tagged zeroed memory (a tag past the range stops with `<call>: bad tag`). | done | `c_tag_malloc` |
| api.TagFree | `TagFree(p)` | | done | |
| api.FreeTags | `FreeTags(tag)` | | done | |
| api.cvar | `cvar(name, value, flags)` | Registers or finds a setting; extended flags masked (`Game attempted to set extended flags on '<name>', masked out.`). | not-taken | Tail (decided 2026-11-23), hosted game libraries only: `c_cvar`; first-default rule and masking line not matched. |
| api.cvar_set | `cvar_set(name, value)` | Sets a setting as a user would (refusals apply). | done | `c_cvar_set` |
| api.cvar_forceset | `cvar_forceset(name, value)` | Sets regardless. | done | `c_cvar_forceset` |
| api.argc | `argc()` | | done | |
| api.argv | `argv(n)` | | done | |
| api.args | `args()` | | done | |
| api.AddCommandString | `AddCommandString(text)` | Text for the console, run at the end of the frame. | not-taken | Tail (decided 2026-11-23), hosted game libraries only: only map changes, the load-game line and assignments to settings run (S11) |
| api.DebugGraph | `DebugGraph(value, color)` | A client-side graph; nothing on a dedicated server. | done | `c_debug_graph` (nothing) |
| api.export.Init | `Init` | Called at load. | done | |
| api.export.Shutdown | `Shutdown` | | done | |
| api.export.SpawnEntities | `SpawnEntities(map, entities, spawnpoint)` | | done | |
| api.export.WriteGame | `WriteGame` / `ReadGame` | Game file of a save (with an autosave flag). | done | `q2_native_save.cpp` |
| api.export.WriteLevel | `WriteLevel` / `ReadLevel` | Level file of a save. | done | same |
| api.export.ClientConnect | `ClientConnect(ent, userinfo)` | May refuse with `rejmsg` (`<reason>` + `Connection refused.`). | not-taken | Tail (decided 2026-11-23), hosted game libraries only: called at `begin` (S9) |
| api.export.ClientBegin | `ClientBegin` | | done | |
| api.export.ClientUserinfoChanged | `ClientUserinfoChanged` | Every user info change. | not-taken | Tail (decided 2026-11-23), hosted game libraries only: only a name change in a game (S8) |
| api.export.ClientDisconnect | `ClientDisconnect` | For a client in the game (any client with the all-disconnects bit). | done | |
| api.export.ClientCommand | `ClientCommand` | | done | |
| api.export.ClientThink | `ClientThink` | Each move; under the move-time budget. | not-taken | Tail (decided 2026-11-23), hosted game libraries only: unchecked by `sv_enforcetime` (S12) |
| api.export.RunFrame | `RunFrame` | | done | |
| api.export.ServerCommand | `ServerCommand` | The `sv` word. | done | (cmd.sv) |
| api.export.edicts | entity array, size, count, max | Checked: `Game library returned bad size of edict_t`, `Game library returned bad number of max_edicts`, `Game library corrupted maxclients value`. | done | Same meaning in our words: the host checks a library's entities and says so in its own words. |
| api.ping | each client's ping given to the library | Before each frame, by `sv_calcpings_method`. | done | `run_frame` (`server/q2_native.cpp`) gives the measured ping; `shooters_edge_q2_native_test` |
| api.apiversion | version 3 or 3302 | `Game library is version <n>, expected 3 or 3302`. | done | `games/shooters_edge/server/q2_native.cpp` (`load_native_q2`): 3 and 3302 taken, any other refused with both named |
| api.newgame | the second interface (version 3302) | A library built on the newer client and move records (and the newer move), the same calls otherwise. | done | `server/q2_native_abi.hpp` (`NativeMoveStateNew`, `NativePlayerNew`, `NativeClientHeadNew`, `NativeMoveBlockNew`), `NativeHost::new_interface` and `with_player`, `q2_native_calls.cpp` (`run_pmove`: 32-bit state, the ground plane, the ladder's mask and flag); `plugins/classic/q2_move.hpp` (`coord_bits`, `time_shift`, `ladder_mask`, `ladder_flag`); the 3302 fixture game (tests/shooters_edge_q2_native_test.cpp). The move's other traces pass the player's own mask (a dead player's or a living one's): derived, not read |
| api.loadlib | the library from `<dir>/<game>/game<cpu>.<ext>` | `sys_forcegamelib` first, then the home and the library folders for the game folder, then for the base folder; `Loaded game library from <path>`, `Failed to load game library: <error>`. | done | Same meaning in our words: the host loads the library path it is given and names a failure in its own words. |
| api.ex | the extended interface (`GetGameAPIEx`) | Extended imports and exports, versions 1 to 3 (2 added entity customizing, 3 the per-client visibility call). | done | games/shooters_edge/server/q2_native.cpp: `GetGameAPIEx` after the stock entry and before `Init`, our table at version 3, the game's taken at 1 or more; tests/shooters_edge_q2_native_test.cpp |
| api.ex.local_sound | `local_sound` | A sound to one client only. | done | games/shooters_edge/server/q2_native_calls.cpp (`x_local_sound`): to the target alone on its own entity, reliable with the reliable channel bit |
| api.ex.get_configstring | `get_configstring` | Reads a configstring. | done | games/shooters_edge/server/q2_native_calls.cpp: `PF_GetConfigstring: bad index: <n>` stops the map |
| api.ex.clip | `clip` | Trace against one entity. | done | games/shooters_edge/server/q2_native_calls.cpp (`x_clip`): the world, a brush or trigger by its model (turned), else its box; the answer names the entity |
| api.ex.inVIS | `inVIS` | Visibility test of a chosen kind (visible, hearing, with or without areas). | done | games/shooters_edge/server/q2_native_calls.cpp (`x_in_vis`): visible or hearable, the areas unless no-areas is added |
| api.ex.GetExtension | `GetExtension` | Named APIs (the file system; debug drawing). | done | games/shooters_edge/server/q2_native_calls.cpp: `FILESYSTEM_API_V1`; anything else (debug drawing on a dedicated server too) null |
| api.ex.TagRealloc | `TagRealloc` | Grows a tagged block (`untagged allocation not allowed` for a new one). | done | games/shooters_edge/server/q2_native_calls.cpp (`x_tag_realloc`): tag kept, growth not cleared, 0 frees, `PF_TagRealloc: untagged allocation not allowed` |
| api.ex.CanSave | export `CanSave` | The library says whether saving is allowed (in place of the dead-player check). | done | games/shooters_edge/server/q2_native_save.cpp: the game's answer in place of the dead player's check (refused: "The game does not allow saving now.") |
| api.ex.PrepFrame | export `PrepFrame` | Called before frames are built. | done | games/shooters_edge/server/q2_native.cpp (`take_view`): in place of the server's event clearing |
| api.ex.RestartFilesystem | export `RestartFilesystem` | On `fs_restart`. | not-taken | Our design: taken; never called, as our server has no `fs_restart` and no `game` change while running |
| api.ex.CustomizeEntityToClient | export `CustomizeEntityToClient` | Per-client entity edits. | done | games/shooters_edge/server/q2_native.cpp (`per_client`, games/shooters_edge/server/q2_wire.cpp `Q2Raw::touch`): at version 3, the record sent in place of the state; one naming another entity is told and the real state sent |
| api.ex.EntityVisibleToClient | export `EntityVisibleToClient` | Per-client culling. | done | games/shooters_edge/server/q2_native.cpp (`per_client`): at version 3, false leaves the entity out of that client's frame |
| api.ex.filesystem | file system API v1 (open, close, load, read, write, flush, tell, seek, read line, list, free list, error string) | Files for the library. | not-taken | Tail (decided 2026-11-23), hosted game libraries only: games/shooters_edge/server/q2_native_files.cpp: every call, the modes, exclusive, lists by extension or filter, the error texts; loose files only (no packs), no gzip or raw deflate |
| api.ex.debugdraw | debug draw API v1 | Lines, points, axes, boxes, spheres, circles, cylinders, arrows, text (debug builds with a renderer; never on a dedicated server). | not-taken | docs/porting/server.md: debug build only, not taken (and never on a dedicated server) |
| api.gmf.clientnum | feature bit 0: client number | The library fills each client's number; the server trusts it in frames. | not-taken | Our design: not offered in `sv_features`: the slot is the client number, as without the bit |
| api.gmf.properinuse | feature bit 1: proper in-use | The library keeps the entity in-use flag right, so the server can rely on it. | done | games/shooters_edge/server/q2_native.cpp (`take_view`): an entity not in use is not sent |
| api.gmf.mvdspec | feature bit 2: recorder-aware | The library knows the recorder's dummy spectator. | not-taken | Tail (decided 2026-11-23), hosted game libraries only: our own rules count as knowing the spectator (`sv_mvd_spawn_dummy` 1 makes it); the recorder records our rules only, so a hosted library's bit is not read |
| api.gmf.alldisconnects | feature bit 3: all disconnects | The disconnect call also for clients that never entered. | done | games/shooters_edge/server/q2_native.cpp (`nv_drop`): the disconnect call for a client the game said yes to that never entered |
| api.gmf.savegames | feature bit 10: enhanced savegames | Saves allowed only with this bit (or `sys_allow_unsafe_savegames`); `Game supports Q2PRO enhanced savegames.` | done | `saves_safe` (q2_native_save.cpp): without the bit (or `sys_allow_unsafe_savegames` 1) `save` and `load` are refused with `Game does not support enhanced savegames`, levels are not kept and no autosave is made; each new non-deathmatch game says which (our wording without the build's name) |
| api.gmf.variablefps | feature bit 11: variable frame rate | `sv_fps` above 10 used only with this bit. | not-taken | The variable frame rate is left out of the reference's default build, so the bit is never acted on there either; ours runs the hosted library at 10 Hz and does not offer the bit |
| api.gmf.extrauserinfo | feature bit 12: extra user info | The connect call gets a second info string: challenge, ip, major, minor, netchan, packetlen, qport, zlib. | not-taken | Tail (decided 2026-11-23), hosted game libraries only: games/shooters_edge/server/q2_native.cpp (`userinfo_text`): the second string in order; challenge, qport and the channel's numbers are fixed (0, 34/0, 1, 1390), as the hosted game sits behind our own server |
| api.gmf.ipv6 | feature bit 13: IPv6-aware | The `ip` key may carry an IPv6 address. | done | games/shooters_edge/server/q2_native.cpp (`ipv4_for_game`): without the bit an IPv6 client's `ip` reads 198.51.100.X:PORT |
| api.gmf.indexoverflow | feature bit 14: index overflow allowed | A full model, sound or image table returns 0 instead of stopping the level. | not-taken | Tail (decided 2026-11-23), hosted game libraries only: added (upstream); ours always returns 0, whatever the library asks (S14) |
| api.gmf.protoext | feature bit 15: protocol extensions | Turns on the extended limits (proto.extended-limits). | done | games/shooters_edge/server/q2_native.cpp: offered in `sv_features`, read as the library's start returns ("Game supports protocol extensions."), `protocol` 36; the edict size then must hold the extension block and `max_edicts` may reach 8192, else "Game library returned bad size of edict_t" / "Game library returned bad number of max_edicts" |
| api.save-files | `save/<name>/` with server, game and level files; `save/.current` the unit in play; `save/save0` the level-start autosave; `save1` the `autosave` word | The save layout and the unit's levels. | not-taken | Our design: same model (`q2_native_save.cpp`, `q2-gaps2-server.md` B3.4), but packed into our own save file, not these folders |
| api.frame-order | frame order | Timeouts, packets and moves, pings, budgets, `RunFrame`, sends, recorders, heartbeats, event clear, queued commands. | not-taken | Tail (decided 2026-11-23), hosted game libraries only: same order; pings (S7), queued commands (S11), budget for a library (S12) |

## 7. Messages a player sees (refusals, drops, prints), quoted exactly

`<x>` marks a filled-in value. Ours quoted where it differs.

done 63, missing 0, not-taken 9

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| msg.oob.protocol | `Unsupported protocol version <n>.` | Connect with an unknown protocol. | done | games/shooters_edge/net/q2_net_server.cpp (`q2_connect_ask`) |
| msg.oob.old-protocol | `You need Quake 2 version 3.19 or higher.` | Protocol 26 to 33. | done | games/shooters_edge/net/q2_net_server.cpp (`q2_connect_ask`) |
| msg.oob.bad-challenge | `Bad challenge.` | | done | `games/shooters_edge/net/q2_net_server.cpp` |
| msg.oob.no-challenge | `No challenge for address.` | | done | same |
| msg.oob.banned | `<comment>` or `Your IP address is banned from this server.`, then `Connection refused.` | Ban list refusal. | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours `Banned.` (the stock library's word) |
| msg.oob.locked | `Server is locked.` | | done | games/shooters_edge/net/q2_net_server.cpp |
| msg.oob.iplimit | `Too many connections from your IP address.` | | done | games/shooters_edge/net/q2_net_server.cpp, engine/server/admission.cpp |
| msg.oob.iplimit6 | `Too many connections from your IPv6 network.` | | done | games/shooters_edge/net/q2_net_server.cpp, engine/server/admission.cpp |
| msg.oob.maxmsglen | `Invalid maximum message length.` | Extended connect. | done | games/shooters_edge/net/q2_net_ext.cpp (`q2_connect_ask`) |
| msg.oob.netchan | `Invalid netchan type.` | Extended connect. | done | games/shooters_edge/net/q2_net_ext.cpp (`q2_connect_ask`) |
| msg.oob.extended-limits | `This is a protocol limit removing enhanced server.` then `Your client version is not compatible. Make sure you are running the latest Q2PRO client version.` | Old client on an extended-limits server. | done | games/shooters_edge/net/q2_net_ext.cpp (`q2_extended_refusal`): our words without the other program's name; the status `protocol` key 36 when extended. Held by tests/shooters_edge_q2_net_ext_test.cpp |
| msg.oob.empty-userinfo | `Empty userinfo string.` | | done | games/shooters_edge/net/q2_net_server.cpp |
| msg.oob.malformed-userinfo | `Malformed userinfo string.` | | done | games/shooters_edge/net/q2_net_server.cpp (`info_strict`, plugins/classic/protocol/info.hpp) |
| msg.oob.no-name | `Please set your name before connecting.` | | done | games/shooters_edge/net/q2_net_server.cpp |
| msg.oob.oversize-userinfo | `Oversize userinfo string.` | | done | games/shooters_edge/net/q2_net_server.cpp; held by `tests/shooters_edge_q2_server_sweep_test.cpp` (a raw connect past 512 bytes with the address) |
| msg.oob.password | `Please set your password before connecting.` / `Invalid password.` | Server password. | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours `Password required or incorrect.` then `Connection refused.` (the stock library's words) |
| msg.oob.spectator-password | (library) `Spectator password required or incorrect.` | Through the library's refusal. | done | `games/shooters_edge/net/q2_net_server.cpp` (stock library words) |
| msg.oob.userinfo-ban | `<comment>` or `Userinfo banned.`, then `Connection refused.` | | done | same |
| msg.oob.full-reserved | `Server and reserved slots are full.` | | done | same |
| msg.oob.full | `Server is full.` | | done | same |
| msg.oob.redirect | `Server is full. Redirecting you to <addr>...` (and `connect <addr>` run) | Redirect. | done | `redirect` in `games/shooters_edge/net/q2_net_server.cpp`; the trailing dots: verify |
| msg.oob.game-refusal | `<reason>` then `Connection refused.`, or `Connection refused.` | The library's refusal. | not-taken | Tail (decided 2026-11-23), hosted game libraries only: comes after the map loaded (S9) |
| msg.oob.bad-rcon | `Bad rcon_password.` | | done | `games/shooters_edge/net/q2_admin.cpp` |
| msg.oob.lrcon-refused | `This command is not permitted.` | | done | `engine/server/lrcon.hpp` |
| msg.oob.passive | `passive_connect` | | done | games/shooters_edge/net/q2_admin.cpp |
| msg.oob.shutdown | `shutdown` to masters | done | `plugins/classic/protocol/masters.cpp` |
| msg.drop.announce | `<name> was dropped: <reason>` to everyone (when the client was in the game) and to the client; `?` reasons read `<name> <reason>`; `!` not announced | The drop line. | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: ours announces only skew and idle drops, in other words; others reach only our log (S33) |
| msg.drop.kicked | `<name> was kicked` | To everyone and to the kicked client. | done | games/shooters_edge/net/q2_net_server.cpp (`q2_net_kick`) |
| msg.drop.timed-out | `<name> timed out` | | done | games/shooters_edge/net/q2_net_server.cpp: `<name> timed out`, the slot freed at once |
| msg.drop.disconnected | `<name> disconnected` | Leaving by `disconnect` (the library's line; the server's own is silent). | done | Our rules' line (`games/shooters_edge/server/q2_server.cpp`) |
| msg.drop.reconnected | `<name> was dropped: reconnected` | A connect from the slot's address. | done | games/shooters_edge/net/q2_net_server.cpp; a repeat before the first `new` is told it is connected again |
| msg.drop.time-skew | `<name> was dropped: time skew too high` | | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours `<name> was dropped for a fast clock.` |
| msg.drop.idling | `<name> was dropped: idling` | | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours `<name> was dropped for idling.` |
| msg.drop.nodelta | `<name> was dropped: too many nodelta frames` | | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: dropped; announced only in our log (S33) |
| msg.drop.sequence | `outgoing sequence too big` | | done | games/shooters_edge/net/q2_net_server.cpp |
| msg.drop.reset | `connection reset by peer` | Unreachable notices. | not-taken | `q2-server-gaps.md` row 17 |
| msg.drop.multiple-moves | `multiple clc_move commands in packet` | | done | games/shooters_edge/net/q2_net_server.cpp (`read_messages`) |
| msg.drop.too-many-frames | `too many frames in packet` | | done | `read_q2_batch` (games/shooters_edge/net/q2_net_ext.cpp): more extra frames than a packet holds drops the client with this reason |
| msg.drop.read-past-end | `read past end of message` | | done | games/shooters_edge/net/q2_net_server.cpp (`read_messages`) |
| msg.drop.too-many-usercmds | `too many usercmds in frame` | | not-taken | Cannot happen: a frame's move count is 5 bits (31 at most), so the reference never reaches 32 either |
| msg.drop.unknown-byte | `unknown command byte` | | done | games/shooters_edge/net/q2_net_server.cpp (`read_messages`) |
| msg.drop.empty-userinfo | `empty userinfo` | | done | games/shooters_edge/net/q2_net_server.cpp |
| msg.drop.malformed-userinfo | `malformed userinfo` | | done | games/shooters_edge/net/q2_net_server.cpp |
| msg.drop.malformed-name | `malformed name` | | done | games/shooters_edge/net/q2_net_server.cpp |
| msg.drop.oversize-userinfo | `oversize userinfo`, `oversize userinfo key`, `oversize userinfo value` | | done | Same meaning in our words (the player-text rule makes our words the intended ones). games/shooters_edge/net/q2_net_server.cpp: `oversize userinfo`; the key and value forms come only from protocol 36's delta |
| msg.drop.oversize-stringcmd | `oversize stringcmd` | | done | games/shooters_edge/net/q2_net_server.cpp |
| msg.drop.reliable | `reliable queue overflowed`, `oversize reliable message`, `reliable message overflowed` | | done | Same meaning in our words: a client overflowing its reliable channel is dropped. |
| msg.drop.version-probe | `failed version probe` (silent) | | done | games/shooters_edge/net/q2_net_server.cpp |
| msg.drop.reconnect-check | `failed to reconnect` (silent) | | done | games/shooters_edge/net/q2_net_server.cpp |
| msg.drop.cvarban | `<name> was kicked` | A cvar ban or a filtered command with the kick action. | done | games/shooters_edge/net/q2_net_server.cpp (`act`): `<name> was kicked` |
| msg.shutdown | `Server quit`, `Server quit: <reason>`, `Server restarted: <reason>`, `Server restarted`, `Server was killed.` | Printed to every client before the disconnect or reconnect at `quit`, `recycle`, a full restart, `killserver`. | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/net/q2_admin.cpp; the 1023-character cap kept |
| msg.name.empty | `You can't have an empty name.` | | done | games/shooters_edge/net/q2_net_server.cpp |
| msg.name.too-often | `You can't change your name too often.` | | done | `engine/server/names.hpp` |
| msg.name.changed | `<old> changed name to <new>` | With `sv_show_name_changes`. | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours ends with a full stop |
| msg.console-say | `console: <text>` | The operator's `say`. | done | `games/shooters_edge/server/q2_server.cpp` |
| msg.download.size-differs | `File size differs from server.` then `Please delete the corresponding .tmp file from your system.` | A resume past the file's size. | done | games/shooters_edge/net/q2_net_server.cpp: at the high level, then the refusal |
| msg.lag | `Lag stats for:       <name>`, `RTT (min/avg/max):   <a>/<b>/<c> ms`, `Server to client PL: <x>% (approx)`, `Client to server PL: <y>%`, `Timescale          : <t>` | `lag` answer. | done | games/shooters_edge/net/q2_net_server.cpp |
| msg.sinfo | the `sinfo` block: version, protocol (maj/min), maxmsglen, zlib support, netchan type, ping, movement fps, update rate, RTT, PL both ways, packetdup, timescale, connection time | `sinfo` answer. | done | games/shooters_edge/net/q2_net_server.cpp |
| msg.packetdup | `Packetdup of <n> is not allowed on this server.`, `Server is sending <n> duplicate packet(s) to you.` (`packet` for one) | | done | games/shooters_edge/net/q2_net_server.cpp |
| msg.slot255 | `WARNING: Server has allocated client slot number 255. This is known to be broken in your Q2PRO client version. Please update your client to latest version.` | An old protocol-36 client given no slot number. | not-taken | Cannot arise: slots are capped at 32 (S35), so no client is ever given slot 255 |
| msg.rate | (none) | The reference clamps a client's rate silently. | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours tells the client `Your rate setting is too high/low for this server.` and `Rate set to <n>` (`engine/server/rate.cpp`) |
| msg.mvd.prefix | relay lines start `[MVD] ` | | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp |
| msg.mvd.stream | `[MVD] Streaming resumed.`, `[MVD] Buffering data, please wait...`, `[MVD] Restored connection to the game server!`, `[MVD] Delay buffer overflowed!`, `[MVD] Disconnected from the game server!`, `[MVD] Lost connection to the game server!` | Relay state to viewers. | done | games/shooters_edge/net/q2_mvd_relay.cpp |
| msg.mvd.fwd | `[MVD] Not connected to the game server.`, `[MVD] Game server does not allow command forwarding.`, `[MVD] Send buffer not empty, please wait.`, `[MVD] This channel does not support command forwarding.` | `fwd` answers. | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp |
| msg.mvd.channel | `[MVD] You are already <where>.`, `[MVD] This channel is not compatible with your client version.`, `[MVD] You may not switch channels too soon.`, `[MVD] <name> left the channel`, `[MVD] <name> entered the channel`, `[MVD] Please enter a channel first.`, `[MVD] This channel is not ready yet.`, `[MVD] GTV server doesn't have this map!` | Channels. | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: games/shooters_edge/net/q2_mvd_relay_viewers.cpp: all but the version check and the missing map |
| msg.mvd.admin | `[MVD] Lost admin status.`, `[MVD] Invalid password.`, `[MVD] Granted admin status.`, `[MVD] You don't have admin status.` | | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp |
| msg.mvd.talk | `[MVD] Spectators may not talk on this server.`, `[MVD] Please turn off ignore mode first.`, `[MVD] You can't talk for <n> more seconds.`, `[MVD] You can't talk for <n> seconds.` | | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp |
| msg.mvd.chase | `[MVD] Chasing <name>.`, `[MVD] No players to chase.`, `[MVD] Player slot number <n> is invalid.`, `[MVD] Player slot number <n> is not active.`, `[MVD] No players matching '<x>' found.`, `[MVD] '<x>' matches multiple players.`, `[MVD] Previous chase target is not active.`, `[MVD] You have no previous chase target.`, `[MVD] Unknown chase target '<x>'. Valid targets are: ...`, `[MVD] Chasing players with '<x>' powerup.` | | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp |
| msg.mvd.autochase | `[MVD] Auto chasing is <ON/OFF> (<n> target(s)).`, `[MVD] Auto chasing <list>`, `[MVD] Auto chase list is empty.`, `[MVD] Please add auto chase targets first.`, `[MVD] Usage: ...` | | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp |
| msg.mvd.lists | the `channels` table and the `commands` help text | | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp |
| msg.mvd.names | `[MVD] <old> changed name to <new>`, `[MVD] <name> disconnected` | | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: games/shooters_edge/net/q2_mvd_relay_viewers.cpp: `[MVD] <old> changed name to <new>` and `[MVD] <name> disconnected` to the channel under `mvd_part_filter` and the `uf` mute; a kick or a time-out also reads `disconnected` |
| msg.ac | anticheat lines to players (lost service, modified file, unable to take connections, exempt, failed file checks, not using anticheat) | Anticheat lines. | not-taken | `q2-gaps2-server.md` B1 |

## 8. Messages an admin sees on the server console, quoted exactly

Developer-only prints (the reference's developer level) are left out; one row per message or per
tight group of one command's answers.

done 30, missing 0, not-taken 6

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| log.masters | `Only dedicated servers use masters.`, `Too many masters.`, `Couldn't resolve master: <addr>`, `Master server at <addr>.`, `There are no masters.`, the `num hostname lastmsg address` table | Master words. | done | games/shooters_edge/net/q2_game.cpp, plugins/classic/protocol/masters.cpp |
| log.slot-errors | `Bad client slot number: <n>`, `Client slot <n> is not active.`, `Userid '<id>' is not on the server.`, `No clients matching '<id>' found.`, `'<id>' matches multiple clients.` | Player lookup. | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours the stock words (`Bad client slot: <n>`, `Client <n> is not active`, `Userid <name> is not on the server`) |
| log.no-server | `No server running.` | Most operator words before a game starts. | done | Same meaning in our words: `sesv` always runs a game, so the case does not arise. |
| log.usage | `Usage: <cmd> <userid>`, `Usage: <cmd> <mapname>`, `Usage: <cmd> <raw text>`, `Usage: <cmd> <userid> <raw text>`, `Usage: <cmd> <userid> <variable> [...]`, `Usage: <cmd> <filename>`, `Usage: <cmd> <address>`, `Usage: <cmd> <directory>` | Usage lines. | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: ours for the words it has (`Usage: kick <userid>`); the rest missing with their commands |
| log.map | `Refusing to process oversize level string.`, `Using 'map' will cause full server restart. Use 'gamemap' for changing maps.`, `(You can set 'sv_allow_map' to 1 if you wish to permanently disable this warning. To force restart for a single invocation of this command, use 'map <mapname> force')`, `Couldn't load <path>: <error>` | `map` answers. | done | Same meaning in our words: a missing map is refused with our line. |
| log.dumpents | `No map loaded.`, `Dumped entity string to <path>` | | done | games/shooters_edge/net/q2_game.cpp |
| log.status | `Current map: <map>`, the status table, `No UDP clients.`, `Usage: status [d\|l\|p\|s\|t\|v]` | | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours `map : <map>` and its own header (cmd.status) |
| log.dumpuser | `userinfo` / `--------` then `miscinfo` / `--------` and its lines | | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: ours the user info block only |
| log.pickclient | `Single player server running.`, `Bad client address: <addr>`, `Please specify client port explicitly.` | | done | games/shooters_edge/net/q2_admin.cpp |
| log.sv | `No game loaded.` | | done | Same meaning in our words. |
| log.addr-lists | `Please specify a mask after '/'.`, `Bad address: <addr>`, `Bad mask: <n> bits`, `Usage: <cmd> <address[/mask]> [comment]`, `Entry <addr> already exists.`, `Usage: <cmd> <address[/mask]\|id\|all>`, `Address list is empty.`, `No such index: <n>`, `No such entry: <addr>`, the `id address/mask hits last hit comment` table | Bans and blackholes. | done | Same meaning in our words (`engine/server/bans.cpp`). |
| log.stuffcmds | `<list>cmd already exists: <text>`, `No <list>cmds registered.`, `No such <list>cmd index: <n>`, `No such <list>cmd string: <text>`, `Unknown stuffcmd list: <name>`, `Usage: <cmd> <list> <command>`, the `id command` table | | done | engine/server/operator.hpp (the words as read: `Stuffcmd already exists: <text>`, `No stuffcmds registered.`, `No such stuffcmd index: <n>`) |
| log.lrcon-list | `lrconcmd already exists: <text>`, `No lrconcmds registered.` and the rest of the shared list words | | done | Same meaning in our words (`engine/server/lrcon.hpp`). |
| log.filtercmds | `Usage: <cmd> <command> [ignore\|log\|print\|stuff\|kick] [comment]`, `Filtercmd already exists: <x>`, `No filtercmds registered.`, `No such filtercmd index: <n>`, `No such filtercmd string: <x>`, the `id command action comment` table | | done | Same meaning in our words (`engine/server/cmdfilter.cpp`). |
| log.cvarbans | `Usage: <cmd> <cvar> <match> [log\|print\|stuff\|kick] [comment]`, `<kind>ban already exists: <x>`, `No <kind>bans registered.`, `No such <kind>ban index: <n>`, `No such <kind>ban string: <x>`, `Removed <n> <kind>ban.` / `bans.`, `No cvarbans registered.`, the `id var match action comment` table | | done | Same meaning in our words (the player-text rule makes our words the intended ones). `engine/server/settingban.hpp` (facts in `q2-cvarbans.md`); ours says `No cvarbans.` and `Asked <n> players for their settings.` |
| log.cvarban-hit | `<name>[<addr>]: matched cvarban: "<var>" is "<value>"` | | done | `engine/server/settingban.hpp` (verify the exact line) |
| log.filter-hit | `<name>[<addr>]: issued banned command: <cmd>` | | done | `cmdfilter_log_line` |
| log.stuffcvar-answer | `<name>[<addr>]: "<var>" is "<value>"` | | done | games/shooters_edge/net/q2_net_server.cpp |
| log.version | `<name>[<addr>]: <version>` | Version probe answer. | done | games/shooters_edge/net/q2_net_server.cpp |
| log.game | `Game Error: <text>`, `<call>: bad index: <n>`, `<call>(<name>): overflow`, `<call>: volume = <x>`, `<call>: attenuation = <x>`, `<call>: timeofs = <x>`, `<call>: soundindex = <n>`, `<call>: edict = NULL`, `<call>: bad tag`, `<call>: untagged allocation not allowed`, `PF_setmodel: NULL`, `<call>: message buffer overflowed`, `<call>: bad to: <n>`, `<call>: NULL origin`, `PF_WriteFloat not implemented` | Library errors that stop the level. | done | Same meaning in our words (the player-text rule makes our words the intended ones). our host fails with its own words; index overflow and bad sound values are not errors (S14, S16) |
| log.gamelib | `Failed to load game library: <error>`, `Loaded game library from <path>`, `Game library path length exceeded`, `Can't access <path>: <error>`, `Failed to load game library`, `Game library returned NULL exports`, `Game library is version <n>, expected 3 or 3302`, `Game library corrupted maxclients value`, `Game library returned bad size of edict_t`, `Game library returned bad number of max_edicts` | Loading the library. | done | Same meaning in our words (the player-text rule makes our words the intended ones). `games/shooters_edge/server/q2_native.cpp` has its own words (version mismatch named) |
| log.extensions | `Game supports Q2PRO protocol extensions.` (and at developer level `Game supports Q2PRO extended API version <n>.`); `Game attempted to set extended flags on '<name>', masked out.` | The library's extensions at load. | done | Same meaning in our words (the player-text rule makes our words the intended ones). games/shooters_edge/server/q2_native_calls.cpp: `Game attempted to set extended flags on '<name>', masked out.`; the protocol extensions are not offered, so their line is never printed |
| log.spawn | `------- Server Initialization -------`, `SpawnServer: <map>`, `-------------------------------------`, `Too many inline models`, `Couldn't load <path>: <error>`, `Deathmatch and Coop both set, disabling Coop` | Map start. | done | Same meaning in our words: our rules print their own map-start lines. |
| log.override | `Loaded entity string from <path>`, `Couldn't load entity string from <path>: <error>`, `Loaded <path>`, `Couldn't load <path>: <error>` | Entity files and binary override files at map load. | done | Same meaning in our words (the player-text rule makes our words the intended ones). added (upstream); `sv_entpath` loads entity files with its own log words |
| log.rate-limit | `Period too large: <n>`, `Limit too large: <n>`, `Burst too large: <n>` | Rate-limit syntax errors. | done | Same meaning in our words (the player-text rule makes our words the intended ones). `engine/server/limit.hpp` refuses with its own words |
| log.rcon | `Rcon from <addr>:` / `<line>`, `Invalid rcon from <addr>:` / `<line>`, `Limited rcon from <addr>:`, `Invalid limited rcon from <addr>:` | | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours `rcon from <addr>: <line>`, `bad rcon from <addr>`, `limited rcon from ...` (`games/shooters_edge/net/q2_admin.cpp`) |
| log.skew | `<name>[<addr>]: detected time skew: <x.xxx>` | `sv_timescale_warn`. | done | Same meaning in our words (`engine/server/skew.hpp`).; held by `tests/shooters_edge_q2_server_sweep_test.cpp` (a client whose clock runs fast warned, then dropped) |
| log.maxmsglen | `Fixing up maxmsglen for <name>: <a> --> <b>` | | not-taken | porting/shooters-edge/q2-proto36-server.md: printed only by path-MTU discovery, which the reference build never enables |
| log.overflow | `Game left <n> bytes in multicast buffer, cleared.`, `Frame overflowed for <name>`, `Dumping datagram for <name>`, `<msg> to <name>: overflow on the first message`, `Error <n> compressing <n> bytes message for <name>` | | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours drops the unreliable part (S23) with its own log words |
| log.name | `<old>[<addr>] changed name to <new>` | Every rename on the dedicated console. | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours logs renames with its own words: verify; held by `tests/shooters_edge_q2_server_sweep_test.cpp`: `<old> [<addr>] renamed <new>` on the console, told to the players with `sv_shownames` 2 |
| log.drop | `<name>[<addr>] was dropped: <reason>` (or `<name>[<addr>] <reason>`) | Every drop, on a dedicated server. | done | Same meaning in our words (the player-text rule makes our words the intended ones). ours `<name> dropped: <reason>` (`games/shooters_edge/net/q2_net_server.cpp`) |
| log.mvd | `Auto-recording local MVD to <path>`, `Not recording a local MVD.`, `Stopped local MVD auto-recording.`, `No slot for dummy MVD client`, `Dummy MVD client rejected by game: <reason>`, `Stopping MVD recording, maximum size reached.`, `... maximum duration reached.`, `... maximum number of level changes reached.`, `Couldn't write local MVD: <error>`, `MVD recording is disabled on this server.`, `Already recording a local MVD.`, `Begin local MVD recording.`, `Missing filename argument.`, `Recording local MVD to <path>`, `Stopped local MVD recording.`, `Can't 'mvdstuff', dummy MVD client is not active`, `Unreliable MVD datagram overflowed.`, `Dumping unreliable MVD datagram.` | Recorder. | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: games/shooters_edge/net/q2_mvd_server.cpp, games/shooters_edge/net/q2_mvd_dummy.cpp: the recording's lines, the fatal errors, `No slot for dummy MVD client`, the auto-recording lines; the unreliable datagram's two lines never arise (blocks are written whole) |
| log.mvd-tcp | `Accepted MVD client <name>[<addr>]`, `TCP client <name>[<addr>] dropped: <reason>`, `TCP client [<addr>] rejected: too many connections`, `TCP client [<addr>] rejected: no free slots`, `Error opening server TCP port.`, `Server TCP port already in use.`, `Fatal MVD server error: <x>`, the TCP clients' table (`ZMBI`, `ASGN`, `CNCT`, `PRIM`, `SEND`) and versions table, `No TCP clients.` | Viewers over TCP. | done | games/shooters_edge/net/q2_mvd_server.cpp: accepted, dropped with each reason, rejected (too many connections, no free slots), the port error |
| log.save | `Savegame cvar name too long`, `Savegame cvar value too long`, `Game does not support enhanced savegames`, `Savegame path too long`, `Bad savegame configstring index`, `Savegame configstring too long`, `Couldn't write level file.`, `Couldn't write server file.`, `Couldn't wipe '<dir>' directory.`, `Couldn't write '<dir>' directory.`, `Couldn't read level file.`, `Game supports Q2PRO enhanced savegames.`, `Game supports YQ2 enhanced savegames.`, `Use of unsafe savegames forced from command line.`, `Game does not support enhanced savegames. Savegames will not work.`, `Usage: <cmd> <directory>`, `Bad savedir.`, `No such savegame: <name>`, `Couldn't read '<dir>' directory.`, `Couldn't read server file.`, `You must be in a game to save.`, `Can't savegame in a deathmatch.`, `Can't savegame while dead!`, `Game saved.` | Saves. | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: saves exist (`games/shooters_edge/server/q2_save.hpp`, `q2_native_save.cpp`) with our own words; no dedicated `save`/`load` (S31) |
| log.world | `Bad mins/maxs on entity <n>: <mins> <maxs>` (developer), `SV_AreaEdicts: MAXCOUNT`, `<call>: inline model <n> out of range`, `<call>: NULL` | World errors. | done | Same meaning in our words (the player-text rule makes our words the intended ones). our host's own words |
| log.ac | anticheat console lines (violations, kicks, the service's state) | | not-taken | `q2-gaps2-server.md` B1 |

## 9. Server features beyond single words: downloads, rates, floods, bans, filters, recording, status

done 45, missing 0, not-taken 11

| id | thing | what it is / does (one line, our words) | status | ours / note |
| --- | --- | --- | --- | --- |
| feat.dl.http | HTTP downloads | `dlserver=<url>` in the connect answer sends clients to a web copy. | done | games/shooters_edge/net/q2_net_server.cpp: the connect answer names the web copy (`sv_downloadserver`) |
| feat.dl.pieces | piece size and pace | Pieces sized to the packet's room, several a frame within the rate (extended); 1024 a frame stock. | done | classic/protocol/download_pace.hpp (`fill_room`) from games/shooters_edge/net/q2_net_download.cpp: the room less 4, one piece owed at a time, the pause of its bytes over the rate; facts porting/shooters-edge/q2-downloads-pace.md (the earlier note's "several a frame" corrected: one a `nextdl`) |
| feat.dl.no-frames | no frames while downloading | Game frames are not sent to a downloading client. | done | games/shooters_edge/net/q2_net_server.cpp (`send_frames`): reliable data alone while a file comes down |
| feat.dl.compressed | compressed downloads | Deflate pieces straight from compressed packs (36 minor 1021 and up, zlib, from offset 0). | done | same (games/shooters_edge/net/q2_net_server.cpp `start_download`); tests/shooters_edge_q2_net36_test.cpp |
| feat.dl.resume | resume by offset | A download restarts at the client's offset. | done | games/shooters_edge/net/q2_net_server.cpp: from the client's offset; past the end told the size differs and refused |
| feat.dl.kinds | what may be downloaded | A path with a folder, no `..`, by folder: players, models and sprites, sounds, maps, textures and env, pics, others; empty files and files past the size limit refused. | done | games/shooters_edge/net/q2_net_server.cpp: a folder, no climbing out, a plain first and last letter; by folder players, models and sprites, sound, maps, textures and env, pics, others; empty files and files past the size limit refused |
| feat.dl.refused | refused download | The empty download answer (size -1). | done | `start_download` sends the refusal piece |
| feat.rate.clamp | rate clamped to the bounds | `rate` held to `sv_min_rate`..`sv_max_rate`; LAN unlimited by setting; loopback unlimited. | done | `engine/server/rate.hpp`; loopback: verify |
| feat.rate.no-key | no `rate` key: 5000, not clamped | | done | 5000, held to no limit (`RateBounds::none_held`); `server_rate_test` |
| feat.rate.hold | frames held over the rate | | done | `games/shooters_edge/net/q2_net_server.cpp` |
| feat.flood.players | players' chat flood | The library's (stock: `flood_msgs`, `flood_persecond`, `flood_waitdelay`). | done | Our rules (`games/shooters_edge/server/q2_commands.cpp`); a hosted library its own |
| feat.flood.relay | relay spectators' chat flood | `[MVD] You can't talk for <n> seconds.` | done | games/shooters_edge/net/q2_mvd_relay_viewers.cpp: the 16-line ring, the wait, the lines |
| feat.ban.list | address list with masks, comments, hits, last hit | Refuses connects. | not-taken | Our design: the engine ban list (patterns, timed entries, `sv_banmode`); no hit-count or comment columns. |
| feat.ban.blackhole | blackholes | Every packet ignored. | done | Engine |
| feat.ban.drop-on-add | a ban that matches a connected client drops it | | done | `drop_refused` (`games/shooters_edge/net/q2_admin.cpp`; the stock library's `addip` behaviour) |
| feat.filter.cmd | client command filter with actions | ignore, log, print, stuff, kick. | done | `engine/server/cmdfilter.hpp` |
| feat.filter.match | setting-ban match forms | `!` negates, `*` any non-empty, `=` `<` `>` numbers, `~` contains, `#` literal, else exact; text without case. | done | `engine/server/settingban.hpp` (`q2-cvarbans.md`) |
| feat.filter.userinfo | user info bans checked at connect, at `new` and on change | | done | `judge_userinfo` |
| feat.stuff.connect | text run at connect and at begin (`addstuffcmd`) | | done | games/shooters_edge/net/q2_net_server.cpp: the connect list after the first `new`'s probe, the begin list at each `begin` |
| feat.lrcon | limited rcon by prefixes, not macro-expanded | | done | `engine/server/lrcon.hpp` |
| feat.rcon.split | rcon answers cut into packets | | done | games/shooters_edge/net/q2_admin.cpp: packets of at most 1384 bytes after `print\n`, whole lines where they fit; an empty answer still sends one |
| feat.rcon.reach | rcon runs any console command | | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: S29 |
| feat.slots.reserved | reserved slots and password; server password counts | | done | `engine/server/admission.hpp` |
| feat.slots.redirect | redirect when full, or always with `!` first (everyone but reserved-password holders) | | done | same |
| feat.slots.iplimit | per address or IPv6 /48; a zombie slot counts half; IPv6 LAN addresses exempt | | done | Checked 2026-11-23: the engine's `admit_refusal` limits slots per address (`engine/server/admission.cpp`). |
| feat.slots.zombie | zombie slot resends the disconnect | | done | `games/shooters_edge/net/q2_net_server.cpp` |
| feat.slots.ghost | silent challenged slots freed after `sv_ghostime` | | done | `sv_ghosttime` |
| feat.slots.count | up to 256 slots | | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: 32 (S35) |
| feat.dedicated-dm | a dedicated server is deathmatch unless `coop` is set; neither set means one player | | done | Checked 2026-11-23: our rules default to deathmatch on the dedicated server. |
| feat.idle | idle kick on no buttons, move or chat | | done | `engine/server/idle.hpp` (setting name in cvar.sv_idlekick) |
| feat.skew | time-skew warn and kick over a window | | done | `engine/server/skew.hpp`; held by `tests/shooters_edge_q2_server_sweep_test.cpp` |
| feat.enforcetime | move budget 1800 ms per 16 frames, silently ignored past it | | not-taken | Tail (decided 2026-11-23), hosted game libraries only: our rules only (S12) |
| feat.nodelta | full-frame flood drop | | done | `sv_nodelta` |
| feat.ping | ping by method from the last 16 frames | | not-taken | Tail (decided 2026-11-23), hosted game libraries only: measured; not given to a hosted library (S7) |
| feat.status.keys | server info keys: `protocol`, `deathmatch`, `cheats`, `dmflags`, `fraglimit`, `timelimit`, `maxclients`, `hostname`, `version`, `game`, `mapname`, `port`, `uptime`, admins' own keys, the library's own (stock: `gamename`, `needpass`) | | done | games/shooters_edge/net/q2_admin.cpp (`q2_status_keys`): protocol, deathmatch, coop, cheats, dmflags, fraglimit, timelimit, maxclients, maxspectators, hostname, version, gamename, gamedir, game, mapname, port, uptime, needpass and every `sets` key |
| feat.status.reserved | `maxclients` shown minus reserved slots (status, heartbeat and `info`) | | done | games/shooters_edge/net/q2_net_server.cpp (`q2_public_slots`): `info` and status answers and heartbeats |
| feat.status.uptime | `uptime` key short or long form | | done | `engine/server/status.hpp` |
| feat.recording.mvd2 | multi-view recording file (`MVD2`, length-led blocks, ends with 0; gzip) | | done | games/shooters_edge/net/q2_mvd_server.cpp: `mvdrecord`, gzip with `-z`, the limits; frames from games/shooters_edge/server/q2_mvd2_live.hpp |
| feat.recording.dummy | the recorder's dummy client `[MVDSPEC]` holding the scoreboard (begin and score commands) | | done | games/shooters_edge/net/q2_mvd_dummy.cpp: `[MVDSPEC]` in a slot past the seats, a spectator to the rules, its commands (`wait`, `cmd`, `set`, `alias`, settings, the ignored words), the scoreboard kept up, out when the recorder stops; tests/shooters_edge_q2_mvd_server_test.cpp |
| feat.recording.limits | stop by size, time, map count | | done | games/shooters_edge/net/q2_mvd_server.cpp: `sv_mvd_maxsize`, `sv_mvd_maxtime`, `sv_mvd_maxmaps`, each with its line; size and length held by `tests/shooters_edge_q2_mvd_server_test.cpp` |
| feat.recording.capture | which players are recorded (in game, invisible, visible) | | done | games/shooters_edge/server/q2_mvd2_live.cpp: the capture test in its order; layouts only the spectator's |
| feat.relay.tcp | viewers over TCP on the game port (`MVD2` magic, hello, deflate; access by allow list, ban list, password) | | done | games/shooters_edge/net/q2_mvd_server.cpp: the magic, hello, deflate as one zlib stream, the allow list, the ban list, the password, stream start and stop, ping |
| feat.relay.channels | the relay's channels, waiting room, chase cameras, menus, scoreboard, `[MVD]` chat | | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: games/shooters_edge/net/q2_mvd_relay.hpp: channels, the Waiting Room and its chooser, a view a viewer, the menus, chat, admin; no free flight, no stats rewrites |
| feat.relay.demo | relay plays recorded files in a loop | | done | games/shooters_edge/net/q2_mvd_relay.hpp: `mvdplay` loops `.mvd2` files (`-l 0`) |
| feat.serverdemo | the server's own recording | | not-taken | Our design: engine `serverrecord` (`.ged`) and `mvdrecord` (`.mvd2`, games/shooters_edge/net/q2_mvd_server.cpp); no `.dm2` (S40) |
| feat.cinematics | cinematic and picture levels (`.cin`, `.pcx`, `.dm2`) and `nextserver` | | done | games/shooters_edge/net/q2_game.cpp, games/shooters_edge/server/q2_wire.cpp: a level of its own, server data with no slot and the file as the level (state 5 or 3 for 36), no configstrings, no `begin` |
| feat.map.chain | `+` chained level after a cinematic; `*` end of unit; `$spot`; `!` stop if missing | | not-taken | Tail (decided 2026-11-23), debug, developer or admin extra: unit ends and spawn spots: our rules (`games/shooters_edge/server/q2_campaign.hpp`); the shows before a map and `+` after them (`games/shooters_edge/net/q2_game.cpp`, a level each); `map`'s and `gamemap`'s level string read the same way; `!` is taken off, so a missing map leaves the level as it is instead of stopping the server |
| feat.map.entfile | entity file `<map>@<crc>.ent` (four hex digits of the map's entity text checksum) then `<map>.ent` in `map_override_path` | | done | `sv_entpath` |
| feat.map.override | `<map>.bsp.override` (name, checksum, entities) | | not-taken | `q2-server-gaps.md` row 19 |
| feat.map.vispatch | three stock maps' visibility repaired | | done | `sv_vispatch` |
| feat.save.dedicated | saves on a dedicated server | | done | games/shooters_edge/net/q2_game.cpp: save, autosave and load on the dedicated server, the level-start autosave kept as `save0` |
| feat.save.autosave | level-start autosave into `save0` at each level change in a unit (not in deathmatch) | | not-taken | Our design: ours autosaves as a level is entered (`q2_native_save.cpp`, our rules too) in our own save format; the dedicated server keeps it as `saves/save0.save` (games/shooters_edge/net/q2_game.cpp, `keep_autosave`) |
| feat.spectator-fly | extended clients' spectators fly faster, with the fly fix | | done | plugins/classic/q2_move (`fly_time`, `fly_friction`, `fly_glide`, `q2_fly_for_protocol`): a stock 35 or 36 client's spectator flies at twice the time, a 36 client's with friction 4 gliding on; a 34 client's keeps the original step; ours and bots fly as before. Our rules by `ClientInfo::protocol`, a hosted library by the thinking slot (`NativeHost::thinking`). Facts: porting/shooters-edge/q2-server-batch13-facts.md |
| feat.ext-behaviours | what an extended server does beyond the wire | With the extensions: the map's extended contents in traces, a box starting all-solid stopped at once, players and projectiles clipped only by masks asking for them, entities flagged no-cull always sent, looping sounds culled by distance, the low-priority render flag dropped first when a frame overflows, a client's no-flares setting, the newer footstep and ladder events. | done | plugins/brush/trace.cpp (`set_extended`: each leaf's contents with its brushes', a sweep that stays inside a brush stopped at 0 with its contents), server/q2_native_calls.cpp (players and projectiles flagged by the game clipped only by a mask asking for them), server/q2_wire.cpp (`Q2Sight::extended`: a looping sound by the hearing set and its reach; no-cull linked everywhere), server/q2_wire_ext.cpp (no flares, the newer steps, the rocket's gib trail kept). Not taken: the low-priority render flag (our hosted frames are uncapped, `cvar.sv_prioritize_entities` not taken). Facts: q2-server-batch13-facts.md § B 10 |
| feat.recycle | quit telling clients to reconnect | | not-taken | `q2-server-gaps.md` row 15 |
| feat.master | classic heartbeat (status text) every 300 s, first 5 s after a game starts; `shutdown` on quit | done | `plugins/classic/protocol/masters.cpp` (only while public and dedicated) |
