# The nQuake and nQuake2 installs, as a client started in them must read them

What a finished nQuake install (QuakeWorld) and nQuake2 install (Quake 2) look like on disk,
and how their bundled clients find files and run configs, so that the Shooter's Edge client
placed in (or above) such an install behaves as the bundled client does with the same files.
Written by a clean-room READER (the `clean-room` skill) on 2026-11-12; no code. Each fact is
marked **[read]**, **[measured]** (counted or sized from the unpacked packages) or
**[derived]** (follows from read facts).

**Sources read.** nQuake's legacy installers (Windows NSIS 2.9, last change 2022-10-31; Linux
bash, 2025-02-20), nQuake's package tree "distfiles" (2026-09-21, with its manifest), the
current nQuake installer "nquake-reborn" (a web page and desktop shell, 2026-09-23) and its
upstream-binary list (2026-09-20); the ezQuake 3.6.9 release source (2026-03-01), the client
the current installer installs by default (the bundled fallback Windows build is 3.6.6
**[measured]**); nQuake2's installers (Windows v1.5, 2014-03-16; Linux v1.0, 2014-03-17); the
Q2PRO user manual; and, in a second session (2026-11-12), the Q2PRO source at commit
601a8df8433b0c50dbbe37c0716c3793fff140a7 (2025-12-11, github.com/q2pro/q2pro, the project's
new home) with its manual. nQuake2's content zips are no longer reachable, so its file
inventory stays open. No game pak was opened.

## nQuake: the install

**Layout** **[read, measured]**. One root folder (legacy defaults `C:\nQuake`, `~/nquake`).
Every distfiles package is a top-level folder copied verbatim into the root.

| Path (exact case) | What it holds |
| --- | --- |
| `ezquake.exe` / `ezQuake-x86_64.AppImage` / `ezQuake.app/` | The client, in the root (Windows / Linux / macOS). |
| `start_ezquake.sh` | Linux and macOS only: changes to its own folder, makes the client executable (macOS: clears quarantine), starts it. |
| `id1/` | `pak0.pak` (shareware, 18,689,235 bytes, or the player's), `pak1.pak` when registered; otherwise `gpl_maps.pk3` (free `dm1`-`dm6`, `e2m2`, `e3m7`, `end`, `b_exbox2` maps and a few models and sounds) and `readme.txt`. |
| `qw/` | The QuakeWorld game folder and nQuake's content (below). |
| `ezquake/` | The client's folder: `ezquake.pk3`, `configs/` (the player's configs), `gnu.txt`. |
| `fortress/`, `arena/`, `prox/` | Optional add-ons (Team Fortress; Clan Arena). |
| `ktx/`, `qtv/`, `qwfwd/`, `mvdsv` | Server installs only. |
| `readme.txt`, `LICENSE` | nQuake's FAQ; licence. |

**The player's own files** **[read]**: configs in `ezquake/configs/` (`config.cfg` saved on
quit, `<name>.cfg` from `cfg_save <name>`, the installer's `preset.cfg`); demos
`qw/matchinfo/demos/`, screenshots `qw/matchinfo/screenshots/`, logs `qw/matchinfo/logs/`,
captures `qw/capture/` (nQuake's values of `demo_dir`, `sshot_dir`, `log_dir`,
`demo_capture_dir`, relative to the root while `cl_mediaroot` is 0; the client's own defaults
are empty, meaning `qw/`). With `cfg_use_home 1` configs go to the home folder (below).

**File kinds by folder** **[measured]**:
- `qw/nquake.pk3`: `configs/ctf.cfg`, `demo.cfg`, `dm.cfg`, `spec.cfg`; menu and console
  pictures `gfx/*.jpg|tga` (`conback`, `loading`, `mainmenu`, `mcharset`, `menudot1..6`,
  `box_*`, `weaplist*`, ...); `lits/*.lit` for 10 maps; `progs/` replacement `.mdl` and `.md3`
  with `.png` skins; level shots, scroll bars, tracker icons.
- `qw/models.pk3`: replacement `progs/*.mdl` (32), `.md3` (3), `textures/models/*.png`.
- `qw/textures.pk3` (default on): 24-bit world textures, mostly `.jpg`, in `textures/`,
  `textures/bmodels/`, `textures/models/` and per-map folders `textures/<map>/`; `_luma`
  twins for fullbright layers; `*` in texture names stored as `#`.
- `qw/qrp_*.pk3` (optional, 404 MB): the same scheme as `.png`.
- `qw/ktx.pk3`: 381 `locs/*.loc`, bot and race data, KTX configs and models.
- `qw/scoreboard_flags.pk3`: 61 `textures/scoreboard/flags/*.png`.
- Loose in `qw/`: `autoexec.cfg`, `nquake_default.cfg`; `maps/` (62 `.bsp`, 29 `.txt` notes,
  15 `.ent` entity replacements); `env/` sky boxes (7 boxes; `doom` named `doomrt.png`, the
  rest `<name>_rt.jpg`); `crosshairs/1..20` (`.png`, `.tga`); `skins/` (60 `.png`); `textures/charsets/1..21.png`; `textures/wad/` (113 HUD pictures); `textures/bmodels/`,
  `textures/models/`; `sound/ca/*.wav`; `fragfile.dat`; sample demos (`.mvd`, `.qwd`).
- `ezquake/ezquake.pk3`: optional config presets `cfg/*.cfg` (graphics, HUD, teamplay),
  help XML, 10 `keymaps/*.kmap`, 372 `locs/*.loc`, charsets `povo5` (the default) and
  `moondid2`, 100 level shots, effect pictures, sky layer pictures `textures/sky1_alpha.png`
  and friends, `gfx/conchars-cyr.lmp`, `radars/*.png`, `sb/sources.txt`.
- Add-ons: `fortress/` (`pak0.pak`, `pak1.pak`, `misc.pak`, `default.cfg`, maps); `arena/arena.pk3`; `prox/prox.pk3` and `prox/configs/config.cfg`.

**Shipped configs** **[read]**:
- `ezquake/configs/config.cfg`: only sets the user variable `_nquake_first_startup` to 1.
- `ezquake/configs/preset.cfg` (installer-written): `name`, `cl_fakename` (current
  installer), `m_pitch` ±0.022, binds for `+forward`, `+back`, `+moveleft`, `+moveright`,
  `+jump`.
- `qw/autoexec.cfg`: runs `configs/config.cfg`; on the first start also `nquake_default.cfg`
  and `configs/preset.cfg`, then clears the flag; about 160 help, graphics-preset, demo and
  list aliases.
- `qw/nquake_default.cfg`: a full saved config: 102 binds, 29 aliases, about 1,630
  settings, of which 1,365 are `hud_*`. Notable values: `cfg_save_onquit 1`,
  `cfg_use_gamedir 1`, `cfg_legacy_exec 0`, `cl_onload sb_refresh`, `gl_consolefont povo5`,
  `crosshairimage 10`.
- The mode configs in `nquake.pk3` are not run: the aliases that used to run them on
  entering a server are commented out.
- `platform.cfg` (Linux, macOS) is not run by anything shipped **[derived]**.

**Shareware versus registered** **[read]**: the legacy installers accept a `pak0.pak` only at
18,689,235 bytes and a `pak1.pak` only at 34,257,856 bytes (Linux: any-case `pak1.pak` whose
size rounds up to 33 MiB); the current installer requires exactly 34,257,856. The registered
pack goes to `id1/pak1.pak`, and `id1/gpl_maps.pk3` and `id1/readme.txt` are then removed or
left out. No digest check on the player's pack. The shareware release's `ID1/PAK0.PAK` is
always renamed `id1/pak0.pak`.

**Browser installs on Windows** **[read]**: a browser there cannot create `.cfg` files, so
every client config is packed into `id1/configs.pk3` with its first folder dropped
(`ezquake/configs/config.cfg` becomes `configs/config.cfg`, `qw/autoexec.cfg` becomes
`autoexec.cfg`), and a mod's into `<mod>/configs.pk3`.

## ezQuake: where the root is, and the search path

**The root** **[read]**: the working folder, or `-basedir <path>`; accepted when `gfx.wad`
is found through the search path. Otherwise the executable's folder (Windows, Linux); on
macOS a remembered folder or one the player picks ("containing id1/pak0.pak"; `sys_forget_sandbox` forgets it). No parent-folder search exists.

**Home folder** **[read]**: `<Documents>\ezQuake` on Windows, `$HOME/.ezquake` elsewhere; `-nohome` disables it.

**Search path, highest first** **[read]**: a `-userdir` folder (only with `-game`), then
`<home>/<mod>`, then `<root>/<mod>` (both with `-game <mod>` or `+gamedir <mod>`, or after a
server names a game folder), then `-data` folders, then `<home>`, then `<root>/qw`,
`<root>/ezquake`, and `<root>/id1` last. The game folder is `qw` when none is named.

**Within one folder, highest first** **[read]**: packs named in `pak.lst`, `*.pk4`, `*.pk3`
(later in the folder listing ranks higher), other `*.pak`, numbered packs from the highest
(`pak1.pak` over `pak0.pak`), loose files last. A `pak.lst` turns the `*.pak`/`*.pk3` scan
off. **[derived]** On Windows the listing is sorted, so the alphabetically last `.pk3` wins.

**Letter case** **[read]**: `id1` is used when it exists, otherwise `ID1`. Numbered packs are
tried as `pak<N>.pak`, then `PAK<N>.PAK`, from 0 to the first gap. Other packs match
`*.pak`/`*.pk3` case-sensitively on every platform (`X.PK3` is ignored). Names in a pk3 match
ignoring case; names in a `.pak` and loose files must match exactly (so on Linux a loose
`MAPS/DM6.BSP` is not found **[derived]**). Names starting with `.` are never listed.

## ezQuake: the startup config order

**[read]**, with **[derived]** where marked. Defaults: `cfg_use_home` 0, `cfg_use_gamedir` 0,
`cfg_legacy_exec` 1, `cfg_save_onquit` 0, `cfg_backup` 0.

1. `default.cfg`, the copy that wins in the search path, unless that copy is the one inside
   `<root>/id1/pak0.pak` or inside `<root>/qw/nquake.pk3`. A loose or other packed
   `default.cfg` (for example `fortress/default.cfg` under `-game fortress`) is run.
2. The main config: `config.cfg`, or `+cfg_load <name>` from the command line. At startup the
   home folder is tried first whatever `cfg_use_home` says: `<home>/<name>`, then
   `<root>/ezquake/configs/<name>`. With `cfg_use_gamedir 1` and a game folder other than `qw`
   the `<home>/<gamedir>/<name>` and `<root>/<gamedir>/configs/<name>` forms are tried first,
   but at startup that setting still has its default **[derived]**. Read from disk only, never
   from a pack. When found: settings, aliases and binds are reset; `default.cfg` runs again
   (rule 1); then `autoexec.cfg` (any copy in the search path) runs, **before** the config's
   text **[derived]**; then the config; then the trigger `f_cfgload`. When not found: a console
   message, and nothing of this step.
3. Command-line `+set name value` pairs, in order.
4. The client starts its subsystems.
5. `autoexec.cfg` again.
6. The other `+` commands, in order.
7. A demo or QTV file given as first argument, else `cl_startupdemo` / `cl_onload`.

Every config runs with `con_bindphysical` 1 for its duration. **Saving** (`cfg_save`; on quit
with `cfg_save_onquit 1`): `<root>/ezquake/configs/<name>`; `<root>/<gamedir>/configs/<name>`
with `cfg_use_gamedir 1` outside `qw`; `<home>/<name>` or `<home>/<gamedir>/<name>` with
`cfg_use_home 1`; `cfg_backup 1` keeps `<name>.bak`. **On a server's game folder change**,
`cfg_legacy_exec` 1 or more runs `<root>/<gamedir>/config.cfg` and `frontend.cfg` from disk
(nQuake sets 0).

**nQuake's first start** **[derived]**: the queued `autoexec.cfg` runs `configs/config.cfg`,
`nquake_default.cfg` and `preset.cfg`; then the main `config.cfg` runs again; at step 5
`autoexec.cfg` applies the defaults and preset a second time. The merged result is saved to
`ezquake/configs/config.cfg` on quit. Under `-game prox`, `autoexec.cfg`'s
`exec configs/config.cfg` reaches `prox/configs/config.cfg` through the search path. On a
browser-made Windows install the first start finds no `config.cfg` on disk for step 2, but the
packed copy still runs through `autoexec.cfg`.

## ezQuake: paths and extensions per kind

**Any picture** **[read]**: the requested path loses its extension and every `*` becomes `#`.
First `<path>.link` (one line naming a picture under `textures/`), else `.tga`, `.png`,
`.jpg`, `.pcx` in that order. The first extension found anywhere wins, except that a loose
file of a later extension replaces a packed file of an earlier one.

- **World textures** (`gl_externalTextures_world`, `gl_externalTextures_bmodels`):
  `textures/<map>/<name>`, `textures/<mapgroup>/<name>`, a checksum-recognised alias under
  `textures/`, then `textures/<name>`. Brush entities start at `textures/bmodels/<name>`.
  Fullbright layer: the matched path plus `_luma` (not on liquids).
- **Sky boxes**: the name is `r_skyname`, else the world entity's `sky` (or `_sky`) key; a
  `skygroup` overrides; a dotted name is refused. Faces `env/<name><suf>`,
  `gfx/env/<name><suf>`, `env/<name>_<suf>`, `gfx/env/<name>_<suf>`, suffixes
  `rt bk lf ft up dn`. The world entity may also carry `fog` (density r g b), `skyfog`,
  `wateralpha`, `lavaalpha`, `slimealpha`, `telealpha`.
- **`.lit`** (`gl_loadlitfiles`: 0 off, 1 inline and file (default), 2 file only, 3 inline
  only): `maps/lits/<map>.lit`, `maps/<map>.lit`, `lits/<map>.lit`, then for a player's map
  group `maps/<group>.lit`, `lits/<group>.lit`. Inline means the BSPX `RGBLIGHTING` or
  `LIGHTING_E5BGR9` lump; a valid `.lit` file overrides it.
- **`.loc`**: `locs/<map>.loc`, then `locs/<group>.loc`; loaded with `tp_loadlocs` 1 (default)
  in deathmatch, always in demos.
- **Player skins**: `skins/<skin>.tga|png|jpg` (`gl_no24bit` 0), then `skins/<skin>.pcx` (at
  most 320 by 200); failing that the same for `baseskin` (default `base`).
- **Models**: `progs/<x>.md3` before `progs/<x>.mdl` (also for `maps/b_*.bsp`). Skins
  `textures/models/<model>_<skin>[_<frame>]`, then `textures/<model>_<skin>`, with `_luma`.
- **HUD pictures** from `gfx.wad`: `textures/wad/<name>`, then `gfx/<name>`, then the wad lump.
  Console background `gfx/<scr_conpicture>` (default `conback`). Crosshairs
  `crosshairs/<crosshairimage>`.
- **Charsets** (`gl_consolefont`, default `povo5`): `textures/charsets/<name>`, extra pages
  `<name>-001`... and `<name>-cyr`; `original` is the wad's `conchars`; a 16 by 16 grid.

## nQuake2 (Quake 2)

**What it is** **[read]**: nQuake2 by the nQuake author, last released 2014 (Windows v1.5,
Linux v1.0), client **Q2PRO**, plus Action Quake 2 and add-ons CTF, Eraser bot and
high-resolution textures. Effectively dead: its download index host no longer resolves.

**Layout** **[read]**: root `q2pro.exe` (Linux `q2pro`), `qsb.exe` (server browser); lower
case folders `baseq2/` (`pak0.pak`, the 3.20 point release's files, `gamex86.dll` or
`game*.so`, `q2pro.menu`), `action/`, `ctf/`, `eraser/` (with the CTF pack copied as
`ctf.pak`). Defaults `C:\nQuake2`, `~/nquake2`. Each of `baseq2/`, `action/`, `ctf/`,
`eraser/` gets an installer-written `q2config.cfg` (`name`, `m_pitch`, binds `+forward`,
`+back`, `+moveleft`, `+moveright`, `+moveup`, `+movedown`). File inventory: open.

**Full versus demo** **[read]**: a player's `pak0.pak` is accepted only at 183,997,730 bytes
(Linux: any-case `pak0.pak` rounding up to 176 MiB) and goes to `baseq2/pak0.pak`; otherwise
the 3.14 demo's pack. The 3.20 point release is always installed.

**Q2PRO, from its manual only** **[read]**: loose files, `.pak` and `.pkz` (zip) packs; the
user folder `~/.q2pro/` outranks the data folder and holds saves, screenshots, demos and
downloads; game libraries never load from it; `+` commands on the command line override the
configs. Replacement textures: `r_override_textures 1` (default) swaps a `.wal`/`.pcx` for a
true-colour file of the same path, extensions in `r_texture_formats` order (default
`png jpg tga`; the one-letter form `pjt` is also accepted). Model format is sniffed from content. `cl_noskins` 0/1/2. Locations:
`locs/<map>.loc`, one `loc` command with sub-commands (the names `loc_add`, `loc_delete`,
`loc_update`, `loc_write` do not exist in the source read: `loc-editing.md` § 7), `loc_dist`
(500 world units), `loc_trace`, `loc_draw`, macros `loc_here`, `loc_there`. Map light is already
coloured; `gl_coloredlightmaps` 1 keeps it.

## Q2PRO: the startup config order (from the source)

Read in the source unless marked. "Loose" means a file on disk; a copy inside a pack is
ignored for that step.

1. Unix only: `/etc/default/q2pro`, read raw from disk before anything else and run if not
   empty ("Execing /etc/default/q2pro"). Windows has none.
2. The command line's `+set name value` pairs, first pass. This is the only place `basedir`,
   `homedir` and `game` can be set. A `+set` missing its value prints "Usage: +set <variable>
   <value>" and cuts the command line there.
3. The filesystem starts (the search path below).
4. `default.cfg`: the first match on the search path. Packs are allowed.
5. The settings file: `config.cfg` on Linux and macOS, **`q2config.cfg` on Windows**. Loose,
   the first match on the search path.
6. `autoexec.cfg` from `baseq2`: loose, and only when a mod is set.
7. `autoexec.cfg` from the mod: loose. With no mod set, it comes from `baseq2`, so it runs once.
8. `postexec.cfg`: loose, the first match on the search path.
9. The `+set` pairs again, so they beat every config. They are then removed.
10. Subsystems start. The menu script `q2pro.menu` is read through the search path, packs
   allowed.
11. The other `+` commands. Each `+` starts a command and the words up to the next `+` are its
   arguments. With none given, the alias `client_start` runs if the player defined it.
12. `postinit.cfg`: loose, the first match. It runs after the `+` commands.

How each file is handled:
- A missing file is skipped silently.
- Other failures warn "Couldn't exec <name>: <reason>": a zero byte in the file, more than
  65,535 bytes after comments are stripped, or more than 16 nested execs.
- A success prints "Execing <name>" and runs the file at once.

**Saved at exit** to the same settings file in the **write folder**: the user folder's game
folder when `homedir` is set, else the base folder's. That folder is the mod's when a mod is
active, else `baseq2`. The file is written at shutdown and just before the game folder
changes. Its first line is "// generated by q2pro, do not modify", then the bindings, then the
archived settings. It is the file read back next start [derived].

**On Linux, Q2PRO looks for `config.cfg`**, so nQuake2's installer-written `q2config.cfg` is
read only on Windows [derived].

**A mod on the command line** (`+set game <mod>`) is in force before step 3, so steps 4, 5 and 8
find the mod's files before `baseq2`'s, and step 6 adds `baseq2`'s `autoexec.cfg` before the
mod's. `game` set to `baseq2` (any case) means no mod. A value outside letters, digits, `_` and
`-` is refused with "'game' should contain characters [A-Za-z0-9_-] only.".

**A game change later** (a local server starting, a server's connection data naming a mod, or
the player while disconnected) works like this:
- The settings file is first written into the old folder.
- The search path is rebuilt.
- If `fs_autoexec` is 1 (the default), steps 4 to 8 run again, searching the mod's folders only
  (`baseq2`'s when the new value is no mod). The exception is that step 6 still reads
  `baseq2`'s `autoexec.cfg`.
- A mod without its own settings file then reads none [derived].

## Q2PRO: the search path and letter case (from the source)

**The roots.**
- Windows: the base folder is the executable's folder (the process changes into it). There is
  no user folder (`homedir` is empty).
- Linux and macOS, default build ("system-wide"): the base folder is `<prefix>/share/q2pro`
  and the user folder is `~/.q2pro` (none when `HOME` is unset or empty).
- Linux and macOS, portable build: the base folder is `.` (the current directory) and there is
  no user folder.
- The game folder's name is a build option, by default `baseq2`.

**Searched first to last** [read: the order added; derived: the order searched]:
1. user `<mod>`: packs, then loose files,
2. base `<mod>`: packs, then loose files (skipped if missing, but only when a user folder
   exists),
3. user `baseq2`: packs, then loose files,
4. `<user>/Q2Game.kpf`, a zip at the root,
5. base `baseq2`: packs, then loose files,
6. `<base>/Q2Game.kpf`.

**Packs in one folder.**
- A pack is a `.pak` or `.pkz` (zip) file directly in the folder: the ending in any case, no
  subfolders, no names starting with `.`.
- They are sorted with names starting `pak` (any case) first, by the number after `pak` (no
  digits counts as 0), then by name. All other names follow, alphabetically without case.
- They are searched in reverse of that sort, so the last one wins: `pak1` beats `pak0`, and
  `zz.pkz` beats every `pakN`.
- **A folder's packs are searched before its own loose files.**
- A pack that will not open prints "Couldn't load <path>: <reason>" and is skipped.
- A zip with bytes in front (self-extracting) is read, with a warning.

**Letter case.**
- Names inside packs match in any case on every platform.
- Loose files on Linux and macOS: the path is tried as asked. If that fails and the path has an
  upper-case letter, it is tried once more fully lower-cased (below the game folder). So a
  file stored in upper case is never found by a lower-case request [derived].
- Folder names are used as spelled: `baseq2` is literal lower case, and a mod folder is as
  typed. So a `BASEQ2` folder is not found on Linux [derived].
- `PAK0.PAK` is found and sorts as a numbered pak.
- `Q2Game.kpf` must have that exact case [derived].
- macOS runs the Linux code. Its file system usually ignores case anyway (derived from the
  platform).

**Writes** (configs, demos, screenshots, `.loc`, downloads) go to the write folder above.

## Q2PRO: sky, extra map files, skins, replacement images (from the source)

**Sky.**
- The world keys are `sky` (default `unit1_`), `skyrotate` (degrees per second, default 0) and
  `skyaxis` (default `0 0 0`).
- An axis shorter than 0.001 means no rotation, so by default there is none.
- With the extended protocol the rotate string may add a second number, autorotate (default 1).
  With 0, the sky is turned by a fixed `skyrotate` degrees.
- The faces are `env/<sky><suffix>.tga`, with suffixes in the order `rt lf bk ft up dn`. All six
  must load, or the sky is black. Faces must be square.
- Extensions tried for a face: `.tga`, then `.png`, `.jpg`, then `.pcx` (by default).
- `gl_drawsky 0` draws no sky.
- The command `sky <basename> [rotate] [x y z]` overrides the sky for this map (the axis
  defaults to `0 0 1`). An empty name restores the server's sky.
- With `gl_cubemaps 1` (default 0), one image `sky/<sky>.tga` is tried first. Its face layout
  (a cross, a row, a column or a double row or column) is chosen by its aspect ratio.

**Files beside a map.**
- There is no `.lit`. Light is the BSP's own coloured lightmap, plus optional BSPX lumps inside
  the file.
- `locs/<map>.loc` (the client's place names).
- `textures/<name>.mat`, one footstep-material word per wall texture.
- `<texture or skin path>_glow.pcx` glow maps (`r_glowmaps` 1, default).
- Server side, only when `map_override_path` is set (default empty):
  - `<path>/<map>.bsp.override`, binary, 32-bit little-endian fields: a bit mask (1 an alias
   name, 2 a checksum, 4 an entity text), then a 64-byte name, then a checksum, then a length
   and the entity text, each only when its bit is set,
  - then `<path>/<map>@<hhhh>.ent` (4 lower-case hex digits of the 16-bit CRC-CCITT, initial
   0xFFFF, over the map's entity text without its last byte),
  - then `<path>/<map>.ent`. These replace the map's entities.

**Skins.**
- The skin string is `model/skin` (`\` also accepted). An empty model means `male`. Names must
  be letters, digits, `_` or `-`.
- An invalid skin becomes `female/athena` for `female` and `male/grunt` otherwise. An invalid
  model becomes `male/grunt`.
- `cl_noskins`: 1 makes everyone `male/grunt`. 2 allows `female/athena` as well.
- Files tried, in order:
  1. `players/<model>/tris.md2`, falling back to the `male` model,
  2. `players/<model>/<skin>.pcx`,
  3. `players/female/athena.pcx` (for `female`),
  4. switch to `male` and try `players/male/<skin>.pcx`,
  5. `players/male/grunt.pcx`,
  6. the weapons `players/<model>/<file>` (`cyborg` falls back to `male`'s),
  7. the icon `players/<model>/<skin>_i.pcx`.
- If the skin, the icon, the model or the first weapon is still missing, the player is drawn as
  `male/grunt`.
- Each `.pcx` is sought first as `.png`, `.jpg`, `.tga` (by default).

**Replacement images.**
- `r_override_textures`: 0, 1 (the default, for 8-bit requests only) or 2 (for all requests).
- `r_texture_formats` (default `png jpg tga`).
- `r_texture_overrides`, a bit mask (default -1): 1 pictures, 2 fonts, 4 skins, 8 sprites,
  16 walls, 32 sky.
- When replacing, the true-colour formats are tried in order, then `.wal` for walls and `.pcx`
  for everything else. A replacement keeps the 8-bit original's size for scaling.
- Wall texture: `textures/<name>.wal`, by default found as `.png`, `.jpg`, `.tga`, then `.wal`.
- HUD picture: `pics/<name>.pcx` (a leading `/` means a path from the game folder's root).
- Console font: `con_font` (default `conchars`), giving `pics/conchars.pcx`. A failing
  non-default font resets to the default.
- `scr_font` (default `conchars`) is the HUD text font.

## nQuake2: the content zips

Still **open**. The content host `nquake2.com` no longer resolves, the Internet Archive has no
copy of its zips, SourceForge's `nquake2` project holds only the installers, and
`nquake.com/2/` is 404 (all measured 2026-11-12). The zip names, read from the installers:
- `q2-314-demo-x86.zip`
- `q2-3.20-x86-full_3.zip`
- `nquake2-gpl.zip`
- `nquake2-non-gpl.zip`
- `nquake2-linux.zip`
- `nquake2-addon-ctf.zip`
- `nquake2-addon-eraser.zip`
- `nquake2-addon-textures.zip`
