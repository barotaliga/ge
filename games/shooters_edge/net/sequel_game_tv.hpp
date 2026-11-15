// THE SEQUEL'S GAME BROADCAST AND WATCHED BY THE SERVER CORE (engine/server/record.hpp,
// playback.hpp; docs/servers.md § Recording): the sequel's dedicated server, recorded with every
// player's view, so `sesv -game 2 --record=PATH` keeps its matches as the classic game's are kept.
//
// THE BROADCAST: the sequel's wire speaks for one viewpoint at a time, so the core's recording
// carries every player's state whole instead: a frame and a keyframe are the same bytes, a view
// for each of the 32 slots off the rules' world rows (rules_world.hpp). A view is 0 for a slot
// nobody plays; else 1, then his origin in eighths of a unit and view angles in 65536ths of a turn
// (shorts), model, frame, skin, effects, carried weapon model, health and frags (truncated, as the
// wire shows them), held weapon model and frame, water level, and the 32 stats his status bar
// reads (shorts). All little-endian. A frame is whole, so a seek needs no keyframe before it but
// the stream's own; the replay file packs the repeats away (zstd).
//
// THE WATCH: the last frame's 32 views, kept as they came. A player's view off the live server is
// the same writer's (`sequel_player_view`), so a recording played back is held to the match.
//
// ge:summary The sequel's game broadcast and watch hooks: every player's state whole each frame
// ge:keywords tv-frame tv-keyframe broadcast record server-record playback watch-hooks sequel
// ge:keywords player-view sequel_player_view sequel-game recording
// ge:skill net replay shooter
// ge:test tests/shooters_edge_sequel_record_server_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "server/server.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::net {

// The slots a broadcast frame carries.
inline constexpr int kSequelViewSlots = 32;

// A slot's view off the rules (above), appended to `out`.
void sequel_player_view(const shooters_edge::server::RulesCommon& c, int slot,
                        std::vector<uint8_t>& out);
// Every slot's view, appended to `out`: a broadcast frame.
void sequel_tv_state(const shooters_edge::server::RulesCommon& c, std::vector<uint8_t>& out);
// The watch hooks filled in.
void sequel_watch_hooks(ge::server::ServerGame& g);

}  // namespace shooters_edge::net
