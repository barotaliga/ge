// A RECORDING AS QUAKE II'S TV (docs/tv.md § Quake II's TV): a recording of Quake II (a
// .dm2: blocks, each a length and its messages, -1 at the end) played into Quake II face
// (q2_tv_face.hpp) at the game's pace, so unmodified clients of Quake II watch it as they
// would a live match.
//
// THE PACE: a block holding a frame is one server frame, 100 ms; the blocks between frames go
// at once. A block that does not read ends the play (the face notes why).
//
// THE LOOP: at the end it starts again from the first block, the server data's count raised by
// one each time round: the face sends its viewers `reconnect`, and a client keeps its loading
// picture up until the count it signs on with is new.
//
// ge:summary A recording of Quake II played at the game's pace into Quake II face, looping
// ge:keywords q2-tv tv demo dm2 recording playback loop stock-viewer protocol-34
// ge:keywords q2_tv_demo_open q2_tv_demo_update protocol-26 older-recording convert
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_tv_face_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooters_edge/net/q2_tv_face.hpp"

namespace shooters_edge::net {

struct Q2TvDemo {
  std::vector<uint8_t> bytes;
  bool loop = true;
  size_t at = 0;        // the next block's length
  int64_t due_us = -1;  // when the next frame may go; -1: at the first update
  int64_t frames = 0, rounds = 0;
  bool ended = false;     // the end reached with no loop, or a block that did not read
  int from_protocol = 0;  // an older recording's protocol, converted to 34 when opened
};

// The recording in `bytes` checked (its first block is whole and starts with the server data). An
// older protocol's recording (26 on) is converted to 34 first (client/demo_convert.hpp), since that
// is what every stock client takes. False with `error` when it is not a recording of Quake II.
bool q2_tv_demo_open(Q2TvDemo& d, std::vector<uint8_t> bytes, bool loop, std::string& error);
// The blocks due by `now_us` handed to `face`.
void q2_tv_demo_update(Q2TvDemo& d, Q2TvFace& face, int64_t now_us);

}  // namespace shooters_edge::net
