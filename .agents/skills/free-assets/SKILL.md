---
name: free-assets
description: "Use when a game needs sounds or models it does not have and someone else has made them free: the free sources (Kenney, OpenGameArt, Poly Haven, Quaternius and others), which licences this repository can carry, how to fetch and unpack each source, finding what a game needs (Shooter's Edge's registry from the classic content's maps and game logic), picking a replacement without listening (the match on length, bands and envelope), converting and renaming it to our name, the classic.assetmap the map cloner and the game read, CREDITS.md, and the pain points the `ge` tools for all of this should remove."
---

# Free assets

A game here makes most of what it shows in code and its sounds on the synth. Where a game needs
recorded sounds or authored models (Shooter's Edge first: it must answer every sound and model
the classic content names), they come from people who gave them away. This skill is where to
find them, which ones the repository may carry, and how one becomes ours.

## The licence rule

The repository is public and under the PolyForm Noncommercial licence, with commercial use kept
for its owner. So an asset we COMMIT must allow redistribution, modification and commercial
use, with nothing that binds the code around it:

| Licence | Commit? | Why |
| --- | --- | --- |
| CC0 1.0, public domain | Yes, first choice | No conditions. Credit anyway in CREDITS.md |
| CC-BY 3.0/4.0, OGA-BY | Yes, with its credit line | Attribution only; the credit row is the condition |
| BSD, MIT, zlib (content packs under a code licence) | Yes, with the notice | Keep the copyright notice in CREDITS.md |
| CC-BY-SA, GPL, LGPL | No | Share-alike would bind the derived asset (and argued over, the game) to a licence this repository cannot take |
| Any NC (noncommercial) licence | No | The owner's commercial use is reserved; an NC asset forbids it |
| Any ND (no derivatives) | No | Converting, trimming and renaming are derivatives |
| "Royalty-free" bundles (the yearly game-audio giveaways and the like) | No, fetch only | Use in a game is allowed, redistributing the raw files is not, and a public repository is redistribution |
| Shareware or retail game data | Never | Read it for facts in scratch; never commit, never publish its audio or images |

A pack whose page doubts its own provenance (a comment asking where the recordings came from, a
"found it on my computer") is skipped even when it says CC0.

## The sources

Check each pack's own page every time; a site's default licence is not a pack's.

| Source | Licence | What it is good for | How to fetch |
| --- | --- | --- | --- |
| Kenney (kenney.nl/assets) | CC0, all of it | Low-poly kits in GLB with previews (weapons, crates, props, dungeon and station pieces, small animated characters), UI, impact, sci-fi, foley and announcer sound packs in OGG | The site's search ignores its query: list the catalogue with `https://kenney.nl/assets/page:<n>` (about 30 pages) and grep `kenney.nl/assets/<slug>`. The zip is the one `https://kenney.nl/media/pages/assets/<slug>/<hash>/kenney_<slug>.zip` link on the pack's page. Models: `Models/GLB format/*.glb` reference `Textures/colormap.png` OUTSIDE the file (see Models). Previews: `Previews/<name>.png` |
| OpenGameArt (opengameart.org) | Per submission: filter to CC0 | Sound packs of every kind (search "CC0" and a subject), creature voices, weapons, water, ambient loops | Search `https://opengameart.org/art-search-advanced?keys=<words>&field_art_type_tid%5B%5D=13&field_art_licenses_tid%5B%5D=4` (13 sound effects, 4 CC0; 10 is 3D art). On a page, the licence is in `license-name'>…`, the author in the submitter field, the files are the `https://opengameart.org/sites/default/files/…` links. Read the comments for provenance doubts |
| Poly Haven (polyhaven.com) | CC0 | Photoscanned props (barrels, crates, medical boxes, lamps), HDRI skies, PBR textures | An API: `https://api.polyhaven.com/assets?t=models`, then `https://api.polyhaven.com/files/<id>` names a glTF per texture size with its includes. Heavy: a 1k prop is about 2 MB with three textures; commit only a pick, at 512 px or 1k |
| Quaternius (quaternius.com) | CC0 (one licence is QAL: read it) | Animated low-poly characters, monsters, guns, sci-fi kits | Each pack page links a shared-drive folder, not a file: not fetchable headless here. Ask the user to drop the zip in the project's uploads, or skip |
| Freesound (freesound.org) | Per sound: CC0, CC-BY or CC-BY-NC | The largest sound library | Blocked from the cloud container and its API needs a key: skip here, or ask the user for the files |
| ambientCG | CC0 | PBR textures and some HDRIs | `https://ambientcg.com/api/v2/full_json` |
| The BSD-3 free replacement for the classic game data | BSD-3-Clause | Every classic sound and model by the classic name | Named in `porting/shooters-edge/survey.md`; the labs fetch it into their cache by hash, never commit it, since its name may not appear outside `porting/` |

Unpacking needs `unzip`, `7z` (`apt-get install p7zip-full`) and for old self-extractors
`lhasa`; `ffmpeg` reads every audio format met so far (wav, ogg, flac, mp3).

## The procedure (Shooter's Edge)

Everything lives in `games/shooters_edge/content/`:

| File | What |
| --- | --- |
| `sounds.tsv`, `models.tsv` | THE REGISTRY: one row a sound or model the game needs, by OUR name, with what it is, what plays or shows it, its scope (`game` or `creature`), its status (`replaced`, `open`, or `skip` when no multiplayer code plays or shows it: the game is multiplayer only, so creature content and single-player-only content are listed but not replaced) and its source pack's id |
| `classic.assetmap` | The classic name each of ours stands in for, read by the map cloner (`rooms::parse_asset_map`) and by the game: `sound <classic path> <our name>`, `model <classic path> <our name>`, and `set <classname> <key>=<value> <field> <our name>` for the numbered sound sets a map picks by number (doors, buttons, lifts, trains, triggers) or by worldspawn's `worldtype` (locks and keys). The same comment and `# pin` rules as a `.texmap` |
| `sounds/<our name>.wav` | Mono 16-bit PCM at 22 050 Hz, trimmed, loudness-normalised to -16 LUFS with -1.5 dB true peak |
| `models/<our name>.glb` | Binary glTF with every texture packed inside, loadable by `assets::load_gltf` |
| `CREDITS.md` | Every committed file: its pack, author, source URL, licence and the source file's name |

1. **Know what is needed.** The registry was built from the shareware data in scratch: every
   path in the pak, every entity key of every map (`noise*`, `sounds`, `model`), and every call
   in the game logic's bytecode that plays a sound (`sound`, `ambientsound`) or sets a model
   (`setmodel`, `.weaponmodel`, a `.noise*` field) with the function it sits in. The function
   names say the event; the registry says it again in our words and never keeps the classic
   identifiers. Plus what the client plays itself (ricochets, the explosion, menu clicks, the
   leaf ambients). A new content set gets the same treatment; `ge content scan` is the tool
   this wants (below).
2. **Name it ours.** `<group>/<name>`: groups `weapon`, `pickup`, `power`, `player`, `door`,
   `button`, `lift`, `mover`, `trigger`, `teleport`, `world`, `ambient`, `ui`, `common`,
   `creature/<kind>`; models `character`, `view`, `pickup`, `projectile`, `effect`, `prop`,
   `gore`, `creature`. Variants are `_1`, `_2`. A creature is named by what it does
   (`brute`, `leaper`, `hulk`), never by its classic name.
3. **Gather candidates** into scratch, one directory per pack, unpacked.
4. **Match without listening.** For a sound with a reference, rank a keyword pool of
   candidates by the distance in log length, in log energy over eight bands (0, 150, 400, 800,
   1 600, 3 200, 6 400, 11 025 Hz, normalised to the loudest band) and in the loudness envelope
   over ten slices. The top three were the right kind of sound nearly every time; pick by name
   among them. Without a reference (content outside the shareware set) choose by name and
   description. Say in the PR that the choice was not listened to, and publish an audition page
   (an Artifact with a player per sound) so the user can.
5. **Convert and rename**:
   `ffmpeg -i <src> -af "silenceremove=start_periods=1:start_threshold=-50dB,areverse,silenceremove=start_periods=1:start_threshold=-50dB,areverse,loudnorm=I=-16:TP=-1.5:LRA=11" -ac 1 -ar 22050 -c:a pcm_s16le sounds/<our name>.wav`;
   a loop is cut to at most 6–8 s with short fades. A model gets its external textures packed
   into the GLB's binary chunk (a `bufferView` and a `mimeType` per image): Kenney kits ALL
   name their texture `colormap.png` with different pixels, so a model renamed without packing
   draws with the wrong one or none.
6. **Record it**: the registry row's status and source, `classic.assetmap` unchanged unless a
   name moved, a CREDITS.md row. Nothing is committed without its credit.
7. **Budget**: keep the sounds under about 8 MB and a model under 300 KB unless the PR argues
   for it; prefer low-poly CC0 kits over photoscans.

## The `ge` tools this wants

Every step above was done by hand with throwaway scripts. Each is a `ge` command to build (the
wishes are in `docs/goals/ge-backlog.md`):

- `ge content scan <pak|dir>`: the registry and an assetmap draft from a content set's maps,
  game logic and archives (on M18a–c's readers).
- `ge asset source search|fetch <source>:<pack>`: a catalogue of the sources above, unpacked
  into a cache, with licence, author, URL and hash written to a lock file; a licence the table
  above refuses is refused.
- `ge sound match <reference> <candidates…>`: the ranking above, a reason per trait, a sheet of
  waveforms; `ge sound split <file>` cuts a long take at its silences.
- `ge sound import <source> <our name> [--loop]` and `ge model import <source> <our name>`:
  convert, pack, load-check, preview, and write the registry and CREDITS.md rows in one step.
- A check that every file under a game's `content/` has a CREDITS.md row.
