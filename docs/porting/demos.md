# Demos, TV and video: recordings, playback, conversion, broadcasting

A match kept and watched again: each game's recording formats read and written, played like a
video (seek, speed, skips, highlights), converted between formats, packed, broadcast live through
TV with a delay, relayed by proxy and forward, shown on the hub's live map, and turned into a video
with sound. Each game's format is its own; what a player DOES with a demo is the same in every game.

## Shared

| Layer | File | What it gives |
| --- | --- | --- |
| Engine | `engine/replay/stream.hpp`, `codec.hpp` | Replay file version 2: header, tracks, blocks with keyframes, on disk and on the wire; the field codec |
| Engine | `engine/replay/transport.hpp`, `player.hpp`, `keep.hpp`, `highlights.hpp`, `director.hpp`, `radar.hpp` | Play, pause, speeds, skip, seek; keyframes; highlights scored by a game's rules; the director; the radar track |
| Engine | `engine/tv/` (`node.hpp`, `net_link.hpp`, `websocket.hpp`, `library.hpp`) | A TV node: a stream held by a delay, fanned out to viewers and relays (`docs/tv.md`) |
| Engine | `engine/forward/`, `engine/proxy/` | Forward and proxy hops for any game's datagrams |
| Engine | `engine/server/record.hpp`, `autorecord.hpp`, `spectators.hpp` | Every match recorded by the server, watched on its own TV node |
| Engine | `engine/capture/` (`video_cfg.hpp`, `av_export.hpp`, `mux.hpp`, `video.hpp`) | Video with sound: `video.cfg`, the A/V clock, Matroska, WebM, MP4 |
| Engine | `engine/overview/` | The map from above for the live map (`features.md`) |
| Engine | `engine/vfs/zstd.hpp` | Recordings packed as they are written |
| Shooter kit | `plugins/shooter/demo_transport.hpp` | What a player does with a demo in any shooter: `cl_demospeed`, `demo_seek`, `demo_skip`, `demo_next`, `demo_bar`, `demo_lerp` |
| Shell | `games/shooters_edge/client/demo_playback.hpp`, `demo_scan.hpp`, `demo_highlights.hpp`, `demo_autotrack.hpp`, `demo_lerp.hpp`, `demo_eyes.hpp` | Watched like a video with highlight markers; kills scanned; multi-kills and air shots; autotrack; frames interpolated; the classic eye |
| Shell | `games/shooters_edge/client/demo_convert.hpp`, `demo_convert_client.hpp`, `demo_convert_single_out.hpp`, `demo_v2.hpp`, `demo_pack.hpp`, `demo_dzip.hpp` | Conversion as a stream; the client recording for the oldest clients; recordings packed in replay file v2 and back exactly; dzip archives |
| Shell | `games/shooters_edge/client/demo_video.hpp`, `demo_caption.hpp`, `demo_voice.hpp`, `demo_sound.hpp`, `timedemo.hpp` | A demo as a video with captions, voices and sounds; the demo benchmark |
| Shell | `games/shooters_edge/client/recording_tv.hpp`, `tv_live.hpp`, `listen_record.hpp` | A recording still being written broadcast in one-second blocks; a live broadcast played as a growing demo; `record` on the listen game |
| Shell | `games/shooters_edge/livemap/` | The live map from a match or a demo |

## Each game's

| | The first game | Quake II | Next |
| --- | --- | --- | --- |
| Formats | single-player demo, client recording, multiview; with voice (`client/demo.hpp`, `demo_single.cpp`, `demo_client.cpp`, `demo_multiview.cpp`, `net/qw_record.hpp`) | its client recording and its multiview (`demo_q2.cpp`, `demo_mvd2.hpp`, `server/q2_mvd2.hpp`, `client/q2_demo.hpp`) | its own, read into the same transport |
| Conversion | `demo_convert_*.cpp` | `demo_convert_q2.cpp`, `demo_convert_q2_ext.cpp` | its converter to and from replay file v2 |
| TV faces | `net/qw_tv.hpp`, `qw_tv_udp.hpp`, `qw_qtv.hpp`, `qw_watch.hpp`, `qw_watcher.hpp`, `qw_game_tv.hpp` | `net/q2_tv_face.hpp`, `q2_tv_relay.hpp`, `q2_tv_demo.hpp`, `q2_game_tv.hpp` | its face for its stock clients |
| Forward | `net/qw_forward.hpp` | through the same | its face |

## Adding a game

1. A reader's facts on each recording format (`porting/shooters-edge/demo-formats-<game>.md`).
2. Read each into the window's drawn recording, so playback, seeking, highlights and the eyes work
   at once. The multiview recording must carry every player's health, armour, frags, weapon and
   dead state, or kill highlights and autotrack see only one player (Quake II adds its own
   message 125, left out of a file written for a stock client).
3. Pack and unpack through replay file v2 byte for byte (`demo_v2.hpp`, `q2_demo.hpp`).
4. Converters to and from the shared form; the client recording for the game's oldest clients.
5. The game's TV face for its stock clients, its server's broadcast hooks (`ServerGame`'s
   broadcast and watch), its forward face.
6. Its radar track and live map adapter (items and respawn times, players' colours, a demo's
   entities).

## Brought and left behind

| Benefit, now every game's | From | Layer |
| --- | --- | --- |
| Multiview recordings of every player | the first game's later servers | both games; Quake II's `.mvd2` written by our server |
| TV with a delay, relays, viewers by the thousand | the first game's later broadcasters | `engine/tv`, each game's face |
| Seeking, speed, skips, highlights, autotrack | the first game's later clients and tools | the shared transport, both games |
| The radar and the live map on the hub | the first game's community tools | `engine/replay/radar.hpp`, `engine/overview` |
| Voice kept in recordings | ours | `.qwdv`-style recordings, both games |
| A demo as a video with sound | ours | `engine/capture` |

| Drawback left behind | How |
| --- | --- |
| A demo played only from the start, at one speed | keyframes and seek in any game |
| One format a game, unreadable elsewhere | converted as read; replay file v2 as the shared form |
| Recordings as large files | packed by zstd as they are written |

## Gaps

- An item's return as an event in Quake II's recordings; Quake II's effects carry no normal.
- The third game's recordings.

## Labs and tests

`se-demo`, `se-demos`, `se-demo-convert`, `se-demo-pack`, `se-demo-eyes`, `se-demo-smooth`,
`se-record`, `se-q2-record`, `se-tv`, `se-tv-node`, `se-tv-netgame`, `se-q2-tv`,
`se-demo-video`, `se-highlights`, `se-timedemo`, `hub-radar`, `se-live-map`;
`tests/shooters_edge_demo_mvd2_test.cpp`.

## Lessons

- Each game's demo format is its own; the multiview machinery is shared where it can be; what a
  player does with a demo is the same everywhere.
- A recording's highlights need every player's stats in the recording: check before building the
  scan.
