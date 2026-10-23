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
spaces them into a cut list: which moment holds the frame and at what time rate (slow motion is
fewer steps a frame and nothing else). The engine owns the ranking, the spacing and the clock;
the game owns its moments' kinds, their weights and its lenses (Fall Line's chase, side, front,
aerial and the long lens by a kicker).

## Video with sound (M23d)

Two ways to a video, one pipeline:

- **Export** (the usual one): a replay rendered offline, frame by frame, at any size and frame rate,
  with its sound rendered offline in lockstep. The synth is deterministic and renders on demand
  (`engine/audio/synth.hpp`), so the sound is the run's sound sample for sample, with no drift and
  no dropped frames however slow the machine. This is `ge record` (`engine/play/record.hpp`) made a
  player's feature: the same frame clock, the renderer's own picture, plus the sound.
- **Live capture** (what the player saw, as it happened): the presented frame copied into a ring of
  readback buffers inside the frame's own encoder and mapped a frame or two later
  (`engine/gpu/target.hpp`'s `Readback`), never stalling the frame; the mix tapped where the synth
  hands it to the audio device. Both are stamped on the run clock, so the muxer interleaves them by
  time, not by arrival.

The ENCODERS, in order of preference: the platform's hardware encoder (VideoToolbox on Apple,
Media Foundation on Windows, MediaCodec on Android, VA-API on Linux) for H.264, HEVC or AV1, and
a software fallback that runs anywhere, the primary environment included: VP9 (libvpx, BSD) with
Opus (libopus, BSD, shared with voice chat, M17h). H.264, HEVC and AAC are only ever produced
through an operating system's own encoder, whose licence covers them; the engine ships no
patent-encumbered encoder. The CONTAINERS are the engine's own: fragmented MP4 (a crash leaves a
playable file up to the last fragment) and WebM, written in C++ in `engine/capture`, small and
held by golden tests. ffmpeg stays a lab tool only (`ge record`), never a player's dependency.

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
| `capture` | Does a replay export decode to its frames and its sound, is the sound in sync to a frame (a click and a flash on the same step), and what does each encoder cost a frame? | M23d |
| `stream` | Does an RTMP session to a test ingest in the lab carry every frame, does the bitrate follow a narrowed link and recover, and is the key never printed? | M23e |
| `spectate` | Do spectators through a relay over every network preset step to the player's digest, how late, and at how many bytes a second? | M23f |
| `discord` | Against a stand-in Discord client in the lab: the handshake, the activity set and cleared, a join request answered? | M23g |

Every one runs headless on the GPU-less Linux container: the export renders on the software
adapter, the software encoder is always there, the ingest and the Discord client are stand-ins the
lab starts itself.
