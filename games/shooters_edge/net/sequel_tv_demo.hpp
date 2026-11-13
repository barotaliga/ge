// A RECORDING AS THE SEQUEL'S TV (docs/tv.md § The sequel's TV): a recording of the sequel (a
// .dm2: blocks, each a length and its messages, -1 at the end) played into the sequel face
// (sequel_tv_face.hpp) at the game's pace, so unmodified clients of the sequel watch it as they
// would a live match.
//
// THE PACE: a block holding a frame is one server frame, 100 ms; the blocks between frames go
// at once. A block that does not read ends the play (the face notes why).
//
// THE LOOP: at the end it starts again from the first block, the server data's count raised by
// one each time round: the face sends its viewers `reconnect`, and a client keeps its loading
// picture up until the count it signs on with is new.
//
// ge:summary A recording of the sequel played at the game's pace into the sequel face, looping
// ge:keywords sequel-tv tv demo dm2 recording playback loop stock-viewer protocol-34
// ge:keywords sequel_tv_demo_open sequel_tv_demo_update
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_tv_face_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooters_edge/net/sequel_tv_face.hpp"

namespace shooters_edge::net {

struct SequelTvDemo {
  std::vector<uint8_t> bytes;
  bool loop = true;
  size_t at = 0;        // the next block's length
  int64_t due_us = -1;  // when the next frame may go; -1: at the first update
  int64_t frames = 0, rounds = 0;
  bool ended = false;  // the end reached with no loop, or a block that did not read
};

// The recording in `bytes` checked (its first block is whole and starts with the server data).
// False with `error` when it is not a recording of the sequel.
bool sequel_tv_demo_open(SequelTvDemo& d, std::vector<uint8_t> bytes, bool loop,
                         std::string& error);
// The blocks due by `now_us` handed to `face`.
void sequel_tv_demo_update(SequelTvDemo& d, SequelTvFace& face, int64_t now_us);

}  // namespace shooters_edge::net
