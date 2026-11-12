# The classic proxy and its compressed demo format (working note)

WORKING AREA: deleted when porting ends. This note holds facts about the classic game's
closed-source proxy (Qizmo, 1997–1999, by Juha Kujala and Ilkka Rajala of UDP Soft) and its
compressed client-demo format (`.qwz`), gathered only from public documentation (its own README,
wiki pages, forum posts, client manuals) and from black-box byte statistics of real demo files.
No program was downloaded, run, decompiled or disassembled, and no source code (leaked or
reconstructed) was read. It is input for the engine's own proxy, forwarder and broadcast relay
(generic in `engine/`, game-specific where it must be). Each fact carries its source: a URL, or
**observed** (a byte statistic of a file listed in the demo corpus,
`demo-corpus/INDEX.md` in the project's shared files, kept out of the repository).

Sources, cited below by tag:

| Tag | Source |
| --- | --- |
| [R291] | The program's own README.TXT, version 2.91 (41,830 bytes, dated Nov 1999), read out of `https://www.quaddicted.com/files/tools/qizmo291.zip` by fetching only that member's bytes (HTTP range requests on the zip's central directory and the README entry; the executable member was never fetched) |
| [Rold] | An earlier README (≈2.5x era, 39.5 KB), copied to `https://docs.google.com/document/d/1GITmaQoa_ze0b6HbQvnW_wrnclnztcAbv--5n1uBb2w` (linked from [Wiki]) |
| [Zip] | The same zip's central directory: member names and sizes (QIZMO.EXE 273,420 B; README.TXT 41,830; COMPRESS.DAT 262,144 B, deflating to 50,603; SOURCES.TXT; PASSWORD.CFG; MENU.CFG; REGISTER.TXT; FILTERS.TXT; PS2RATE.VXD; HOSTS.CFG), and the text configs MENU.CFG, HOSTS.CFG, SOURCES.TXT, FILTERS.TXT read the same way |
| [Wiki] | `https://www.quakeworld.nu/wiki/Qizmo` |
| [Lag] | `https://www.quakeworld.nu/blog/382/how-to-use-qizmo-to-fix` (a player's 2015 guide, with comments) |
| [TF] | `https://www.quaddicted.com/files/webarchive/www.planetfortress.com/tfdone_easy/tf/demos/` (a 1999-era demo site's help) |
| [F4141] | `https://www.quakeworld.nu/forum/topic/4141/66153/no-qizmo-download-link-anywhere/` |
| [F5034] | `https://www.quakeworld.nu/forum/topic/5034/convert-qwz-to-qwd` |
| [ESR] | `https://www.esreality.com/?a=post&id=354665` (2003 forum thread) |
| [EzDemo] | `https://ezquake.com/docs/settings/demos.html` |
| [QWDt] | `https://www.quakeworld.nu/wiki/QWDtools`, the tool's changelog `https://raw.githubusercontent.com/ParadokS81/qw/HEAD/QuakeWorld/qw/qwdtools/changes.txt`, and its usage text quoted in `https://www.quakeworld.nu/forum/topic/6479/converting-qwd-to-mvd` |
| [fQ] | `https://fquake.sourceforge.net/getstarted.html` § Using Qizmo |
| [QTV] | `https://www.quakeworld.nu/wiki/QTV` (the QuakeTV page, with a Qizmo-versus-QTV table) |
| [Fwd] | `https://www.quakeworld.nu/wiki/QWfwd`; [Cat] `https://www.quakeworld.nu/wiki/Category:Proxies`; [QWD] `https://www.quakeworld.nu/wiki/QWD`; [MVD] `https://www.quakeworld.nu/wiki/MVD` |

## (a) What the proxy did, as a player and an operator saw it

### What it is

- A proxy that sits on the packet path between a client and a server; based on an earlier proxy
  (FAQ-Proxy 1.02) and backward compatible with it; its query commands keep that proxy's `f_`
  prefix. Headline features: the in-game menu, a server browser and pinger, voice channels, data
  compression, "FPS boosting", and many small helpers. Supported client/server versions 2.0–2.33.
  [R291]
- Platforms: Windows, Linux (libc5 and glibc), Solaris, BSD, Alpha Linux, OSF/1 (the OS letter
  `f_version` appends: W, L, G, S, B, A, O). [R291]
- Shareware. Old versions: unregistered use was disconnected after 50 minutes. 2.91: the
  registration code "unlocks all the features on any Qizmo you connect to"; one person per code
  at a time, a second simultaneous user kicks the first. [Rold] [R291]
- The README does not say "multiplayer only", but every feature is for the online client game.
- Still in use for three things in 2015–2024, per the community: the nail filter on a
  server-side proxy, link compression, and sending each packet several times. [Wiki] [Lag]

### Starting it and connecting (the player's view)

- Command line: `qizmo [options] [path to client] [client options]`; given the client's path it
  starts the client itself (needed for `f_modified`). [R291]
- The player runs the client, types `connect localhost` (or the proxy's address), and is now
  "on" the proxy: the client sees the proxy as a server and the proxy shows its menu as
  centerprints. Then `.connect <server>` (or `say .connect …`) makes the proxy connect onward.
  [R291] [Wiki]
- Later clients learned the proxy: with `cl_useproxy 1` the client's plain `connect` and its
  server browser route through the proxy automatically. [Wiki] [Lag] [fQ]
- Command addressing in a chain. Proxy commands are typed as `say` text the proxy intercepts:
  `say .cmd args` goes to the first proxy that sees it; `say <proxyname>:cmd args` to the named
  proxy (name from `-n`, default the host name, shown top right in the menu); `say ,cmd args` to
  the last proxy in the chain (nearest the server); `proxy:` addresses any. Later clients accept
  `.cmd` and `,cmd` typed directly. [R291] [Lag]
- Chaining: `client → proxy1 → proxy2 → … → server`. Uses: re-routing for a better (or, to
  be fair to a high-ping opponent, a worse) ping; a local proxy for client-side features plus a
  remote one for link features. The menu has "previous proxy / next proxy" to move between the
  menus of each proxy in the chain. "From:" picks which proxy connects onward ("last" = use the
  server list of the local proxy but connect from a remote one). [R291] [fQ]
- TCP tunnel between two proxies for networks that block UDP:
  `client ↔ proxy1 ↔ firewall ↔ proxy2 ↔ server`; proxy2 listens with `-x port`, the player issues
  "TCP connect proxy2:port" on proxy1. Outward TCP needs no flag. [R291]

### Operator: command-line options (2.91) [R291]

| Option | Effect |
| --- | --- |
| `-a password` | admin password (saved to `password.cfg`; an old proxy's file is refused) |
| `-b dir` | game directory (for sounds, `.loc` files, model checks); default the current dir |
| `-c ip` / `-s ip` | bind client-side / server-side connections to one interface |
| `-d flags` | rights a client has before entering a password (letters `acromsptvx`, see the access menu) |
| `-e dir` | where demos are recorded; default `<game dir>/qw` |
| `-h`, `-?` | list options |
| `-i 1..31` | process priority (Windows) |
| `-l file` | log the proxy console to a file |
| `-m n` | maximum clients, default 32 |
| `-n name` | proxy name for `name:cmd` and the menu bar |
| `-p port` | UDP port, default 27500 |
| `-q` / `-v` | quiet / verbose console |
| `-r masters` | report this proxy to the given master servers (the public-proxy list) |
| `-x port` | accept TCP tunnel connections on this port |
| `-o` (mentioned under `f_skins`) | allow the client's `allskins`/`baseskin` commands, disabling `f_skins` |
| `-C file.qwd` / `-D file.qwz` | compress a demo / decompress one (undocumented in the README; documented by [TF]; `-D` confirmed by [F5034]) |

Files beside the program: `password.cfg` (passwords and address rules), `qizmo.cfg` (admin
settings, written on first run: e.g. `maxclients`, `browser_a`, `name`, `hostname`, `public`,
`quakedir`, `maxs2crepeat`), `sources.txt` (master servers: `master "<name>" <ip:port>
<protection 0..3>`, IP only; 0 none, 1 protect delete, 2 protect delete/add, 3 protect
delete/add/read), `filters.txt` (server-list filters), `hosts.cfg` (`allow`/`deny a.b.c.d` with
`*`; the most specific entry wins), `menu.cfg` (key binds), `cfg/<player>.cfg` (per-player saved
settings), `<map>.loc` (location marks), `compress.dat` (262,144 bytes; see (b)). [R291] [Zip]
[Wiki]

Linux server-side use as documented in 2020: `./qizmo -p 28000 -r -a passwd`; it needs 32-bit
libraries on a modern system, and stale System V message queues can stop it starting (clear them
with `ipcrm`). [Wiki]

### Access rights [R291]

- Features shown red are allowed, white are not. "Input password" unlocks; admin edits the
  authorisation data. Two kinds of key: passwords and addresses (`ip:port`, wildcards for subnets,
  most specific match wins). The Windows package's `password.cfg` auto-admins
  `localhost:27001` (the classic client's fixed local port). Rights letters `acromsptvx`.
- Example policy: protect "record a demo on the proxy's disk" from strangers.
- "Dynamic IP mask" (255 = must match, 0 = may change) lets a client's address change and keep
  its session.

### The in-game menu [R291] [Zip MENU.CFG] [Wiki]

- Drawn with centerprints in the client. Keys are client binds that send `say proxy:menu up`,
  `down`, `left`, `right`, `select`, `delete`, `home`, `end`, `pgup`, `pgdn`, `back`, `help`, and
  `say proxy:menu` (toggle). `proxy:menu bindstd` (later `.menu bind std`, `.menu binds`) binds
  them to the arrows, Enter, Del, Home, End, PgUp, PgDn, Backspace, Ins and Pause.
- Left/right switch sub-menus; Enter descends or acts; Backspace goes up; Delete removes an item.
- Limits it works around: the classic client caps a centerprint at 40 columns and 1024
  characters and crashes on longer ones ("Video mode 320x200/320x240/400x300" and "Max
  centerprint lines" exist because of this).
- Menus named in the README: Ctrl (fast connect, disconnect, reconnect, mode
  spectator/player, From, TCP connect), Fltr, List, Sort, Ping, Source, Setup (server browser),
  demo recording, demo player, client list, voice channels, player list, config manager, sound
  system, data compression, access rights, teamplay help (with Loc, Way, Point, Pick, Names, Need,
  Filter), observer mode, FPS settings, misc (weapon impulses, video mode, HUD clock,
  floodprotprot, CR→LF, fun, f_modified, drop fov changes, MP3), lag settings, admin setup.

### Server browser [R291] [Zip FILTERS.TXT]

- Pings first with small packets, then fetches server info, so pings are not inflated: about 100
  servers a second on a modem, 200 on ISDN, within 10 ms. Settings: ping timeout, pings per
  server (averaged), pings per second, serverinfo timeout and retries, simultaneous refreshes,
  master timeout and retries, how long to keep dead servers and server infos.
- Sources: master servers or favourite files; each server lists which sources it belongs to.
- Filters: a tiny language, one line = `line# [if data1 op data2] <goto line# | keep | remove>`;
  data are `gamedir, ping, players, maxplayers, map, ip` or any serverinfo key, or a quoted string
  with `?`/`*` wildcards; ops `= != ~= ~!= < <= > >=` (`~` = case-insensitive); goto only jumps
  forward; falling off the end = passed. Shipped filters: Deathmatch, CTF, TF, Arena, not empty,
  empty, not full, iD maps, FFA, 1on1, 2on2, team competition, ping < 200.

### Packet loss, lag and the link

- **Send each packet N times** (`repeatc2s 1..3` on the local proxy, `repeats2c 1..5` on the
  server-side proxy; admin cap "Maximum s2c repeats", `maxs2crepeat` in `qizmo.cfg`). The classic
  client and server already discard duplicate packets, so client→server repeats work without a
  second proxy; repeats toward the client need a proxy on the server side because the server does
  not answer each copy. A player reports 10% loss cut to 1–2% with 2× up and 3× down. [R291]
  [Lag] [Wiki]
- **Showdrop**: `.showdrop` / `,showdrop` print a message for each packet lost *between two
  proxies*. [R291]
- **Lag settings** (to play fair against a high-ping opponent): constant added lag; a bandwidth
  limit that delays big packets more (e.g. 28,800 to mimic a modem); random packet loss %. The
  README tells the player to set the client's `rate` to match. Servers can query these
  (`f_lagreport`), force reporting of changes, or forbid them (`fpd` bits 8 and 16). [R291]
- **Process priority control** of a local server on Windows (give the server CPU time right after
  a packet is sent to it, to remove local loss). [R291]
- **Autodrop rate**: lower the client's upstream rate while talking on voice. [R291]

### Compression of the link [R291] [Lag]

- Only **between two proxies**: `client → proxy1 ⇄ proxy2 → server`. The client↔proxy hop and the
  proxy↔server hop stay the plain game protocol. A modem player runs proxy1 at home and proxy2 at
  the ISP, "and your modem line has to deal with 80% less data".
- Switched on **at the sending side, per direction**: server→client compression is turned on at
  proxy2 (`packs2c`), client→server at proxy1 (`packc2s`). A per-player config can do it on
  connect (`stuff say proxy:packc2s 1` in the server-side proxy's extra commands). A later
  client-side convention defines aliases `ezcomp` (both directions), `ezcomp2` (with quality-line
  mode), and a third with lossy mode too. [R291] [Lag]
- Ratios claimed: 20–30% saved server→client and 30–50% client→server [R291]; one player measured
  64% (c2s) and 80% (s2c) saved with both directions on [Lag].
- **Quality line mode**: compresses much better but multiplies the effect of any loss *between the
  two proxies* (loss elsewhere is unaffected). Read: the compressor's state spans packets, so one
  lost packet damages the ones after it. [R291] [Lag]
- **Lossy compression**: drops "some unnecessary bits"; players saw nails flying sideways, i.e.
  angles quantised more coarsely. [R291] [Lag comments]
- How it is asked for on the wire is not documented anywhere found (no handshake described).

### FPS and bandwidth filters (`fps` takes ten numbers) [R291] [Lag]

Explosions (normal, tarbaby, teleport, blood, off), own powerup glow on/off, muzzle flashes
(on, own off, all off), gib filter (removes gibs and corpses; saves bandwidth), rockets drawn as
grenades, userinfo filter (drops `noaim` and `rate` setinfo spam), **nail filter** (off, 1/2,
2/3, 3/4 of nails removed: the 7th of the ten numbers, e.g. `,fps 0 0 0 0 0 0 2 0 0 0` for half;
done on the server-side proxy it saves bandwidth too, since nails compress badly), damage flash
filter, ambient sounds, pickup flashes, powerup blend (2.91). Several are applied to the stream,
not the screen, so a server-side proxy saves link bandwidth with them.

### Teamplay helpers [R291]

- **Say-team macros** expanded by the proxy (only in `say_team`): `%A` armour type, `%a` armour,
  `%b` best weapon and ammo, `%e` enemies near, `%h` health, `%l` nearest location, `%n` send only
  to teammates near, `%N` hide from yourself, `%o` teammates near, `%p` powerups, `%w` weapon in
  hand and ammo, `%x` name of the thing pointed at, `%y` its location, `%g` powerups due in 15 s,
  `%i` last item picked up and where, `%j` last item pointed at, `%k` either, `%m` `%k` if within
  5 s else nearest item, `%d` where you died, `%r` last reported location, `%S` skin, `%C`
  colour, `%L %O %E` as `%l %o %e` frozen 5 s after death, `%q` last seen enemy's powerups, `%t`
  `%x at %y`, `%z` / `%Z` nearest waypoint by view / by motion, `%u` what you need (per the "need"
  thresholds). Default alias `f_report` = `say_team %Aa:%a h:%h %b %p%n`.
- **Point**: which item classes `%x` can name; "drop missed %x" (say nothing when nothing is
  hit); TF class pointing. **Pick**: which items `%i/%k/%m` report. **Names**: rename every item.
- **Locations**: `<map>.loc` files of named marks beside the proxy; menu to display, delete, mark
  (with the previous name), autoload on map change, include automatic marks (weapons, armours,
  powerups, megas), flush, reload; `%r/%b` in a mark name replaced by red/blue or enemy/own.
  **Waypoints**: marks plus a view direction, for `%z/%Z`.
- **Message channels** (2.91): `say_team … #channel`; a filter of which channels you listen to.
- **Soundtrigger**: a character (e.g. `!`) in `say_team` plays the named sound for teammates;
  sequential (text-to-speech-like, needs the proxy's sound system) or concurrent.
- **Team / enemy skin and colour forcing** (shirt/pants 0–255, `*` keep, `x` = player number);
  disabled under TF. Community trick: forced colours plus a special skin. [Wiki]
- **Message flash**: a talking teammate flashes like a muzzle flash.
- **Weapon impulses**: one impulse (e.g. 20) picks the best weapon from a list you have ammo for.
- **Powerup timer**: earlier versions timed powerup respawns (`%g`, a timer menu, `f_timer`
  answering "<- llama"); removed in 2.55, which always reports `notimers`. [R291] [Rold]
- **HUD clock**; **floodprotprot** (keeps you under a 4-messages-in-4-seconds flood limit);
  **CR→LF** (stops fake-name messages built with a carriage return); "fun" character conversion;
  **drop fov changes** the server stuffs.

### Voice and sound [R291]

- Real-time voice between proxy users who all play through one shared proxy (`client - Q1 - Q2 -
  server`, Q1 local doing capture and playback). Channels: create, join (max 8), part, bind a
  push-to-talk key per channel; a special channel `near` reaches players on the same server who
  are visible to you; feedback (hear yourself). Server `fpd` bit 64 stops spectators and players
  talking to each other.
- The proxy can play the game's sounds itself (client run with `-nosound`), mix with the client's
  window, 11/22/44 kHz modes, "enhanced stereo", mix-ahead, voice buffering, volumes, voice input
  gain, FX attenuation while voice plays; Winamp control and song-name announcing (Windows).
- Voice can be **recorded into demos** (2.91 "Record voice"); such demos need the proxy's own
  demo player [R291]; a demo tool later raised its packet size limit because "some qizmo demos
  with voice caused problems" [QWDt changelog].

### Demo recording, playback and conversion

- **Recording on the proxy** [R291]: keeps delta compression on (the classic client turns it off
  while recording), leaves out the client's own FPS effects, starts a new demo on each level
  change, and can run on another machine (no disk-write stutter on the player's PC).
  "Easyrecord" names files `player_ownteam_vs_enemyteam_map.qwd`, `player_vs_enemy_map.qwd`,
  `player_ffa_map.qwd` and appends `_2`, `_3`… when the name exists. Options: get pings
  (normal / while recording / always), overwrite or number, record voice, a game-event log like
  the client's console log. Demos go to `-e dir`.
- **Demo player** [R291] [Rold]: the proxy serves a demo to a connected client as if it were a
  live server; play, stop, rewind, fast forward, slow motion, any speed from −10000% to +10000%
  (`.demospeed x`); plays demos of any protocol version by converting to 2.3; chase-cam demos play
  correctly; the viewer is in observer mode; a demo appears as a normal client in the client list
  (so others can observe it too).
- **qwz files** can be played straight from the proxy [TF], and later clients play `.qwz` by
  launching the whole proxy program to decompress them [F4141].
- **Converting**: `qizmo -C demo.qwd` writes `demo.qwz`; `qizmo -D demo.qwz` writes `demo.qwd`;
  the program, `compress.dat` and the demo must be in the same directory [TF] [F5034]. A batch
  file shipped for this [ESR]. Only client demos (`.qwd`): nothing found says the proxy reads or
  writes server multiview demos (`.mvd`); those came later from the server side [MVD]. Converting
  POV demos into one multiview demo is a separate tool, which takes `.qwz` only by calling the
  proxy (`-qd <proxy dir>`) [QWDt].

### Observing and broadcasting

- **Observer mode** [R291]: one client watches "through the eyes" of another client on the same
  proxy. "New" mode: you are a spectator who can fly or use the chase/track cameras but only see
  the entities your target's connection receives, so you must track that player; "old" mode puts
  you in player mode. Autotracking; smoothing (smoother but later); setangle threshold; angle
  correction speed; yaw and pitch offsets; set/reset the client variables for observing; maximum
  centerprint lines; stop observing. Alias `f_reset` repairs the client's view variables.
- **Client list** [R291]: max observers of you, observer password (2.91), allow observers to
  `say_game` / `say_game_team`, kick an observer (or anyone, as admin), **Follow** (connect to the
  same server, or observe the same player, as another client), Observe, Server.
- **Cams** (how tournaments broadcast) [fQ]: a "cam" is a client observing the match through a
  proxy; viewers connect to that proxy, `.menu binds`, open the client list's Follow tab and pick
  a client shown as "Obs. <name>-cam". Two spectator slots on the server, hundreds of viewers on
  the proxies; commentary over a separate voice server.
- Its limits as recorded against the later broadcast relay (QTV) [QTV]: ~100 observers versus
  thousands; uses the server's maximum rate per viewer versus ~7 kB/s for a 4on4; server CPU like a
  player versus none; follows the commentator always versus switchable; cannot track anyone at
  will; "smooth experience: sometimes". The wiki calls its mass observing "low quality and
  comfort … deprecated" [Cat].

### Queries and server control

- **`f_` queries** said on a server; every proxy user answers, at most once per 20 s
  [R291]: `f_version [x]` (version, `notimers`, a checksum of x plus the program's memory image,
  and the OS letter: detects a modified proxy), `f_lagreport`, `f_modified[2|3] [x]` (checks the
  game's model and sound files that could be used to cheat; 2 ignores missile and grenade models,
  3 also the lightning bolt), `f_skins` (fullbright pixel % of forced skins), `f_server` (checksums
  of the proxy's and the destination's addresses: exposes bots and extra hops), `f_cmdline`,
  `f_speed` (speed-cheat check), `f_system`. A server key `f_query` (bitmask 1..128 in that order)
  asks automatically on connect. Aliases `f_fps` (`say .showfps`), `f_sens` (`say ,showsens`).
- **`fpd` serverinfo bitmask** to disable features [R291]: 1 %-reporting, 2 powerup timer, 4
  soundtrigger, 8 lag features, 16 force lag-change reports, 32 silent `%e`, 64 no voice between
  spectators and players, 128 silent `%x/%y`, 256 no skin forcing, 512 no colour forcing. TF
  servers: `%e/%x` use bottom colour as team, no skin forcing.
- Admin setup [R291]: quake dir, hostname (browser name), proxy name, public (listed on the
  public proxies' master), Windows priority and the local server's time slice, PS/2 mouse rate
  (Windows 9x), dynamic IP mask, max s2c repeats, save config.

### Console command words (both READMEs list the same 64) [R291]

`background browser browser_a connect clients c2spps clock color who weaponimpulse hostname
help record recordmode report reconnect disconnect dropextra talk time tcpconnect teamplay
voice voffset vidmode follow feedback fps flood fake fun observing observe observers quakedir
qwsv kick lag menu mmode mark mixahead mp3 name soundsystem smooth soundtrigger stopobs stuff
skins showfps initcapture password packc2s packs2c playsounds part play public priority ps2
join`. Help with `say proxy:help <cmd>`. Also documented elsewhere: `repeatc2s`, `repeats2c`
[Lag]; `showdrop` [R291]; `demospeed` [Rold]; `showsens` [R291]. The README gives no argument
lists ("everything can be controlled from the menu anyway"). Meanings by name and menu:
`packc2s/packs2c` compression per direction, `c2spps` client packets per second (likely),
`dropextra` (not described), `fake` (not described), `smooth` observer smoothing, `voffset`
observer view offset, `mmode` menu mode (likely), `mark` locations, `talk/join/part/voice`
voice, `stuff` make the client run a command, `qwsv` local-server priority control.

### The modern relatives (for the engine's three pieces)

- **Forwarder** (QWfwd): you never connect *to* it; the client sets `cl_proxyaddr` (chain with
  `@`) and connects to the destination through it; the client's browser builds a "ping tree" and
  `connectbr` uses the best route; ~5 kB/s per player; open source. [Fwd]
- **Broadcast relay** (QuakeTV): 4on4 ≈ 7 kB/s, 2on2 ≈ 3 kB/s, 1on1 ≈ 1.5 kB/s per stream;
  relay-to-server always one stream; chat delayed 500 ms; `qw://3@host:port/qtvplay` URLs. [QTV]
- The proxy (this note) is the only one of the three that did per-player features, link
  compression and packet repetition. [Cat] [Lag]

## (b) What is publicly known of the .qwz format

- **Name and purpose**: "Qizmo compressed demo", a lossless compression of a client demo
  (`.qwd`): "dramatically decreasing the file size, while containing all the information from the
  original QWD". [EzDemo] [QWD]
- **Ratio claimed**: "Qizmo compresses demos at 10:1". [TF] No other published figure was found.
- **Tooling**: compression and decompression only by the proxy program (`-C`, `-D`), which needs
  `compress.dat` beside it [TF]. `compress.dat` is 262,144 bytes (exactly 256 × 256 × 4) and
  deflates to 50,603 bytes [Zip]. Its role suggests a fixed (pre-trained) model shipped with the
  program; nothing public describes its contents.
- **Magic, header, versions, algorithm**: **no public description was found** — not in the
  READMEs, wikis, client manuals or forums searched. A forum moderator in 2012: the authors
  "couldn't agree on giving out the source" [F4141]. The community tool for demos says "No QWZ
  format support yet" [QWDt].
- **Reconstructions exist; not read.** A GitHub search shows repositories `osm/qizmo` and
  `osm/qizmo-unpack`, and a Go package `github.com/osm/quake/qizmo/freq` appears in search
  results. By this note's rules they were not opened; whether they are clean (black-box) work is
  unknown. Anyone deciding to use them goes through the `clean-room` skill.
- **Decodable from public information?** No. With only public facts, a reader could not write a
  decoder: the header layout, the coder and the model in `compress.dat` are all undocumented.

## (c) Observations of real .qwz bytes (observed)

Corpus: 16 compressed demos (14 named `.qwz`, 2 named `.qwd` that are in fact qwz), 6 true
`.qwd`, 15 `.mvd`; see INDEX.md.

- **Two-byte lead, first byte constant.** Every qwz starts `0x80`. The second byte varied:
  `0x01` (12 of 16), `0x02`, `0x03`, `0x08` (twice). Meaning unknown (a version, a model or
  option id, or the first coded byte).
- **No text, no recognisable header after that.** No magic string; no length or checksum at the
  start or the end (the last 8 bytes look random in every file).
- **Fully entropy coded.** Byte entropy is 8.000 bits/byte in every file; all 256 values occur. In
  the three files tested for it, per-64 KiB χ² against uniform was 199–294 (uniform ≈ 255) and
  the most common 3-byte sequences occur 3–5 times in 0.5–2 MB (random level). No periodic block headers were visible.
- **Not compressible further.** gzip −9, bzip2 −9 and xz −9e all make every qwz *larger* (by
  0.01–0.6%); an archive tool stored them uncompressed (a RAR in the corpus holds two qwz with
  method "stored").
- **Deterministic coding, early bytes shared.** 11 of the 12 files with lead `80 01` share the
  same first 16 bytes (`80 01 ba 1f 57 c1 ff f5 2f 06 6f eb f8 76 01 de`) even across different
  matches, servers and years (the twelfth, `exo_…_dm3_000.qwz`, a client-named `_000` file,
  differs from byte 2); three POVs of one match (game_749) share their first **181** bytes;
  two demos from different matches share bytes 0–15, differ in 16–19, then agree again for 156
  bytes (20–175). So the coder is deterministic and, early in the file at least, a local
  difference in the input does not change all later output. That fits a static model (one not
  adapting to the data) better than a fully adaptive one. Hypothesis, not established.
- **Sizes.** 4on4 player-POV qwz of a 20-minute map: 0.78–2.14 MB (median ≈ 1.4 MB); 1on1 POV:
  0.45 MB (one file). A 4on4 spectator-cam `.qwd` of the same era is 15.1 MB, and two 1998 qwd
  are 15.6–17.4 MB. Cross-match estimate of qwd : qwz ≈ 7–12 : 1 for 4on4, consistent with the
  claimed 10 : 1. No qwd/qwz pair of the same recording exists in the corpus, so this is an
  estimate only.
- **The plain formats with general-purpose compressors** (the yardstick a new format must
  beat):

  | Set (files) | gzip −9 | bzip2 −9 | xz −9e |
  | --- | --- | --- | --- |
  | `.qwd` client demos (6) | 2.79–3.61 : 1 | 2.63–3.63 : 1 | 4.14–5.57 : 1 |
  | `.mvd` server recordings, 4on4 (7) | 1.56–1.81 : 1 | 1.62–1.99 : 1 | 1.88–2.22 : 1 |
  | `.mvd`, 1on1 / 2on2 (6) | 1.61–1.86 : 1 | 1.73–2.02 : 1 | 1.87–2.22 : 1 |
  | `.mvd` merged from POV demos (2) | 1.50–1.52 : 1 | 1.58–1.64 : 1 | 1.79–1.80 : 1 |
  | `.qwz` (16) | 1.00 | 0.99–1.00 | 1.00 |

  A qwd is redundant (its own byte entropy 5.8–6.5 bits) because each frame repeats much of the
  last; an mvd already delta-codes entities and stores ~20 frames a second, so it compresses only
  about 2 : 1. If the 10 : 1 claim holds, qwz beats xz −9e on qwd by about 2×.
- **Store hygiene seen**: two `.qwd` files are qwz; one `.qwz` is a RAR; several "all pov
  (mvd).zip" are raw MVDs; one MVD lacks an extension; two archives are truncated; five `.qwz`
  are 56-byte text files. A reader must sniff content, not trust the extension. Magic to sniff:
  qwz `80`, qwd = little-endian float time then byte `01`…`0b 1c|1a` (server data, protocol 28
  or 26) a dozen bytes in, mvd `00 01` then a length then `0b 1c`, zip `PK`, RAR `Rar!`, gzip
  `1f 8b`.

## (d) Tools that still play or convert .qwz

- **The proxy itself** (Windows and 32-bit Linux builds of 2.91, still mirrored): `-D` to get a
  `.qwd`, `-C` back; or serve the demo to a client through its demo player. Linux build needs
  32-bit libraries on a modern system. [TF] [Wiki] [F4141]
- **ezQuake**: records match demos in qwz by default (`demo_format qwz`; options `mvd`, `qwd`) and
  plays `.qwz` by running the proxy program from `qizmo_dir` (default `qizmo`) to decompress
  first, so it works only where that program runs (Windows; on Linux only with the 32-bit build).
  [EzDemo] [F4141] An issue asking for qwz playback on Linux exists
  (`https://github.com/ezQuake/ezquake-source/issues/149`; not read: the page was rate limited).
- **QWDtools** (qwd → mvd converter): takes qwz only by calling the proxy (`-qd <proxy dir>`).
  [QWDt]
- **Reconstructed decoders** may exist (the GitHub names above); not examined.

## What this means for the engine (inferences, not sources)

- The proxy's features split cleanly into engine and game: chaining, routing, forwarding, packet
  repetition, link compression per direction, loss/lag simulation, demo recording off the
  player's machine, serving a recording as if it were a server, observers, cams and relays are
  game-independent; the say-team macros, item names, `.loc` marks, skin/colour forcing, the FPS
  filters (nails, gibs, explosions) and the `f_` checks are the classic game's.
- Its ideas to keep: one hop does not need to compress, only the slow one; compression and
  repetition are chosen per direction at the sending end; a "quality line" (cross-packet state)
  mode trades loss tolerance for ratio; a static trained model shipped with the program makes
  demo files deterministic and small. Its ideas to drop: commands smuggled through chat, a menu
  drawn in centerprints, a closed format.
- The ratios a new demo codec must beat on this corpus: xz −9e at ~4–5.6 : 1 on client demos,
  ~2 : 1 on multiview recordings, and the claimed 10 : 1 of qwz.

## Hosts that were blocked or failed during this research

`web.archive.org` (connection reset; WebFetch: site blocked), `archive.ph` (reset),
`discmaster.textfiles.com` (reset), `qizmo.quakeworld.nu` (gateway 502),
`timetravel.mementoweb.org` (502), `aluigi.zenhax.com` (robots disallow), `qwdrama.com` (DNS
fails), `www.boards.ie` (403), `github.com` per-repository API and GraphQL (403 for this session;
raw files via `raw.githubusercontent.com` worked), `github.com` issue pages (rate limited).
`archive.org` (not the Wayback Machine) worked, including single members out of a tar.
