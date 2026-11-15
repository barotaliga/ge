// THE CLASSIC GAME BROADCAST AND WATCHED BY THE SERVER CORE (engine/server/record.hpp,
// playback.hpp; docs/servers.md § Recording): the dedicated server's match as the core records it,
// in the multiview stream a classic relay already takes.
//
// THE BROADCAST: a multiview recorder in the live-stream form (classic_record.hpp) joins the
// server's streams when the first keyframe is asked for, and is written every server frame from
// then on. A frame's bytes are the records written since the last frame; a keyframe's are the
// catch-up a relay writes for a late viewer (classic_mv_state.hpp's `mv_catch_up`), off a kept
// match fed the same records. The first keyframe writes one frame at once, so it already holds the
// level and every player. A server with no port (offline) runs no endpoint and writes nothing.
//
// THE WATCH: a kept match (`MvState`) started from a keyframe's records and fed each frame's. A
// player's view is classic_mv_state.hpp's `mv_player_view`, and the live server's the same
// bytes (classic_record.hpp's `mvd_player_view`), so a recording played back is held to the match.
//
// ge:summary The classic game's broadcast and watch hooks for the server core's recordings
// ge:keywords tv-frame tv-keyframe broadcast record server-record playback watch-hooks multiview
// ge:keywords catch-up player-view classic-game recording
// ge:skill net replay shooter
// ge:test tests/shooters_edge_classic_record_server_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "server/server.hpp"
#include "shooters_edge/net/classic_mv_state.hpp"
#include "shooters_edge/net/classic_record.hpp"

namespace shooters_edge::net {

struct ClassicServer;

struct ClassicTv {
  MultiviewRecorder rec;  // in the server's streams once `on`
  MvState kept;           // the match as a viewer who joined now would have it
  bool on = false;
};

// The records written since the last call, kept and appended to `out`.
void classic_tv_frame(ClassicTv& tv, ClassicServer& s, std::vector<uint8_t>& out);
// The catch-up as the match stands; the first call starts the recorder.
void classic_tv_keyframe(ClassicTv& tv, ClassicServer& s, std::vector<uint8_t>& out);
// The recorder taken out of the server's streams (before the server goes).
void classic_tv_stop(ClassicTv& tv, ClassicServer& s);
// The watch hooks filled in: a kept match per watcher, a player's view off it.
void classic_watch_hooks(ge::server::ServerGame& g);

}  // namespace shooters_edge::net
