# Facts: Quake II client sound (Q2PRO upstream, commit 601a8df): OpenAL, footsteps, music, the sound page

Reader's notes, kept for the sound rows of Shooter's Edge's Quake II client. These are a clean-room reader's facts about the Q2PRO client at upstream commit
601a8df (December 2025), written in the reader's own words. They contain no code. The only names
quoted are settings, commands, file names and texts a player sees. **[read]** = read directly in
the source; **[derived]** = worked out from what was read (the arithmetic or the consequence is
shown); **[spec]** = a property of the OpenAL or FFmpeg library the client calls, not of Q2PRO
itself. A "developer line" is printed only when the `developer` setting is 1 or more (an extra
level, 2 or more, is called "developer 2" below); `developer` exists only in debug builds, so a
release build never prints these **[read]**.

Units: distances in map units; times in milliseconds or seconds as stated; gains are linear
factors (1 = unchanged).

---

## 1. Every `al_` setting and command, and settings only the OpenAL backend reads

There are exactly three `al_` settings and no `al_` commands **[read]**:

| Setting | Default | Values | Flags | What it does |
| --- | --- | --- | --- | --- |
| `al_device` | empty | any device name, or `?` | not saved by `writeconfig`; restarts sound when changed, but only once OpenAL has started successfully | Names the output device. Empty = the system's default device. `?` prints the device list (question 4), then resets the setting to empty and opens the default device. |
| `al_hrtf` | `0` | integer; 1 is special | not saved; restarts sound when changed, but only when OpenAL started AND the device offers the HRTF extension | 1 = leave HRTF to the OpenAL implementation's own default. Any other value asks for HRTF explicitly: more than 1 = on, 0 or less = off. Without the HRTF extension the setting is ignored silently. |
| `al_merge_looping` | `1` | 0, 1, 2 (integer) | not saved; takes effect at once, no restart | 0 or less = never merge, 1 = auto, 2 or more = always merge (question 3). Changing it stops every looping (entity) sound at once; they are rebuilt the next frame in the new mode. |

None is a cheat setting and none is latched in the sense of waiting for a map change **[read]**.
"Restarts sound" means the client queues the `snd_restart` command on its next sound update
(question 5) **[read]**.

Settings that only matter to the OpenAL backend: none besides the three above **[read]**. Shared
settings that the OpenAL backend reads in its own way **[read]**:

- `s_volume` becomes the OpenAL listener's gain, applied at once when changed, to every sound AND
  the music stream. It is passed through without clamping under OpenAL; the software mixer clamps
  it to 0..1.
- `s_underwater_gain_hf` (default 0.25) is clamped to 0.001..1 and becomes the high-frequency gain
  of an EFX low-pass filter (question 2), applied at once when changed.
- `s_underwater` (default 1) turns that filter on and off; it is checked every frame.

Settings that only the SOFTWARE mixer reads (so they do nothing under OpenAL) **[read]**:
`s_khz` (default 44, saved, restarts sound), `s_mixahead` (default 0.1, saved), `s_swapstereo`
(default 0), `s_driver` (default empty, restarts sound), `s_testsound` (default 0).

Not present at all in this release **[read]**: reverb or EAX environments, any other EFX effect,
occlusion, obstruction, doppler, air absorption, a distance-model setting, a pitch or time-scale
setting for sounds, a maximum-sources setting. The source count is fixed by the code (question 2).

---

## 2. What the OpenAL backend does that the software mixer does not

Beyond HRTF and the underwater filter, the audible differences are these. None has its own
setting unless stated.

**Reverb, environments, occlusion, doppler, pitch: none** **[read]**. The backend never creates
a reverb or any EFX effect slot; the only EFX object it makes is one low-pass filter. It never
sets a source or listener velocity, so OpenAL's doppler has nothing to work with [derived +
spec]. It never sets a source's pitch. Nothing traces geometry between a sound and the listener.

**Number of voices** **[read]**: the backend asks OpenAL for up to 64 sources for sound effects
(the same 64-channel limit as the software mixer) plus one more source for the music and
cinematic stream. If fewer than 16 effect sources can be had, OpenAL start-up fails with
"Insufficient number of AL sources" (question 4) and the client falls back (question 5).

**Distance attenuation** **[read]**: OpenAL is put in its "linear distance, clamped" model. Each
positioned sound gets a reference distance of 80 units, a maximum distance of 8192 units, and a
roll-off factor equal to the sound's distance multiplier times 8112 (8192 minus 80). The
distance multiplier is the same one the software mixer uses: the sound's attenuation times 0.0005
(attenuation 1 "normal" = 0.0005, 2 "idle" = 0.001), except that attenuation 3 ("static") uses
0.001 per unit of attenuation, that is 0.003.
- Consequence **[derived + spec]**: inside 80 units the gain is 1; from 80 to 8192 units the gain
  is 1 minus the multiplier times (distance minus 80), floored at 0. That is the same fall-off as
  the software mixer (which also starts at 80 units and falls linearly at the same rate). A
  normal sound reaches silence at 2080 units, an idle one at 1080, a static one at about 413.
  The only difference in fall-off is the 8192-unit clamp: beyond 8192 units OpenAL holds the
  gain at its 8192-unit value. That matters only for a multiplier below 1/8112 (about 0.000123),
  for example an entity's loop with a very small custom attenuation.

**Panning** **[read + spec]**: under OpenAL the position of each sound is given to OpenAL as a 3D
point (the listener's position and its forward and up directions are updated every frame), and
OpenAL's own panner (or HRTF) places it. The software mixer instead splits a sound between left
and right by the dot product with the listener's right vector (each side gets half of 1 plus or
minus that dot product). So the stereo image differs; the loudness fall-off does not.

**Sounds played at full volume** **[read]**: a sound whose entity is the listener's own entity,
a sound with no entity, and any sound with zero attenuation is played relative to the listener at
the listener's own position, with OpenAL Soft's spatialization switched off for it (when the
implementation offers that switch), so it is heard centred and unpanned. Every other sound is
explicitly marked for spatialization (again when offered), which also makes OpenAL Soft
spatialize a STEREO sound file as a point source rather than play its two channels as they are
[spec]. A sound tied to a moving entity is moved to the entity's current sound origin every
frame; a sound given a fixed origin stays there.

**Sounds with loop points** **[read]**: a sound file with a loop start (a WAV cue point) loops.
When the implementation offers OpenAL Soft's loop-points extension the loop runs from the loop
start to the sound's end, as in the software mixer; without the extension the whole file
loops from its first sample [derived]: the intro part repeats.

**Entity loops kept in step** **[read]**: when merging is off (question 3), each entity's looping
sound is its own source. A new one starts at the current sample position of an existing source
playing the same sound, so two copies of one loop are in phase rather than each starting from
its beginning.

**Sample formats** **[read]**: 8-bit and 16-bit mono or stereo files are uploaded as they are; 32-bit float files only when the implementation offers float buffers. A sound in another format
fails to load, and `soundlist` lists it as not loaded with the reason.

**The music stream** **[read]**: music and cinematic sound go to one extra source that is
listener-relative, has no distance roll-off and no spatialization, and is sent straight to the
output channels (OpenAL Soft's direct-channels extension, remixing channels the device lacks,
when offered). The underwater filter is applied to the 64 effect sources only, never to the
music. The stream's queue holds up to 32 buffers of up to 8192 sample frames each, so up to
about 5.5 seconds at 48 kHz can be queued ahead [derived: 32 × 8192 / 48000 = 5.46 s]. The music
is resampled to the device's own rate when the device reports one between 11025 and 48000 Hz.

**The underwater filter** **[read]**: when the player's view is under water (the frame's
underwater flag), the game is fully connected, and `s_underwater` is non-zero, every effect source
gets an EFX low-pass filter whose high-frequency gain is `s_underwater_gain_hf` (clamped
0.001..1). It needs the device's EFX extension; without it there is no underwater effect under
OpenAL and nothing is printed. The frequency at which the high-frequency gain applies is the
implementation's (5 kHz in the EFX specification) [spec].

**Focus** **[read]**: when the window loses the focus level `s_auto_focus` asks for, all effect
sounds stop and the music stream pauses; on regaining focus the stream resumes where it was.

**Clock wrap** **[read]**: the OpenAL backend measures time in milliseconds of the client's real
time, wrapped at 2^30 ms (about 12.4 days); at the wrap every playing sound is stopped once.

---

## 3. `al_merge_looping`: what merging does, and what "auto" tests

**What is merged** **[read]**: only entity loops (the sound an entity carries every frame, such as
a machine hum or a rocket's flight), never one-shot sounds. Every frame the client gathers the
loops of the entities in the frame, subject to `s_ambient` (none while paused, none when
`s_ambient` is 0, only entities with a model when it is 2, only the player's own entity when it
is 3). All entities carrying the SAME sound file collapse into ONE OpenAL source, however many
there are.

**How the merged source's gain and position are chosen** **[read]**: for each entity carrying the
sound, the client works out a left and a right gain exactly as the software mixer would: the
entity's loop volume (1 when the entity sets none) times the distance fall-off (1 inside 80
units, then falling linearly by the entity's loop distance multiplier per unit), split left and
right by the dot product with the listener's right vector, unless the entity asked for no stereo
(an extended-protocol render flag) or has zero attenuation, in which case both sides get the
full value. The lefts are summed and the rights are summed over all those entities; each sum is
then capped at 1. If both are 0 the sound is not played this frame. Then:

- The source's gain is the left sum plus the right sum (so between 0 and 2).
- The pan is the right sum minus the left sum, divided by their total (from -1 full left to +1
  full right).
- The source is placed relative to the listener, one unit away, on the circle in front of the
  listener: sideways offset equal to the pan, and forward by the square root of 1 minus the pan
  squared; height 0. Its distance roll-off is switched off, so only that placement and the gain
  shape what is heard.
- Gain and placement are recomputed every frame; the source keeps looping without restarting.
  [spec] OpenAL's default maximum source gain is 1, so a requested gain above 1 is held at 1 by
  the implementation unless it allows more.

The loop distance multiplier for an entity **[read]**: an entity loop with no stated attenuation,
or the "static" attenuation 3, uses 0.003 per unit; a stated positive attenuation other than 3
uses that attenuation times 0.0006; the special "no attenuation" value (-1) uses 0, which makes
the loop full-volume and unpanned.

**When not merged** **[read]**: each entity's loop is its own positioned source at the entity's
sound origin, with the distance model of question 2 and the entity's loop volume and multiplier,
kept in step with other copies of the same sound (question 2). Many copies then add up in OpenAL
with no cap, which is why overlapping loops can be louder than in the software mixer, and is the
problem merging solves.

**What "auto" (value 1) tests** **[read]**: at start-up the client asks the device for OpenAL
Soft's output-mode query. If the device offers that query AND reports any mode other than plain
(basic) stereo, such as HRTF stereo, a surround layout, UHJ or mono, then value 1 does NOT merge
and only value 2 or more does. If the device reports basic stereo, or does not offer the query at
all, value 1 merges. Value 0 or less never merges. The test is made once per sound start-up; a
change of `al_hrtf` restarts sound and so re-tests [derived].

---

## 4. Lines the OpenAL backend prints

Ordinary lines (every build) **[read]**:

- On success: `OpenAL initialized.`
- On failure (printed as an error line): `Failed to initialize OpenAL: ` followed by the reason.
  The reasons the client itself gives are:
  - `At least OpenAL 1.1 required`
  - `alcOpenDevice(<al_device value>) failed` (the value is empty when the default device was
  used, giving `alcOpenDevice() failed`)
  - `alcCreateContext failed`
  - `alcMakeContextCurrent failed`
  - `Insufficient number of AL sources`
  - When the OpenAL library cannot be loaded or lacks a required function, the reason is the
  operating system loader's own message (on Linux and macOS the dynamic loader's error text; on
  Windows a line naming the library and saying `LoadLibrary failed` or `GetProcAddress(...)
  failed` with the system's error).
- Libraries tried, in order: on Windows `soft_oal` then `openal32`; on macOS `libopenal.1.dylib`
  then `libopenal.dylib`; elsewhere `libopenal.so.1` then `libopenal.so`.
- With `al_device ?`: `Available devices:` then each device name on its own line, or
  `No devices available` when the list is empty. The list is the "all devices" list when the
  implementation offers it, otherwise the plain device list.
- When the EFX extension is advertised but a function in it cannot be found (error line):
  `Couldn't load extension ALC_EXT_EFX`. The underwater filter is then absent.
- On shutdown (including every `snd_restart`): `Shutting down OpenAL.`
- `soundinfo` under OpenAL prints four lines `AL_VENDOR: `, `AL_RENDERER: `, `AL_VERSION: `,
  `AL_EXTENSIONS: ` each followed by the implementation's string, then
  `Number of sources: <n>` (the effect sources obtained, at most 64). When sound is not started
  it prints `Sound system not started.`

Developer lines **[read]**: `Trying <library>` for each library tried; the vendor, renderer and
version lines as above at start-up; `Got <n> AL sources`; `Loaded extension ALC_EXT_EFX`. At
developer 2: the AL extension list, the ALC extension list, and the output mode as a hexadecimal
number.

Not printed **[read]**: nothing says whether HRTF is on, whether merging is on, or that the HRTF,
loop-points, spatialization, float or direct-channels extensions are missing; those are silent.

---

## 5. Changing `s_enable` at run time

- `s_enable` is not saved by `writeconfig` and is marked "restart sound when changed" **[read]**.
  A change made by the player (console, config, menu) is noticed on the next sound update, which
  queues the command `snd_restart` and does nothing else that frame **[read]**. So the change
  takes effect within a frame without the player typing anything **[derived]**.
- `snd_restart` is a client command, always registered **[read]**. It shuts the sound system
  down (stops every sound, frees every loaded sound, stops the music, closes the backend), starts
  it again, and re-registers the current map's sounds **[read]**. The music restarts from the
  start of its track, and a track started by hand with `ogg play` is dropped in favour of the
  automatic one [derived: the shutdown forgets the current track, and start-up starts the
  automatic one].
- What is printed **[read]**: under OpenAL `Shutting down OpenAL.` first. Then, for value 0 or
  less, only `Sound initialization disabled.`. Otherwise `------- S_Init -------`, the backend's
  lines (question 4; the software mixer prints `sound sampling rate: <Hz>`), and a closing line of
  22 dashes. If no backend starts, `Sound failed to initialize.` (an error line) comes before the
  dashes.
- **Fallback** **[read]**: value 2 (or more) tries OpenAL first; if OpenAL fails it prints the
  failure line and then tries the software mixer, which value 1 would have used. At the end of
  start-up the client writes the backend actually running back into `s_enable` (0 none, 1
  software, 2 OpenAL). So a failed OpenAL leaves `s_enable` reading 1, and a total failure leaves
  it reading 0. That write is made by the code and does not trigger another restart.
- Build edge case **[read]**: the numbers are positions in a list of the backends compiled in.
  In a build without the software mixer, OpenAL is value 1 and there is no value 2; in a build
  without OpenAL, value 2 simply means the software mixer and the setting then reads 1.
- Other settings that restart sound the same way: `s_khz`, `s_driver`, and `al_device` and
  `al_hrtf` while OpenAL is running (question 1) **[read]**.

---

## 6. `cl_footsteps` and per-surface footsteps

`cl_footsteps` defaults to 1 and is not saved by `writeconfig` **[read]**. Values: 0 = no
footsteps, 1 = per-surface sets when found, 2 = always the default set **[read]**.

**Yes, master has per-surface sets** **[read]**. How a surface gets a material:

- When a map loads, for each wall texture the client looks for a text file
  `textures/<texture name>.mat`. Its contents (at most 15 characters are read) are the material
  name. Textures with the same name (compared without case) share one result.
- A material name may contain only letters, digits, `_` and `-`. Anything else prints a warning
  `Bad material "<name>" in textures/<texture name>.mat` and the texture is treated as having
  no material.
- No file, an empty file, or the material `default` (any case) = the default set. The material
  `ladder` (any case) = the ladder set. Every other distinct material (compared without case)
  gets its own set.

Which sound files are tried **[read]**:

- Default set: `sound/player/step1.wav`, `sound/player/step2.wav`, and so on.
- Ladder set: `sound/player/steps/ladder1.wav`, `ladder2.wav`, and so on in that folder.
- A material's set: `sound/player/steps/<material>1.wav`, `<material>2.wav`, and so on.
- Numbering starts at 1 and stops at the first number whose file does not exist, with at most 15
  variants per set. The material name is used as written in the `.mat` file.
- Fallback: a set with no files at all uses the default set; if the default set is empty too, no
  sound is played.

**How the surface under a stepping entity is found** **[read]**: the client sweeps a flat box
(the entity's horizontal size, zero height) straight down from 1 unit above the entity's origin.
For an entity with a bounding box the sweep reaches 9 units further down than the box's bottom; for anything else it reaches 75 units below the start (9 plus 66, chosen to cover the largest
monster). This first sweep hits solids only. If it hits nothing, the default set is used. If it
hits a surface, that surface's material is taken, and then a second sweep from the same start to
1 unit above the hit point, this time hitting water too, replaces it with whatever it hits first
(so standing in shallow water gives the water surface's material). The search is skipped, and
the default set used, when the map has no materials beyond default and ladder, or when
`cl_footsteps` is 2 or more.

**Choice of variant** **[read]**: a variant is chosen at random (uniformly); if it is the same
sound as the last footstep played by anyone, the next variant in order (wrapping) is used instead.
So one sound is never played twice in a row, as long as the set has two or more.

**Which events play and how loud** **[read]**:

- The ordinary footstep event: volume 1, normal attenuation, surface looked up.
- The quieter footstep event for other entities (extended protocol only): volume 0.5, idle
  attenuation, surface looked up.
- The ladder-step event (extended protocol only): volume 0.5, idle attenuation, always the
  ladder set, with no surface search, even when `cl_footsteps` is 2.
- All use the footstep channel of the stepping entity, so a new step replaces that entity's
  previous one.

**Value 2 versus 1** **[read]**: with 2 every ground step uses the default set (no `.mat` lookup
under the feet); ladder steps still use the ladder set. 0 plays none.

**Telling the server** **[read]**: only on the Q2PRO protocol (36). The client sends a
client-setting message: the message byte 5, then the setting number 11 ("no footsteps") as a
16-bit value, then a 16-bit value of 1 when `cl_footsteps` is 0 and 0 otherwise. It is sent when
the setting changes and again each time the client sends `begin` after loading a map. A server
told "no footsteps" removes the footstep event from every entity it sends that client (and, on
the extended protocol, the quieter footstep and ladder-step events too), saving bandwidth; the
server's player listing marks such a client with `F`. On other protocols the server is not told
and the client simply does not play them [derived].

---

## 7. `ogg play <name>`, lookup and `ogg info`

**The track list** **[read]**: the client builds a list of music files when it starts and again
whenever the file system restarts (a game-folder change). It scans, in order:

1. the folder `music` inside each directory on the search path (loose directories only; pack
   and zip files are never used, on purpose, since packs can be downloaded from servers),
   recursively, in search-path order (the active game folder before the base folder); 2. then `music` in the user's home game directory, not recursively; 3. then `music` in the installation's base directory, not recursively.

On Unix-like systems, files whose names start with a dot are ignored. A file is kept when its extension is one of `.flac`,
`.opus`, `.ogg`, `.mp3`, `.wav` (compared without case) AND the FFmpeg build linked in can read
that container and decode that codec. Each file is keyed by its path below the `music` folder
without the extension (for example `track02`, or `quake2/track02` in a subfolder), compared
without case. When two files have the same key, the first one found wins: an earlier folder in
the list above beats a later one, and within one folder the names are sorted alphabetically
without case, so for the same base name the extension order is `.flac`, `.mp3`, `.ogg`,
`.opus`, `.wav` [derived from the sort].

**Looking a name up** **[read]**:

- A whole number (digits only): first remapped for mission packs when `ogg_remap_tracks` is on
  (numbers 2 to 11 in the `rogue` and `xatrix` game folders). Then the key `track` plus the number written with at least two digits (`track02`) is
  tried, then the bare two-digit number (`02`). Zero or a negative number finds nothing.
- Anything else: the name is normalised as a path; if it ends in one of the five extensions that
  extension is removed; the rest is looked up as a key, without case. So `ogg play Track02.OGG`
  finds a `track02.flac` if that is the file listed. A path into a subfolder of `music` works.
- When nothing matches: `No such track: <name>`. Missing argument: `Usage: ogg play <track>`.
  When sound is not started: `Sound system not started.` During a cinematic:
  `Can't play music in cinematic mode.`
- A third argument `soft` closes the current file but keeps the audio already buffered, so the
  new track follows without a gap; without it the buffered audio is dropped at once.
- A track started by hand plays once; when it ends and its buffered audio has run out, automatic
  playback (the level's track) resumes [read]. An automatic track loops (rewinds to its start)
  when `ogg_enable` is on and `ogg_shuffle` is off; with shuffle on, the next shuffled file plays.
- If the file cannot be opened or decoded, an error line is printed, for example
  `Couldn't open <path>: <reason>`, `Couldn't find audio stream`,
  `Failed to find audio codec <codec>`, `Failed to open audio codec`, `Bad filename: <path>`.

**`ogg info`** **[read]**: while a file is open it prints FFmpeg's standard description of the
file (FFmpeg's own wording, routed into the console with non-ASCII characters transliterated): a
line naming input 0, the container format and the file's full path, then its metadata tags, a
duration, start and bitrate line, and a line per stream giving the codec, sample rate, channel
layout and sample format [spec]. When nothing is playing it prints `Playback stopped.`.
Other words: `ogg pause` (or `ogg toggle`) flips pause, `ogg pause 1` / `ogg pause 0` sets it,
and prints `Playback stopped.` when nothing is open; `ogg next` forgets the current automatic
track and starts the automatic choice again (the next shuffled file when shuffling); `ogg stop` stops. Any other word prints `Usage: ogg <info|play|stop|next|pause>`.

---

## 8. When the music stops or changes

All **[read]** unless marked.

- **Level change**: the level's track (the server's "cd track" string) is re-checked when that
  string changes and when loading finishes. The same name (compared without case) keeps playing
  without restarting; a different name stops the old track and starts the new one from its start; an empty name or `0` stops the music. A track the player started with `ogg play` is left alone
  until it ends.
- **Loading plaque**: the sound system does no work while the client is loading, so no new
  music is decoded. What is already queued keeps playing (up to about 5.5 s under OpenAL, about
  0.17 to 0.19 s of buffer under the software mixer) and then it falls silent [derived from the
  buffer sizes: 8192 sample pairs at 44.1 to 48 kHz]. When loading ends the same track continues
  from where decoding stopped unless the level's track differs [derived].
- **Cinematic**: starting a cinematic stops the music completely, and no music may start while a
  cinematic plays. Afterwards the level's track starts again FROM ITS BEGINNING when the next
  level finishes loading; it does not resume where it was [derived: stopping forgets both the
  position and the current track's name]. The cinematic's own sound uses the same stream.
- **Disconnect**: after a drop (an error) or an ordinary disconnect the client plays the menu
  track, `ogg_menu_track` (default `0`, which means silence, so by default the music stops). A
  disconnect for a server-ordered reconnect leaves the music as it is.
- **Pause** (server paused): the music keeps playing. Entity loops fall silent (none are built
  while paused); one-shot sounds already playing finish.
- **Window focus** (`s_auto_focus`): when sound is deactivated, no music is decoded; under OpenAL
  the stream is paused at once and resumes where it was; under the software mixer all sounds
  stop and the music resumes from where decoding stopped once active again [derived].
- **`s_enable 0`**: restarts sound with no backend, which stops the music; nothing plays until
  sound is enabled again, and then the automatic track starts from its beginning.
- **`ogg_enable 0`** while a track is open: the music is PAUSED, not stopped (under the software
  mixer, which has no pause, it simply stops being fed and the short buffer runs out
  [derived]); `ogg_enable 1` resumes it from the same place. With no track open, `ogg_enable 1`
  starts the automatic track and `ogg_enable 0` stops. While a hand-started track plays, or
  during a cinematic, changing `ogg_enable` does nothing.
- **`ogg_menu_track`** changed while disconnected, and **`ogg_remap_tracks`** changed while
  connected, re-run the automatic choice at once.

---

## 9. The sound page of the menu

The main options page has a row `sound setup` that opens a page titled `Sound Setup` **[read]**.
Its rows, top to bottom **[read]**:

| Row | Kind | Choices or range | Writes |
| --- | --- | --- | --- |
| `sound engine` | choice | `no sound`, `software`, `OpenAL` (0, 1, 2) | `s_enable` |
| `sound latency` | slider | 0.05 to 0.2, step 0.015 | `s_mixahead` |
| `sound volume` | slider | 0 to 1, step 0.1 | `s_volume` |
| `music volume` | slider | 0 to 1, step 0.1 | `ogg_volume` |
| `enable music` | toggle | off / on | `ogg_enable` |
| `shuffle tracks` | toggle | off / on | `ogg_shuffle` |
| `underwater effect` | toggle | off / on | `s_underwater` |
| `ambient sounds` | choice | `no`, `yes`, `only player's own` (0, 1, 2) | `s_ambient` |
| `chat beep` | choice | `disabled`, `default`, `alternative` (0, 1, 2) | `cl_chat_sound` |

Details **[read]**:

- A slider without a stated step moves by one tenth of its range: 0.015 for latency, 0.1 for
  the volumes [derived: range divided by 10].
- A choice row writes the index of the chosen label. A setting whose value is outside the row's
  labels (for example `s_ambient 3`) shows the row with no label selected and is left unchanged.
  A toggle shows nothing selected for a value other than 0 or 1.
- Values are written when the player leaves the page, not while sliding; a slider writes only if
  it was moved. So choosing `OpenAL` restarts sound on leaving the page (question 5).
- Mismatch to note: the third `ambient sounds` label reads "only player's own" but writes 2,
  which the setting defines as "only entities with a model" (the player's-own-only value is 3,
  which the page cannot set). `sound latency` writes a setting the OpenAL backend does not read.
- None of the `al_` settings, `s_khz`, `s_swapstereo`, `s_underwater_gain_hf`, `s_auto_focus`
  or `cl_footsteps` has a row on this page.
