# Porting a classic game: where each piece goes

For whoever is about to bring the next classic game into Shooter's Edge. The order of the
work is `docs/shooters-edge.md` § A third classic game: the checklist; this page says
**where** each piece and each console setting lives. The refactor that put today's code in
these places is the M60 row (`docs/goals/M60.md`).

**System by system**: [`docs/porting/`](porting/README.md) takes each system of a game (content,
maps, picture, models, held weapons, sound, movement, network, server, rules and mods, client,
settings, demos, bots, online services, the small features) and says what is shared and where,
what each game owns, the hook points a new game fills, and keeps the ledger of every game's
benefits taken by all and drawbacks left behind ([`docs/porting/ledger.md`](porting/ledger.md)).

A game of another family, whose maps are a flat plan of sectors and whose figures and held
weapons are pictures, has its own research page: `docs/porting-sprite-games.md` (the map
converted to 3D, the family's move beside the first classic game's, sprites in remix, the risks).

**The core rule: lift what is truly shared, and only that.** When the next game needs something
that already exists for one earlier game only, lift it a layer up (game → shooter plugin → engine)
when it is truly shared: code that has to change at the same time in every game, and whose every
improvement every game should get. Anti-cheat, voice chat, lobbies and crosshairs are shared.
Sameness by coincidence is not: two games having a weapon of the same name and idea does not
make one weapon. Shared code must not turn into code full of per-game branches; if a lifted piece
needs an `if` on the game, it was not shared, and stays in each game's module.

The test: would an agent changing it in one game ask "should I change this in the other game
too? It still uses the old implementation"? Then it is shared. Shared, so every game gets each
improvement: pak and file handling, the netcode, the server, TV, proxy, forward, the hub,
anti-cheat, voice, lobbies, crosshairs. Demos are split: each game's demo format is its own, the
multiview demo's machinery is shared where it can be, and what a player does with a demo
(playback, seeking, speed, following a player, highlights) is the same in every game.

Then put each piece in the narrowest layer whose users all share it with the same meaning: not
lower (a copy per game), not higher (a knob for every game's variant).

## The layers

Dependencies run down only: shell → game module → plugins → engine.

| Layer | Home | What it holds | Its settings |
| --- | --- | --- | --- |
| **Engine** | `engine/` | Any genre: the picture, sound, input, the window, files and packs (`vfs`), the network, the dedicated server and lobby, the console, saves, replays, mods, labs, `ge`. The sky, clouds, weather and wind. The live map's drawing (`engine/overview`; a game brings only an adapter, `docs/shooters-edge.md` § the checklist, step 13) | `r_`, `s_`, `vid_`, `in_`, `m_`, `net_`, `con_`, `scr_`, the sky's and weather's cvars |
| **Shooter plugin** | `plugins/shooter/` (with the readers `plugins/brush`, `plugins/morph`, the VMs `plugins/progs`) | What every shooter shares: the move, the loop, the protocol family, the content layer (search path, open calls), weapons and projectiles as mechanisms, the HUD's parts, demos and recording, the scoreboard, match state, movers (doors, lifts, trains), bots, item roles | The move and loop (`sv_`, `pm_`, `plugins/shooter/settings.hpp`), the shared HUD, demo and view settings |
| **Classic enhance** | `plugins/classicenhance/` | Better looks and sound for the player's own packs | One `ce_` switch per effect (`switches.hpp`); every effect can be turned off |
| **Game module** | Each classic game's own files: its profile row, adapter, rules, poses, bar, demo and net files (Quake II's are the `q2_*` files under `games/shooters_edge/`) | That game's rules, items, weapons' numbers, monsters, file formats as the game reads them, protocol version, demo format, menus' art | Settings only that game has, registered by its module |
| **Shell** | `games/shooters_edge/` | The menus, the game picker and pack loading, the server browser, remix | The shell's own (`hud_style`, the menus) |

## Placing a setting or command

Ask in order, and stop at the first yes:

1. Would a game of another genre want it, meaning the same thing? **Engine.**
2. Does every shooter want it, meaning the same thing in each? **Shooter plugin.**
3. Is it an upgrade over how the original looked or sounded? **Classic enhance**, as a `ce_` switch.
4. Otherwise it is **the game's module**.

The prefix follows the subject, not the layer (`docs/console.md` § Names). A demo command
that reads every game's demos is the plugin's; one that reads one game's format is that game's.

### Where today's settings live

| Layer | Settings and commands | Home |
| --- | --- | --- |
| Engine | The window (`vid_fullscreen`, `vid_display`, `volume`), the demo video (`demovideo_`, video.cfg), a hosting window's lobby words and settings (`ready`, `team`, `sv_lobby`, `sv_port`, `sv_maxclients`, `sv_login`), the dedicated server's `sv_` and `net_` | `engine/platform/window_cvars.hpp`, `engine/capture/video_cfg.hpp`, `engine/server/listen_console.hpp`, `engine/server/server.hpp` |
| Shooter plugin | The move buttons and mouse look (`+forward` ... `+attack2`, `sensitivity`, `m_invert`, `lookspring`, `cl_run`, `cl_yawspeed`), what a player does with a demo (`cl_demospeed`, `demo_seek`, `demo_skip`, `demo_next`, `demo_bar`, `demo_lerp`), the move and loop | `plugins/shooter/controls.hpp`, `plugins/shooter/demo_transport.hpp`, `plugins/shooter/settings.hpp` |
| Game module | Its default keys, a mod's `impulse`, the quick weapon's and weapon groups' settings (they read its status bar), its demo format and recording's packing, its look settings a video resets, `scr_levelstats` | The game's `client/` files beside each |

A game's struct of settings derives from the shared one and adds its own fields
(`ControlSettings : ge::shooter::Controls`, `DemoPlayback : ge::shooter::DemoTransport`), and its
register call runs the shared one first: one body, no copy.

## One name, one meaning

Two games can give one cvar name two meanings: a classic game's `crosshair` picks a shape where
the next one's picks a picture, and a shadow setting can mean a blob under each model in one game
and a stencil volume in another. Players' configs carry each game's meaning, and **each game keeps
its classic look by default**: a newer game's better picture never reaches an older game because
both use the name. So a name means one thing per loaded game, and nothing merges two meanings
silently.

**The layers.** The window registers every cvar in a layer (`console/cvar.hpp` § LAYERS,
`games/shooters_edge/client/cvar_layers.hpp`): `engine`, `shooter` (the genre plugin), `classic`
(the classic plugin's picture), `shell` (Shooter's Edge's own) and `game:<id>`, the LOADED game's
own settings, registered when it loads and taken away when another game loads.

**Before a game registers a name, settle what it means in every game that has it.** The meaning
in each classic game comes from a clean-room reader (`clean-room` skill), never from a guess by the
name; the facts are kept in `porting/shooters-edge/cvar-meanings.md`. Then:

1. **Every game that has it means the same, and the lower layer's setting is that thing** (same
   effect, units and direction; only the default differs): register it once, in the lowest layer
   they share, and give each game's default as a `same` row of the game's layer. Example:
   `scr_conspeed`, `loc_dist`, `fov`.
2. **The game means something else by it** (another effect, unit, direction, or values that mean
   other things): it is the game's own. Register it in the game's layer as `own`, bound to the
   game's own field and drawn by the game's own code: the lower layer's setting waits, unreachable,
   while the game is loaded, and comes back with its value when another game loads.
3. **Only one game has it**: it lives in that game's layer (or in a lower layer only if it is a
   lower layer's concept that other games will share; the camera's bob is the engine's).
4. **Ambiguous** (the name suggests one thing, the games differ, or the reader could not settle it):
   do not merge. Keep it in the game that is sure and leave the other game without it until a
   reader settles it. A shared setting with an `if` on the game is two settings.
5. **Another name for the same thing** (Quake II's `vid_gamma` is our `gamma`): ONE SETTING IN
   OUR NAME, and the game's own spelling a synonym of it in the game's layer
   (`Cvars::add_synonym`; a command's: `Console::add_command_synonym`). The game's own configs
   run as written, and our menus and saves use our name, so one config serves the whole series.
   Pick our name once (the engine's or the plugin's word for the thing), never a game's spelling
   because it came first. A command called `x` in one game and `y` in the next is ours `z`, with
   `x` a synonym while Quake is loaded and `y` while the second is.

**What lives in the game's layer lasts only while the game is loaded**: its spellings (setting and
command synonyms), its defaults (`same` rows), its own meanings (`own` rows) and its own settings.
Loading another game takes them all away (`Console::drop_layer`) and puts that game's in their
place, so typing a game's spelling works only while that game is the one loaded.

An undeclared meeting of two layers is a CLASH: the registry keeps the first, never writes the
second field, and records it. It is never printed to a player (another game loading is not a
fault), and `tests/shooters_edge_cvar_layers_test.cpp` fails on it for every game. A port is not
done while that test reports a clash.

**One config for the series.** The player's settings are one file for every classic game,
`<data folder>/settings/config.cfg`, in our names (`client/settings.hpp`); beside each game,
`<game folder>/settings/config.cfg` holds only that game's own meanings of shared names (`own`
rows and the game's names of its own). The series file is read with the loaded game's layer away,
so a game's own meaning never takes the series' value, and each setting is saved with the value
it has outside the game (`console/cfg.hpp` § SAVING ONE PART): Quake's crosshair stays
Quake's while Quake II is loaded. A game folder's config from before the series file
holds everything; it is read once as the game's file and written back split.

**A game's own value over the common one.** In a config, the game's spelling of one of our
settings is that game's own value (`vid_gamma 0.9` beside `gamma 1`: Quake II shows 0.9, every
other game 1), in any order; `<game>:<name> <value>` says it in our name (`game:` for the loaded
one), `unset <game>:<name>` takes it away, and a value typed later is the common one again. The
loaded game's own values are saved in its file and other games' wait in the series' file; a named
`cfg_save empezar` writes `empezar.cfg` and `empezar.<game>.cfg` (`docs/console.md` § Scopes).

### The census: today's names in the two classic games

From the reader's facts (`porting/shooters-edge/cvar-meanings.md`, 2026-11-18) against every cvar
the window registers. "First" is the first classic game, "Quake II" the second.

| Kind | Names | What the window does |
| --- | --- | --- |
| **One setting in every game** | `sensitivity`, `cl_yawspeed`, `cl_run`, `lookspring` (shooter); `cl_rollspeed`, `con_notifytime`, `volume`, `bgmvolume`, `vid_fullscreen` (engine); `gl_polyblend`, `gl_picmip` (classic) | Registered once in its layer; the classic defaults match |
| **The same, another default** | `scr_centertime` (first 2, Quake II 2.5); `scr_conspeed` (first 300 lines a second, Quake II 3 screens a second: another unit, `client/game_start.hpp`); `loc_dist` | The game's layer gives its default (`same`), or the game's row in `profiles/` |
| **The same, Quake II's own spelling** | `vid_gamma` = `gamma` (higher darker in both), `s_volume` = `volume`, `gl_dynamic` = `r_dynamic`, `gl_lightmap` = `r_lightmap`, `cl_gun` = `r_drawviewmodel` | Synonyms in Quake II's layer, gone with it |
| **One thing, two units** | The gun's offset: ours `cl_gunx`/`cl_guny`/`cl_gunz` (metres, right, up, forward), Quake's `r_viewmodeloffset` (one text of units, right, down, forward), Quake II's `cl_gun_x`/`cl_gun_y`/`cl_gun_z` (units, right, forward, up). The gun's lens: ours `r_viewmodel_fov` (degrees vertical, 0 the view's), Quake II's `cl_gunfov` (degrees across at 4:3, 90) | Each game's in its layer, in its units: an offset ADDS to ours, and our lens wins when set (`view/hand.hpp`). A synonym that converts units would fold them into ours |
| **One thing, two names, one shadowed** | The frame cap: ours `cl_maxfps`; Quake II's `r_maxfps`, while its own `cl_maxfps` (the moves a second) sets ours aside | Quake II's layer binds `r_maxfps` to the same cap, started at the cap in force when it loads and saved with its own settings; ours comes back with its value when it leaves (`client/cvar_layers.cpp`). Follow-up: a synonym that reaches a setting set aside while its name is shadowed would make it one setting |
| **Another meaning in Quake II** | `crosshair`: Quake 0 none, 1 its +, 2 a dot (the engine's marks extend it); Quake II 0 none, 1 to 3 its own pictures `ch1`..`ch3` | Quake II's layer registers it `own` (0..3, its pictures); the engine's comes back when Quake loads |
| **Quake's only, inert in Quake II** | `cl_bob`, `cl_bobcycle`, `cl_bobup`, `cl_rollangle`, `v_kickpitch`, `v_kickroll`, `v_kicktime`, `v_idlescale` (Quake II's view is its rules': `bob_up`, `bob_pitch`, `run_pitch` and kin, server settings) | The engine's camera; Quake II's view does not read them |
| **Quake's only, still applied to Quake II** | `v_contentblend`, `v_quadcshift`, `v_pentcshift`, `v_ringcshift`, `v_suitcshift`, `v_damagecshift`, `gl_cshiftpercent` | Next: move them to Quake's layer so Quake II's screen tints are its own (no such settings) |
| **Ambiguous: other meanings in the games, ours is the engine's** | `cl_maxfps` (first: frames and packets 30..72; Quake II: frames, 90; ours: a draw cap), `r_drawviewmodel` (an opacity in the modern client), `r_drawflat`, `v_contentblend` (a switch or a strength), `crosshaircolor` (a palette index or RGB), `skin`, `deathmatch` (a rule variant or a switch), `password`, `sv_airaccelerate`, `samelevel`, `in_mouse` | Not merged with any game's: a game that needs its own meaning registers it `own` |
| **Not ours yet, three meanings** | `r_shadows` (first: a 50 % black flattened silhouette under each model, 0 by default), `gl_shadows` (Quake II's: the same silhouette), `cg_shadows` (the third game: 1 a round blob, 2 stencil volumes, 3 the silhouette) | When the classic shadows are built, the silhouette is one setting the first two games share by their own names; the third game's is its own |

**Lifting.** When the next game's meaning of a game-only setting turns out the same, move the
setting a layer down in its own PR and turn the earlier game's row into a `same` default; when it
turns out different, both stay each game's `own`.

## Bots: what is shared and what is each game's

- **Shared:** what a team message means (`plugins/shooter/team_talk.hpp`: `SayKind`, `Strategy`,
  `Said`), what a bot sees of a match (`bot_view.hpp`), the item tables' shape
  (`item_tables.hpp`), the rules' world, and the engine's `engine/ai`: the decision record, the
  keyboard and mouse hands, thinking on a schedule, sight, memory, choosing. The decision frame
  (goals as scored options, claims, giving up), a bot's memory and its model of the other players
  are shared by the classic games' bots today and lift to the plugin when a game outside this
  shell needs them unchanged.
- **Each game's:** movement (how it drives the hands: the hop, gap and blast-jump tricks, the
  footwork, swimming), item worths, map plans and routes, weapon choice, and how its team talk is
  worded and its places named (the say lines, the macros, the loc file). A game words the shared
  meanings its own way: its `say_text` and `parse_say` over `SayKind`.

## Do not over-generalize

- **The same weapon in two games is two weapons.** A rail gun in Quake II and in the next
  game keeps each game's numbers, sounds and settings in each game's module. Share the
  mechanism (a hit-scan trace, a trail's drawing), never one weapon with a knob per game.
- **Classic enhance may offer one effect per game**, for example two rail trails, each profile
  naming its own. It does not grow one effect with parameters that cover both.
- **The same bot move in two games is two moves.** A hop or a blast jump is tuned to each game's
  numbers; share the hands that press the keys, never one trick with a knob per game.
- **Lift when the next game needs it, not before.** When its piece and an earlier game's must
  change together, move it up a layer in its own PR (goldens unchanged prove the move); until
  then, a file of the game's own beside the previous game's.
- **Nothing shared asks which game is running.** A difference between games is a field of the
  profile row or an entry in the module's table, never an `if` on the game's id.

## Improvements one game had, offered to every game

(Asked by the maintainer on 2026-11-19.) What makes one classic game better to play (a convenience,
an aid, an effect that tells the player something) is offered to every game: its mechanism in the
lowest layer they all share, its setting under one name of ours. **It is on by default in the game
it came from and off in a game that never had it**, so each classic game still looks and plays as
it did until the player turns it on: the setting's default is the older game's (off) and the game
it came from sets its own with a `same` row in its layer (§ One name, one meaning). The census,
both ways:

| What | From | Where it lives | Default |
| --- | --- | --- | --- |
| A power's coloured shell on its carrier (`r_powershells`) | Quake II | the window's model draw (`client/q2_look.hpp`'s `power_look`), either game's bits | on in Quake II, off in Quake |
| An item's pulsing light (`r_itempulse`) | Quake II | the same | on in Quake II, off in Quake |
| The landing dip (`cl_landdip`) | Quake II (its server's fall kick) | `engine/camera`'s first-person rig | 0 in Quake; Quake II's own kick from its rules |
| The easy jump, the blast-jump helper, aim help, the quick weapon, weapon groups, place names, the demo transport, chat ignore lists | Quake's clients | `plugins/shooter`, `engine/console` | as each game's clients had them |
| The radar, autotrack, highlights, the live map | Quake's tools | `engine/overview`, the window's demo code, on both games' recordings | on |

Left in their game, and why:

- **Footsteps** (Quake II): Quake ships no step sounds, and a game uses only its own
  assets outside remix; remix carries them (`profiles/remix_sounds.hpp`).
- **Weapons seen in other players' hands** (Quake II): Quake's player model has no
  weapon to change.
- **The help computer and the pickup icon** (Quake II): its single-player objectives have no
  counterpart in Quake, which says its pickups in the feed.

## Remix

Remix plays one game with another's maps, items, figures, movement or mod. So every piece of a
game module is either **lifted a layer** (when it is truly shared) or **reachable through the
module's tables** (the adapter, the rules runner's calls, item roles), so remix asks for it by
role and never by game. A game uses only its own assets everywhere except remix.

## Taking a game in: installs, discs, images and the soundtrack

A classic game is never played from where the player keeps it. Whatever they point at (an
installed game's folder, a store's install, a mounted disc or USB stick, or a disc image: `.iso`,
`.cue` with its `.bin`, a `.bin` or `.img` alone) is COPIED into the game's own data folder, and the
copy is what plays (`plugins/classic/content/import.hpp`; the window's side,
`games/shooters_edge/client/take_in.hpp`):

    data/<profile id>/<base folder>/pak0.pak ...     the packs and the loose game files
    data/<profile id>/<multiplayer folder>/...        when the source has one
    data/<profile id>/music/track02.flac ...          the soundtrack, one layout for every source
    data/<profile id>/music/tracks.txt                its manifest: one line a track
    data/<profile id>/taken.txt                       the source's fingerprint and the files put in place

The data folder is the player's own, outside the game's program, so it outlives a rebuild, a new
version and a moved app: `~/Library/Application Support/gameengine2/shooters_edge/data/` on a Mac,
`~/.local/share/gameengine2/shooters_edge/data/` on Linux, `%APPDATA%\gameengine2\shooters_edge\data\`
on Windows (beside the game only when that cannot be written). A game is taken in once: a second
launch pointed at the same source finds its fingerprint (every file's path, size and time; a disc
image's own) unchanged in `taken.txt` and opens the copy without reading the source.

What a new game brings, and where:

- **Its content profile** (`plugins/classicenhance/profile.hpp`): the base folder and the first
  pack's name are how a source is recognised (at any depth: a disc keeps the game under an
  install folder, a store under the game's own name) and what is copied. Nothing else to add for
  the files: archives (`*.pak`, `*.pk3`, `*.pkz`) and loose files under the folders a game reads
  are copied, never programs, configs, saves or demos. A game whose loose content lives in a
  folder the list does not name adds that folder to `content_folder` in import.cpp.
- **Its installer, when the disc does not hold the game as files**: Quake's disc ships
  an LHA archive (`resource.1`) that the import opens in memory. Another installer format is a
  reader in `engine/vfs` and one more case in `open_installers`.
- **Its soundtrack row** (`kSoundtracks` in `plugins/classic/content/soundtrack.cpp`): the disc's
  first and last music tracks and, when known, each track's length on the release's disc. Disc
  track 1 is the data; music tracks keep the disc's numbers (`track02` ...) whatever the source
  calls them: a store's `track02.ogg`, a rip's `Track02.flac`, an album's `01 - title.flac`
  (moved up one) all become `track02.flac`.
- **Its known tracks** (`kKnownTracks`, the same file): a track is known by the SHA-256 of its
  samples as little-endian 16-bit, which is the SHA-256 of the track's bytes in a disc image, so a
  `.bin`, a WAV rip and a FLAC rip of the same disc agree. Record a release's hashes from the
  manifest the first time a real disc is taken in (`tracks.txt`'s `pcm=`); never commit the audio.

The music is always FLAC (`engine/audio/flac.hpp`), the samples exactly as the source decodes:
a disc's track bit for bit, a store's Ogg Vorbis decoded once (lossy before it came, nothing
more lost). A better source replaces a worse one (a disc, then FLAC or WAV, then Ogg Vorbis); the
same samples again are left alone. Where the stores keep music, as players report it (not yet
checked against a real install here; correct this when one is taken in): the reissues keep
`trackNN.ogg` files in a `music/` folder in or beside their base folder under `rerelease/`; the
older store installs carried no music (their programs played the disc); other stores' installs
keep `music/` at the game's top or in the base folder. The import looks in any folder named `music`,
`cdtracks` or `soundtrack` at the game's top or beside its base folder, and in the base folder's
packs' `music/` entries; a mission pack's or mod's `music/` is not the base game's and is left.
The stores' library folders and the mounted volumes (`/Volumes` on a Mac) are `store_folders()`.

**Playing it.** The engine plays a kept track (`engine/audio/music.hpp`): decoded on a worker
thread, its left and right channels as two sampled voices panned hard to each side on the music's
own fader, `bgmvolume`, with the CD player's console command (`cd play|loop|stop|pause|resume`,
`engine/audio/music_console.hpp`). A port adds only the way a track number becomes the kept file
(`track_path`) and the track a level asks for (the classic family's world `sounds` key,
`level_track`); a game with another way to name its music maps it onto the same track numbers.

A game the window was started inside, or placed above, still plays where it is: that install
is the game's home, its configs and place names with it (`client/install_search.hpp`).

## Downloads into archives

A file a client lacks and fetches from a server goes back into the archive it came out of on the
server: `pak1.pak` is filled as `pak1.partial.pak` beside the game's packs and renamed to
`pak1.pak` once every file of it is held; a file the server keeps loose lands at its place under
`data/<game>/<folder>/`. Nothing in that depends on the game or the archive's format:

- **The archive** (`engine/vfs/partial.hpp`): `archive_origin` reads a file's origin off the
  server's search path, `keep_in_partial` fills the partial and puts the archive together. A pack
  comes back byte for byte; a zip-family archive (`.pk3`, `.pkz`) holds the same files in the same
  order. A new archive format is one more source kind in `engine/vfs` and one more layout there.
- **The origin on the wire** (`write_origin`, `read_origin`, the same file): one body every
  protocol carries alike. A game's protocol sets aside one message id for it, sends it only to a
  client that agreed to it (the classic family's agreement is the `download-origin` bit of the
  engine's extension set in userinfo) and sends it before the file's first piece.
- **The two ends** (`plugins/classic/content/downloads.hpp`): the server answers the origin with
  `served_origin` over its content, one line in the game's config; the client hands every fetched
  file and the origin (or none) to `keep_download`, which writes it and mounts it at once.

What a new game writes is only its protocol's part: its download messages, the message id for the
origin and the agreement, and where its client calls `keep_download`. A server that is not ours
tells no origin, so the file is kept loose and still used. Keep the original game's rules on what a
server will send to a client that did not agree (Quake II's server never sends a map held in an
archive to such a client). Quake (`net/qw_signon.cpp`, `net/qw_client.cpp`)
and Quake II (`net/q2_net_server.cpp`, `net/q2_client.cpp`) are the two examples.

## Port from a ledger, never audit after

Quake and Quake II were each ported system by system and then audited for gaps, again and again,
and every audit found dozens more: a reader answers what it is asked, so a comparison system by
system finds only what its questions name. Both games ended by writing a **ledger**: one line for
every thing the reference has (each command, setting with its default, entity, item, monster,
message word for word, menu row, protocol message, flag, layout word), each marked `done`,
`partial`, `missing` or `not-taken` against our tree, committed in `porting/` and worked down one
big PR at a time by its missing count. The next game starts there: **the ledger is written before
the first line of the port, and the port is built from its rows** (`port-from-ledger` skill).

What the two ledgers taught:

- **Pin the reference first, by repository and commit, with the owner.** One ledger pass was
  read from a mod's fork of the chosen reference: a twentieth of its rows were the fork's own, and
  its muzzle flashes were the mod's. The ledger's header names the repository and the commit.
- **Write the whole ledger before any fill.** Rolling audits move the target; a complete list only
  goes down, and its count is the progress report.
- **Enumerate from the source's own tables**, not from the game as remembered: the command table,
  the setting registrations with their defaults, the spawn table, the item list, every print.
- **One reader per half** (the rules, the server, the client), each **saving its file table by
  table**, so a reader stopped by a usage limit or a timeout loses nothing.
- **A status found by a name is a guess.** A setting of the same name may mean another thing here
  (§ One name, one meaning); a row is `done` when a test or a lab holds it as a player sees it.
- **Know what a check checks.** A lab that puts players on each key and exit tests co-op's
  plumbing, not that anyone can get through a map; a bot's playthrough is another lab. Say which
  one a result comes from.
- **A program the reference's release lacks** (Quake's single-player rules ship only as the pack's
  compiled program) is run unchanged and its rows are held by playing it, not by reading it.

## Lessons from Quake II

Each is a lesson in `.agents/skills/<skill>/.lessons/` (`make lessons ARGS="shooter"`); read
the skill's lessons before the first edit.

**Bringing a game in**
- A new game is a content row, a game row and a thin adapter of one-line forwards. Ask its files
  through `plugins/classic/content` and the adapter, never by file names in the client.
- A mode's program is found by the profile's file names and its form checked, never by a fixed name.
- Take a classic program's numbers from the program itself; write any stand-in text new.
- Move a hard-coded path into the profile and prove the move by the labs' digests, not by reading.
- A second rules runner reads the map's own entities, mirrors its world into the loop's entities,
  and counts the host's time by the call. Every field the loop reads must be in the mirror, or it
  is silently zero.
- Know a release by its files (SHA-256); a pack that looks like a repack may be another release.

**Moving code between layers**
- The window asks the rules' world or a runner's trait, never which runner it is. Types the rules
  share live in `plugins/shooter`; the filler for one program form stays in the game.
- Shared open calls take the game's content row as an argument, never its adapter.
- Lift by the third copy at the latest: the movers became one kit
  (`plugins/shooter/mover_motion.hpp`), items swap by role (`item_roles.hpp`, one list per game).
- A move that keeps the goldens is proof the lift changed nothing; a setting that replaces a
  constant keeps the constant's arithmetic, or the goldens move.
- `main.cpp` files sit near the line cap: new wiring goes in a file of its own.

**Remix**
- Another game's figure plays this game's poses by its frames' names, not their numbers, and
  wears its own palette.
- A move told another game's numbers feels like that game only with all of them (the landing
  pause too); the stair double jump lives in the air's slide, not the jump.
- The classic rules run on Quake II's maps by tracing its models as brush solids.
- Each part of a match is one `cr_` switch naming whose part it is, chosen apart from the others:
  items never change the sounds (`cr_sounds` does), the HUD follows the weapons in use, and the
  figures and sounds are swapped on the client, so they hold over any server or mod.
- A pairing between games is a table of roles in `profiles/` (`remix_sounds.hpp`,
  `remix_monsters.hpp`): a row per part both play, by role first and then by strength. A
  many-to-one pairing lists the extra rows after the first, and going back takes the first. A part
  with no row is kept as it is; silence can be a row too.
- Over a mod (`cr_over_mods`), only what the engine owns can be set over the mod's: the movement,
  by the server's lines after the mod's and by the engine's move that a game library asks for.
  What a mod's weapon does is compiled into the mod and stays its own.
- A monster swapped whole is the other game's numbers as data in the played rules' own shape: the
  Quake II's roster rows for the classic monsters, a spawn over the classic minds for Quake II's.
  Where the played rules cannot run a monster, it becomes the played game's partner
  (`kRunsMonsters`), so a map never loses its monsters.
- Data one game's rules need from the other's is header-only (a roster, a pose table), so either
  rules library reads it without linking the other's code. Strings a native program adds for the
  other game's things go after its own list, built from that data rather than typed again.

**Menus and text**
- Every game's big menu lettering is cut into an alphabet of its own, one picture per character,
  from the player's pack at load (Quake's carved letters, `menu/carved_letters.hpp`;
  Quake II's plate letters, `menu/plate_letters.hpp`), with the letters a pack lacks pieced
  from the ones it has. Then any word, a page's title or a remixed menu's, is written in that
  game's own letters, never in another game's or in a stand-in font.

**Classic enhance**
- A stage that reports zero on a new game's packs is a gap, not a pass: count each stage's files
  against the packs, matching names in any letter case.
- The replacement table says what the engine draws instead; the game still spawns the thing.

**Console**
- A shared cvar takes its default from the game's settings, and its help words follow.
- A game wraps an engine command by keeping its function, not by copying its body.
- A command registered after the console starts is unknown to `+` lines on the command line.
- A meta-cvar sets values under the explicit ones.
- A player's text reads like a game's (`docs/architecture.md` § Player text).

## Working rules

- **The ledger first**: a port builds the rows of the game's ledger, and a game without one gets
  one before its first line of code (`port-from-ledger` skill); an occasional audit corrects the
  ledger itself (`gap-analysis` skill), never a fresh comparison system by system.
- **The previous game is the template**: the next game's file sits beside Quake II's and has
  the same shape (§ the checklist above).
- **Look first at how the earlier games solved it.** Before building any piece (downloads, demos,
  a server setting, a HUD element, a bot's move), find how the games already ported do it: `make
  find Q="<words>"`, this page's sections, the earlier game's file of the same name. When their
  solution is a shared one (in `engine/` or a plugin), use it as it is and add only the game's
  wiring. When the new game needs it a little differently, build that as a layer on top of the
  shared piece (a wrapper, a table entry, a field of the profile row) rather than a second copy
  beside it; when it really differs, it is the game's own (§ Do not over-generalize). Which of
  the three is a judgement call: make it, and say why in the PR.
- **Our rules are C++**, on the rules runner. The VMs only run community mods' bytecode.
- **Clean room**: source this tree may not copy is read only by a reader subagent
  (`clean-room` skill).
- **No product names** outside `porting/`: say the first classic game, Quake II, the next game.
- **Labs use real maps and models** from the player's packs, never test maps or test models.
