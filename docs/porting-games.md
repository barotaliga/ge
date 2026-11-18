# Porting a classic game: where each piece goes

For whoever is about to bring the next classic game into Shooter's Edge. The order of the
work is `docs/shooters-edge.md` § A third classic game: the checklist; this page says
**where** each piece and each console setting lives. The refactor that put today's code in
these places is the M60 row (`docs/goals/M60.md`).

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
| **Engine** | `engine/` | Any genre: the picture, sound, input, the window, files and packs (`vfs`), the network, the dedicated server and lobby, the console, saves, replays, mods, labs, `ge`. The sky, clouds, weather and wind | `r_`, `s_`, `vid_`, `in_`, `m_`, `net_`, `con_`, `scr_`, the sky's and weather's cvars |
| **Shooter plugin** | `plugins/shooter/` (with the readers `plugins/brush`, `plugins/morph`, the VMs `plugins/progs`) | What every shooter shares: the move, the loop, the protocol family, the content layer (search path, open calls), weapons and projectiles as mechanisms, the HUD's parts, demos and recording, the scoreboard, match state, movers (doors, lifts, trains), bots, item roles | The move and loop (`sv_`, `pm_`, `plugins/shooter/settings.hpp`), the shared HUD, demo and view settings |
| **Classic enhance** | `plugins/classicenhance/` | Better looks and sound for the player's own packs | One `ce_` switch per effect (`switches.hpp`); every effect can be turned off |
| **Game module** | Each classic game's own files: its profile row, adapter, rules, poses, bar, demo and net files (the sequel's are the `sequel_*` files under `games/shooters_edge/`) | That game's rules, items, weapons' numbers, monsters, file formats as the game reads them, protocol version, demo format, menus' art | Settings only that game has, registered by its module |
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

- **The same weapon in two games is two weapons.** A rail gun in the sequel and in the next
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

What a new game brings, and where:

- **Its content profile** (`plugins/classicenhance/profile.hpp`): the base folder and the first
  pack's name are how a source is recognised (at any depth: a disc keeps the game under an
  install folder, a store under the game's own name) and what is copied. Nothing else to add for
  the files: archives (`*.pak`, `*.pk3`, `*.pkz`) and loose files under the folders a game reads
  are copied, never programs, configs, saves or demos. A game whose loose content lives in a
  folder the list does not name adds that folder to `content_folder` in import.cpp.
- **Its installer, when the disc does not hold the game as files**: the first game's disc ships
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

## Lessons from the sequel

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
- The classic rules run on the sequel's maps by tracing its models as brush solids.
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
  sequel's roster rows for the classic monsters, a spawn over the classic minds for the sequel's.
  Where the played rules cannot run a monster, it becomes the played game's partner
  (`kRunsMonsters`), so a map never loses its monsters.
- Data one game's rules need from the other's is header-only (a roster, a pose table), so either
  rules library reads it without linking the other's code. Strings a native program adds for the
  other game's things go after its own list, built from that data rather than typed again.

**Menus and text**
- Every game's big menu lettering is cut into an alphabet of its own, one picture per character,
  from the player's pack at load (the first game's carved letters, `menu/carved_letters.hpp`;
  the sequel's plate letters, `menu/plate_letters.hpp`), with the letters a pack lacks pieced
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

- **The previous game is the template**: the next game's file sits beside the sequel's and has
  the same shape (§ the checklist above).
- **Our rules are C++**, on the rules runner. The VMs only run community mods' bytecode.
- **Clean room**: source this tree may not copy is read only by a reader subagent
  (`clean-room` skill).
- **No product names** outside `porting/`: say the first classic game, the sequel, the next game.
- **Labs use real maps and models** from the player's packs, never test maps or test models.
