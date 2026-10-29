# Replays, recording and broadcasting

What the engine does with a run AFTER it has been played (watching it again, cutting a clip of
it, turning it into a video with its sound) and WHILE it is played for someone else (a stream to
a video service, spectators watching live, a friend seeing it on their chat app). The roadmap
rows are M23a–M23h (`docs/goals.md`); this page is the design they build to.

## The one idea: a run is its inputs

The simulation is deterministic to the bit (G1): the same setup and the same inputs give the same
run on every machine. So a replay is NOT a recording of pixels or of positions. It is the input
log (`engine/play/inputs.hpp`: the game, the seed, the options, then runs of held inputs), which
is a few bytes a second, plus the moments worth marking. Everything else follows from that:

| What | What it is on this engine | Row |
| --- | --- | --- |
| A replay | The input log played back through the game's own `step`: any camera, any speed, seekable | M23a, M23b |
| An instant replay ("the last 30 s") | The keyframes the live run already keeps plus the log's tail: no video buffer in memory | M23a, M23b |
| A clip | The log plus an in and an out mark; it is cut by marking, never by copying the run | M23b |
| A ghost | The racing tape (`plugins/racing/tape.hpp`), the record book's run beside the player (M10) | M10 |
| A demo (Shooter's Edge) | The native demo IS the replay file; the classic formats are read and written by the game (M18h) | M18h |
| A video | The replay rendered to frames and its sound rendered offline, encoded and muxed | M23d |
| A live stream to a video service | The live frames and the live mix, encoded and sent over RTMP | M23e |
| Spectating | The input log streamed over `engine/net` with a delay; each viewer steps the run itself | M23f |

A replay therefore costs little to keep: a ten-minute run at 120 Hz is a few kilobytes of held
keys, or a megabyte or two of text for hands that move every step (§ What a file weighs). What it
costs is CPU time to watch: a seek is a step-for-step replay from the nearest copy of the run.

## Keyframes and the seek (M23a)

A game's `Run` (`engine/play/game.hpp`) may `clone` itself: a deep copy that steps on exactly as
the original would. A clone is a KEYFRAME. The keeper (`engine/replay/keep.hpp`) takes one every
`every` seconds of the run's clock and thins them with age, so the moments most asked for (the
crash a few seconds ago) are never more than a couple of seconds from a copy and a long run's
bill stays a few dozen copies:

- every keyframe of the last `near` seconds;
- then one in `mid_every` seconds back to `mid`;
- then one in `far_every` seconds back to `window` (0: the whole run);
- the run's first step always (it is the setup, reopened for free).

The kept steps are fixed multiples of their spacing, so a keyframe dropped is never wanted again
and the thinning is the same on every machine.

A SEEK is the nearest keyframe at or before the mark, cloned (the keyframe itself is never stepped,
so it can be sought again), then stepped forward off the log to the mark. Seeking ahead goes
forward from where the run stands when that is nearer than any keyframe. The stepping is paid
out in a budget of steps a frame (`Player::advance`), so a long way back holds the picture for a
few frames instead of stalling one. The contract a lab holds: a run sought to step `s` has the
same digest and the same state as the run played straight to `s`.

THE TRANSPORT (`engine/replay/transport.hpp`) is a video player's, because that is what every
player already knows: play and pause (the end pauses, it never leaves), four speeds (a quarter,
a half, one and two), back and forward five seconds, a frame at a time while paused, and a seek
anywhere on the scrubber. It is pure state over the run's step count: the app hands it the
frame's seconds and reads back the step the picture should show; the player gets there.

## The replay file and the instant replay (M23b)

THE FILE (`engine/replay/file.hpp`, `*.ge-replay`) is the input log with what a player keeps beside
it, as text: a `ge-replay 1` line, the game's behaviour version, when it was recorded (the app's
text: the engine reads no clock), a clip's `in` and `out` when it is one, a `mark <step> <kind>
<weight>` line for every moment the game filed (a flight, a crash, a pass, the finish; the kind is
the game's word, the weight what a director ranks by), then `inputs` and the log itself. A clip is
cut by marking: the whole log stays, so a clip can be widened again.

THE VERSION is the game's (`GameSpec::version`), raised by a change that moves what a log plays to.
A replay recorded by another version is refused (`plays_on`) with a diagnostic that names both,
because a change of behaviour changes every digest (G1) and the file would play a different run; a
game that wants old replays to keep playing pins the old behaviour as a trait, the way the
generator pins a map's version. The engine's own version is not in the file: what the engine does
to a run is held bit for bit by the goldens, and a game's version moves with any engine change
that moves its digests.

THE INDEX (`engine/replay/index.hpp`) is a directory's replay files as list rows: the game and its
version, the seed, the length, how many marks and of which kinds, the clip and the date. A file
that does not read is a problem line naming it, never a silently missing row. It is what a menu's
replay browser lists (the menus are M22's).

THE INSTANT REPLAY (`engine/replay/live.hpp`): the live run is a `Live`, a play session that
keeps keyframes as it goes and the marks the game files (back-dated where the game only knows a
moment later: a flight at its take-off). "Watch the last ten seconds" hands a player the log so far
and the live keeper itself, moved, not copied; the player starts from the keeper's copy of the run
as opened (no map built again) and seeks back from the copy nearest the mark, which the keeper's
dense near band puts within `every_s` of it. The live run is set aside untouched, and `resume`
takes the keeper back with any copies the replay took. A live run that steps on while a replay is
out takes no copies until the keeper is home: a later seek costs more steps, never a wrong frame.

WHAT A FILE WEIGHS: the log is run-length text, so it is as small as the hands are still. Held keys
run for many steps a line; an analogue stick moved every step is a line a step, about 20 bytes, so
the bot's hands on Fall Line write 95 to 150 KB a minute (the `replay-file` lab's
`bytes-per-minute`). That is still nothing beside a video, and the racing tape
(`plugins/racing/tape.hpp`) is the packed form where size matters (a ghost kept per record, a
stream to spectators, M23f).

## The director (M23c)

A live camera cannot cut to the lens beside a kicker BEFORE the jump; a replay's can, because the
run has already happened. The game files moments as they happen, back-dated (a flight's mark is at
its take-off, known only at the landing), and the director (`engine/replay/director.hpp`) ranks and
spaces them into a CUT: the shots, each a stretch of the run round one mark, with its lens and its
time rate (slow motion is fewer steps a frame and nothing else).

THE GAME'S HALF is a `ShotRule` a kind of mark: how far before the mark the shot opens (a flight
opens on the run-in, before the lip), how long it holds after, its rate, what the kind's weight is
scaled by beside the others, and the lens (a name only the game's renderer reads). Fall Line's
(`games/fall_line/sim/moments.hpp`): a flight from 2.5 s before its take-off to 3 s after at half
speed through the long lens beside the kicker, worth its air time and its landing's load; a
wipeout at half speed through the impact lens, planted just past the fall; a shoulder with a rival (a brush under 2 m/s is not one) from
the front; a missed gate; the finish from above.

THE ENGINE'S HALF is the choice, a pure function of the marks, the rules and the policy: the best
mark first, each shot kept at least `spacing_s` of run from every other, the reel held to
`most_s` of run and `most_shots`; a mark inside a shot already cut is COVERED, one refused is
DROPPED, one of a kind with no rule is UNRULED, and all three are listed so a lab says why a moment
is missing. THE REEL CLOCK (`reel_at`) plays the shots one after another, each at its rate, and
says for the reel's seconds which shot is up and which run step it shows; between shots the player
seeks. A reel is played off the live run's own keyframes (the instant replay's player), so a cut
seeks from a copy the race already took: seed 38's 27 s reel holds 2 frames at one cut, where a
fresh player, stepping the race again from its start, held 42 (the `replay-cut` lab).

THE FILM CAMERA draws a cut (M26d, `games/fall_line/camera/film.hpp`, `docs/camera.md` §
Directors): each shot's lens is planted once, off the player's path, where it sees most of the
shot's stretch, and every pose after that is a pure function of the shot and the run step
(`film_pose`, fractional between steps), so a video export draws any frame without stepping the
camera from the start (the `camera-director` lab).

## Video with sound (M23d)

Two ways to a video, one pipeline:

- **Export** (the usual one): a replay rendered offline, frame by frame, at any size and frame rate,
  with its sound rendered offline in lockstep. The synth is deterministic and renders on demand
  (`engine/audio/synth.hpp`), so the sound is the run's sound sample for sample, with no drift and
  no dropped frames however slow the machine. This is `ge record` (`engine/play/record.hpp`) made a
  player's feature: the same frame clock, the renderer's own picture, plus the sound.
- **Live capture** (what the player saw, as it happened): the presented frame copied into a ring of
  readback buffers after the frame's own work and mapped a frame or two later
  (`engine/gpu/readback_ring.hpp`), never stalling the frame; the mix tapped where the synth hands
  it to the audio device. Both are stamped on the tap's clock, so the muxer interleaves them by
  time, not by arrival (M23d3, below).

The ENCODERS, in order of preference: the platform's hardware encoder (VideoToolbox on Apple,
Media Foundation on Windows, MediaCodec on Android, VA-API on Linux) for H.264, HEVC or AV1, and
a software fallback that runs anywhere, the primary environment included: AV1 (libaom, BSD) with
Opus (libopus, BSD, shared with voice chat, M17h). H.264, HEVC and AAC are only ever produced
through an operating system's own encoder, whose licence covers them; the engine ships no
patent-encumbered encoder. The CONTAINERS are the engine's own: fragmented MP4 (a crash leaves a
playable file up to the last fragment) and WebM, written in C++ in `engine/capture`, small and
held by golden tests. ffmpeg stays a lab tool only (`ge record`), never a player's dependency.

It lands in four rows:

| Row | What | Lab |
| --- | --- | --- |
| M23d1 | The export's core: the engine's Matroska muxer and reader, I420, the run's sound on the play contract, `record --format=mkv` with the sound in lockstep, uncompressed | `capture`, `capture-run` |
| M23d2 | The software encoders (AV1 by libaom, Opus by libopus) in WebM, `record --format=mkv --codec=av1` | `capture`, `capture-run` |
| M23d3 | The live capture: the readback ring and the mix tapped, stamped on the tap's clock, `fall_line --capture` | `gpu-capture-live` |
| M23d4 | The platform's hardware encoders | `capture` (on the machine that has one) |
| M23d5 | The fragmented MP4 muxer, for the hardware encoders' H.264 and HEVC | `capture` |

### The export's core (M23d1)

`engine/capture` is compiled into `ge_play` (the export is `record`'s), and holds:

- `ebml.hpp` and `matroska.hpp`: the MUXER. An EBML header, a Segment whose size is patched on
  close, a reserved Void the SeekHead is written into on close, the Info (a 1 ms timestamp scale,
  the duration patched on close), the Tracks (video 1, audio 2), Clusters of SimpleBlocks cut at a
  video keyframe after a second or 16 MB, and the Cues at the end. A file is written as it goes,
  so a crash leaves every whole cluster readable (the segment's size reads as unknown). The doc type
  is `webm` when every codec is one WebM allows, `matroska` otherwise.
- `mkv_read.hpp`: the READER the tests and the labs take a file apart with (tracks, every block's
  time, offset and size, the clusters, the cues, the seek head).
- `yuv.hpp`: RGBA to I420 and back, BT.709 at limited range in integer arithmetic, so a picture
  converts to the same bytes everywhere.
- `av_export.hpp`: THE A/V CLOCK. Picture k is at `k / fps` and shows the step `record`'s frame
  clock picks; a step's sound starts at sample `round((step - from) * rate / hz)`. The synth is
  rendered up to a step's sample before the run's sound hears that step (`Sound::step`), and each
  picture's sound (from its time to the next picture's) is muxed just before the next picture, so
  the file is interleaved by time. The sound runs exactly as long as the pictures:
  `frame_sample(frames)` samples a channel.
- `sync_fixture.hpp`: a game that flashes and clicks on the same steps, the proof of the clock.

THE RUN'S SOUND is on the play contract (`play/game.hpp`): a game registers a `SoundSpec`
(`GE_SOUND`) that opens its sound over a synth the export owns; `step` hears each step, `frame`
moves the beds. Fall Line's is `games/fall_line/sound/play_sound.hpp` (its route and mix).

By default the video is uncompressed I420 and the sound 32-bit float PCM in a `.mkv`: about 1.4 MB
a second at 320x180 and 30 fps, 41 MB a second at 720p. ffmpeg plays and converts it
(`ffmpeg -i run.mkv run.mp4`); `ge record --format=mkv` writes it with no ffmpeg at all. It plays
at `--speed=1` only, since a run's sound is its own pace.

### Compressed video (M23d2)

`ge record --format=mkv --codec=av1` writes the same export as AV1 and Opus in a `.webm`, with the
engine's own encoders and no ffmpeg; a browser, a phone and every desktop player play it.

- **Why AV1, not VP9.** Both are royalty-free and both are WebM codecs, but libvpx builds only with
  its own configure script and make (an msys shell on Windows), while libaom builds with CMake on
  every platform the engine ships on. AV1 is also the smaller file at the same quality.
- **The build** (`cmake/Codecs.cmake`, `GE_ENCODERS`): libaom 3.12.1 and libopus 1.5.2 are
  downloaded as their release archives, pinned by SHA-256, and built once at configure time into
  `GE_DEPS_DIR` (about a minute; CI caches it with Dawn and SDL). libaom is built realtime-only,
  8-bit, with its decoder (for the tests and the labs) and without its docs, examples, tests and
  tools; with no nasm on an x86 machine it is built as plain C (same bitstream, slower). Both link
  into `ge_play` privately, in this tree's build only: the published package and a game that
  pulls the engine in get `GE_ENCODERS=OFF`, where `--codec=av1` answers that the build has none.
- **The wrappers** (`capture/codecs.hpp`, the only code that includes either library): AV1 in
  libaom's realtime mode with no lookahead (speed 9, a constant quality `--crf`, 32 by default, a
  keyframe every 2 s so a cluster can start there), each packet without its temporal delimiter
  and the track's codec private the `av1C` record over the sequence header; Opus at 128 kb/s in
  20 ms packets stamped `n * 20 ms`, its encoder's delay written as the track's CodecDelay (a
  player drops those samples, so the sound still starts on its step) and an 80 ms SeekPreRoll.
- **Interleaved by time.** An Opus packet covers 20 ms whatever the frame rate, so the export
  holds an encoded picture until no packet still to come can start before it, and writes the two
  queues in time order (`AvExport::interleave`).
- **Proved** by `tests/capture_codecs_test.cpp` (AV1 decoded back above 30 dB, Opus above 20 dB
  once its pre-skip is dropped, the sync fixture's `.webm` with each click within 3 ms of its step
  and each flash within a frame) and the labs: `capture` repeats its sync check through the
  decoders and times the encoder at 720p and 1080p, `capture-run` exports the race both ways and
  measures every AV1 picture against the uncompressed one. Their AV1 numbers are measured, never
  digested, since an encoder's bytes depend on the machine.

An export to a name ending `.mp4` writes the same tracks in the engine's fragmented MP4 instead
(M23d5, below).

### The live capture (M23d3)

`fall_line --capture=run.webm` records the window and its sound while it is played: the same
file as an export (`.webm` or `.mp4` AV1 and Opus, `.mkv` uncompressed), with no frame of
the game waiting for it. Three threads meet in `engine/capture/live.hpp`, and none waits on another:

- **The audio device's thread taps the mix** (`Live::audio`, called by the device's pull right
  after the synth renders): the samples go into a lock-free ring of a few seconds, and the count
  of samples tapped IS THE CAPTURE'S CLOCK. Each tap also leaves an anchor (the samples before it
  and when it came) in a seqlock.
- **The game's thread stamps each presented frame on that clock** (`Live::stamp`: the anchor's
  samples carried forward by the wall clock since it, at most 100 ms, and never backwards; with no
  sound, the wall clock since the capture opened), then copies the frame back through the ring of
  readbacks (`gpu::ReadbackRing`: its own command buffer submitted after the frame's, the map
  started, the pixels handed back oldest first a frame or two later with their stamps), and hands
  each picture that has come back to the capture (`Live::picture`: a swap of buffers under a lock,
  so nothing is allocated once warm). A frame the ring has no free slot for, or that finds
  `queue` pictures already waiting for the worker, is DROPPED and counted, never waited for: a slow
  machine records fewer frames, not a slower game.
- **The capture's worker** converts each picture to I420 (cropped or padded to the file's size
  if the window was resized), encodes it, cuts the sound into 20 ms packets and writes both tracks
  in time order (`capture/interleave.hpp`, shared with the export): a sound packet goes in once a
  picture at or after it has been encoded, or once it is half a second older than the last frame
  stamped with no earlier picture still waiting; a picture once the sound has been cut past it.
  A picture handed to the encoder counts as still to come until its packet is out, since an
  encoder may hold one back.

The frame rate is variable: each picture keeps the time it was shown at, and `fps` is only the
track's nominal rate and AV1's keyframe spacing. If the worker stalls longer than the tap's ring
holds, the samples that did not fit are written as silence where the worker notices them, so the
sound keeps its length.

Proved by `tests/capture_live_test.cpp` (on a simulated clock: each flash within a frame of its
click at 60, 50 and 24 fps and every tapped sample in the file; a full queue drops and the hand-in
never waits; a resized window cropped or padded; a tap bigger than the ring kept as silence; a
`.webm` in time order), `tests/gpu_readback_ring_test.cpp` (frames back oldest first with their
stamps and colours, a full ring dropping instead of waiting, BGRA read back as RGBA), and the
`gpu-capture-live` lab, which runs it in real time: a frame loop at 60 fps for 30 s on the software
adapter with an audio thread pulling 10 ms every 10 ms took all 1800 frames, dropped none, spent at
most 0.9 ms of a frame on the capture (0.24 ms on average) and put every one of 24 flashes within
2 ms of its click; AV1 cost the worker 7.4 ms a 640x360 frame on average.

### Fragmented MP4 (M23d5)

`fall_line --capture=run.mp4`, or an export to a name ending `.mp4` (`ge record --format=mkv
--codec=av1` with an `.mp4` file), writes the same AV1 and Opus in an MP4: the container the
platforms' H.264 and HEVC encoders need (WebM cannot carry them, M23d4) and the one some sites and
players take alone. The muxer is the engine's own (`capture/mp4.hpp`), and `capture/mux.hpp` picks
it or the Matroska one by the file's name (`.mp4` or `.m4v`), so the export, the live capture and
the interleave hand both the same packets in the same order.

- **The layout.** An `ftyp` (isom, with iso6 for the fragments and av01 when the pictures are
  AV1), a `moov` with every track and no sample in it, then FRAGMENTS: a `moof` (per track a
  `traf` with the decode time in `tfdt` and one `trun` of each sample's duration, size and sync
  flag, its data offset counted from the `moof`) and the `mdat` of its samples. A fragment is
  written whole when the next one starts, at a video keyframe once a second has passed (or 16 MB
  has been held), so A FILE CUT SHORT PLAYS TO ITS LAST WHOLE FRAGMENT: nothing after it is needed.
  At the close an `mfra` lists every fragment's start for a player's seek, and the `mehd`'s
  duration is patched; a file without them still plays.
- **The tracks.** The video counts milliseconds and the sound samples at 48 kHz. A sample lasts
  until the next one on its track, so each track's newest sample waits for the next before it goes
  into a fragment, and the last lasts as long as the one before it. The sample entries take their
  configuration from the same codec private as Matroska's: `av01` with `av1C`, `avc1` with `avcC`,
  `hvc1` with `hvcC`, and `Opus` with a `dOps` built from the OpusHead. Opus's pre-skip is the
  sound track's edit list, so a player drops the encoder's delay and the sound starts on its step.
  Uncompressed pictures and float PCM have no MP4 entry players read; they stay in `.mkv`, and the
  muxer says so.
- **Proved** by `tests/capture_mp4_test.cpp` (the boxes read back by the engine's reader,
  `capture/mp4_read.hpp`: every sample at its time with its bytes, duration and flag; fragments
  cut at keys by time and by size and each found by the `mfra`; the file cut at every seventh byte
  reading to its last whole fragment; the sync fixture as an `.mp4` decoded to every picture with
  each click within 3 ms of its step and each flash within a frame) and the `capture` lab, which
  repeats its sync check in an MP4 at every frame rate, has ffmpeg decode every picture, and cuts
  each file halfway through its last fragment: the engine's reader keeps every whole fragment and
  ffmpeg plays at least as far (at 24 fps over 30 s, 336 of 721 pictures read and 384 played from
  a file cut in half).

## Broadcasting to a video service (M23e)

The services a player streams to take RTMP: plain RTMP and RTMPS carry H.264 and AAC, and Enhanced
RTMP (codec FourCCs in the FLV tags) adds HEVC and AV1 where the service accepts them. HLS ingest
(for HEVC at high resolutions) and WHIP (WebRTC ingest, RFC 9725, under a second of delay) are
later rows: WHIP needs ICE, DTLS and SRTP, a stack too big to build before a service a player uses
asks for it. The engine's half (`engine/capture/stream`): the FLV muxer, the RTMP handshake and
chunk stream over a TCP link of `engine/net`, TLS through the platform (or a small library where
the platform has none), and a bitrate that steps down when the send queue grows (the link's own
send budget, M17c), then back up.

THE STREAM KEY is the player's secret. It is typed into the game's settings, kept in the
operating system's keychain, never written to a log, a capture, a config file, a crash report or
a diagnostic (diagnostics print `rtmp://…/<key hidden>`), and never in the tree.

## Spectating by inputs (M23f)

The deterministic engine has a far cheaper broadcast than video: stream the INPUTS. A player's
game (or a dedicated server) sends the input log as it grows to a relay over `engine/net`, with a
delay so a spectator cannot help a team; spectators step the run themselves and watch it from any
camera, at a few bytes a second per player. A late joiner takes a snapshot (the replication of
M17f) and the inputs after it; relays fan out to more relays for a large audience. For Shooter's
Edge this is the native multi-view demo stream; the classic game's demo formats are read and
written by that game (M18h). A test: every spectator's digest equals the player's at the same
step, through every preset of the simulated network.

## Discord (M23g)

Discord shows what a friend is playing (Rich Presence) and lets them ask to join or watch. The
engine reaches the Discord client over its local IPC (a named pipe on Windows, a Unix socket in
the temporary directory elsewhere: a handshake, then JSON frames with a length header), which needs
no SDK: the activity (what, where, since when, the party's size), and the join and spectate
secrets, which open a session lobby (M17d) or a spectator stream (M23f). It is a backend of
`engine/services` (M20, built with those rows), beside the store's, so a game sets its presence once and every service
that shows one shows it. Discord's Social SDK (friends, lobbies, voice through Discord) is
proprietary, downloaded under Discord's terms and never committed; it is a later row only if a
game needs more than presence. The game's Discord application id is an identifier read from the
environment or the game's own settings, never hard-coded in the engine.

## Labs

| Lab | Question | Row |
| --- | --- | --- |
| `replay` | Does a seek land on the same run as playing straight there, and what do the keyframes cost (bytes, a clone's time, the worst seek's steps)? | M23a |
| `replay-file` | Does a replay file round-trip, refuse another version, and does a clip play its marks? | M23b |
| `replay-cut` | Which moments did the director cut to, when, at what rate, and is each shot spaced from the last? | M23c |
| `capture` | Does a replay export decode to its frames and its sound, is the sound in sync to a frame (a click and a flash on the same step), uncompressed and as AV1 and Opus in a WebM and an MP4, does an MP4 cut short play to its last whole fragment, and what does each encoder cost a frame? | M23d |
| `capture-run` | Does Fall Line's race export with its own sound, every picture and every sample of it in the file, and is the sound never silence? How much smaller is it as AV1 and Opus, and how close to the uncompressed pictures? | M23d1, M23d2 |
| `gpu-capture-live` | Does the live capture take every frame and its sound in real time, in sync, with no frame waiting on it? | M23d3 |
| `stream` | Does an RTMP session to a test ingest in the lab carry every frame, does the bitrate follow a narrowed link and recover, and is the key never printed? | M23e |
| `spectate` | Do spectators through a relay over every network preset step to the player's digest, how late, and at how many bytes a second? | M23f |
| `discord` | Against a stand-in Discord client in the lab: the handshake, the activity set and cleared, a join request answered? | M23g |

Every one runs headless on the GPU-less Linux container: the export renders on the software
adapter, the software encoder is always there, the ingest and the Discord client are stand-ins the
lab starts itself.
