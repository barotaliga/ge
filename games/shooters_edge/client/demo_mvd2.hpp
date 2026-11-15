// THE SEQUEL'S MULTIVIEW RECORDING (`.mvd2`) READ, AND ONE PLAYER'S VIEW OF IT: a server's
// recording of every player of a match (the format: server/sequel_mvd2.hpp; the facts:
// porting/shooters-edge/demo-formats-sequel.md § 5) given as the sequel's client recording
// (`.dm2`, protocol 34), so every stock client plays it and the demo player follows any player.
//
// THE READER keeps every player and entity as the frames give them (a walking player's entity
// placed from his state), the settings strings as they stand, and what each block carried
// besides: the prints, the messages to one player, the effects, the sounds.
//
// ONE VIEW (`mvd2_to_sequel_stream`, demo_convert.hpp's pair; `mvd2_view` for a whole file): for
// the player it follows, a protocol 34 signon (the server data naming him, the settings strings,
// every entity's state as its baseline, `precache`) and a frame a block (his player state, every
// entity of the match as a delta from what the view last held, the effects to everyone, the
// sounds, the prints and the messages sent to him). A view starts once its player is in the game
// and keeps his last state while he is out of it. A block is cut to the stock client's 1400
// bytes: an entity that does not fit waits for the next frame, an effect or a sound that does not
// fit is dropped and counted. Changing the followed player starts the view again with a whole
// signon and a whole frame, so a viewer can switch at any frame.
//
// WHAT A VIEW HOLDS THAT THE SOURCE DOES NOT: area bits that show every area (the recording keeps
// the portals, not the viewer's areas), every entity of the match in every frame (the source is
// not culled by sight), the sounds placed on their entity. The player's velocity, his move's timers
// and his delta angles are not in the source and are 0: a recording plays without prediction.
//
// WHAT IS READ: minor versions 2009 to 2013 of the ordinary limits. A stream with the extended
// limits is refused in words.
//
// THE DEMO PLAYER: `open_demo` opens a `.mvd2` as the lowest playing player's view and keeps the
// recording (`Demo::multiview_source`); `demo_track` (the demo's `track`) or `mvd2_follow` changes
// the view to another player at the frame it is at.
//
// ge:summary The sequel's .mvd2 read; one player's view as a protocol 34 .dm2; follow any player
// ge:keywords mvd2 multiview sequel dm2 protocol-37 pov follow-player convert demo_track
// ge:keywords mvd2_view mvd2_follow mvd2_to_sequel_stream unicast multicast
// ge:skill shooter replay
// ge:test tests/shooters_edge_demo_mvd2_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "shooters_edge/server/sequel_mvd2.hpp"

namespace shooters_edge::client {

using server::is_mvd2;
using server::mvd2_write;
using server::Mvd2Frame;
using server::Mvd2Level;
using server::Mvd2Player;
using server::Mvd2Sound;

// The recording `bytes` (from its magic) as the view of player `slot` (-1: the lowest playing),
// a protocol 34 recording (.dm2) in `out`. False with the reason.
bool mvd2_view(std::span<const uint8_t> bytes, int slot, std::vector<uint8_t>& out,
               std::string& error);

struct Demo;
// A demo opened from a multiview recording (`Demo::multiview_source`) made to follow `slot` from
// the frame it is at: the view written again for him and read up to that frame. False with the
// reason (the demo is left as it was).
bool mvd2_follow(Demo& d, int slot, std::string& error);

}  // namespace shooters_edge::client
