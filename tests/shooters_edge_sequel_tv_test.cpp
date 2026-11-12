// Tests: the sequel's match broadcast (games/shooters_edge/client/sequel_tv.hpp). Fed a made-up
// recording block by block, the broadcast writes exactly the bytes `pack_demo` writes for the
// finished recording; a live broadcast through a TV node (engine/tv/node.hpp), held by a delay, is
// watched by a viewer from the start and one who joins half way: each one's stream unpacks to the
// whole recording byte for byte.
//
// ge:summary Tests: the sequel's broadcast: the same bytes as the file, watched live through TV
// ge:keywords test sequel broadcast tv-node late-viewer replay-v2 round-trip dm2
// ge:skill replay shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/sequel_demo.hpp"
#include "shooters_edge/client/sequel_tv.hpp"
#include "tv/node.hpp"

namespace cl = shooters_edge::client;
namespace tv = ge::tv;

TEST(the_broadcast_writes_the_bytes_the_file_has) {
  const std::vector<uint8_t> rec = cl::sequel_fixture(25, 5);
  std::vector<uint8_t> file;
  std::string error;
  CHECK(cl::pack_demo(rec, file, error));
  cl::SequelBroadcast b;
  cl::sequel_broadcast_begin(b, 30, rec.size());
  std::vector<uint8_t> live;
  for (const std::span<const uint8_t> block : cl::sequel_blocks(rec)) {
    cl::sequel_broadcast_block(b, block);
    const std::span<const uint8_t> fresh = cl::sequel_broadcast_fresh(b);
    live.insert(live.end(), fresh.begin(), fresh.end());
  }
  cl::sequel_broadcast_end(b);
  const std::span<const uint8_t> last = cl::sequel_broadcast_fresh(b);
  live.insert(live.end(), last.begin(), last.end());
  CHECK(live == file);
  std::vector<uint8_t> back;
  CHECK_MSG(cl::unpack_demo(live, back, error) && back == rec, error);
}

TEST(a_live_sequel_match_is_watched_through_a_tv_node_from_its_start_and_late) {
  const std::vector<uint8_t> rec = cl::sequel_fixture(40, 11);
  const std::vector<std::span<const uint8_t>> blocks = cl::sequel_blocks(rec);
  tv::Node node;
  node.config.delay_ms = 2000;
  tv::Watch early, late;
  tv::Watch* watch[2] = {&early, &late};
  const tv::Send send = [&](int link, std::span<const uint8_t> msg) {
    return tv::watch_receive(*watch[link], msg);
  };
  cl::SequelBroadcast b;
  cl::sequel_broadcast_begin(b, 0);
  int64_t late_catch_up = -1;
  int64_t now = 0;
  for (size_t i = 0; i <= blocks.size() + 40; i++, now += 100) {
    if (i < blocks.size()) {
      cl::sequel_broadcast_block(b, blocks[i]);
    } else if (i == blocks.size()) {
      cl::sequel_broadcast_end(b);
      CHECK(tv::tv_feed(node, cl::sequel_broadcast_fresh(b), now));
      tv::tv_end(node);
    }
    const std::span<const uint8_t> fresh = cl::sequel_broadcast_fresh(b);
    if (!fresh.empty()) CHECK(tv::tv_feed(node, fresh, now));
    if (i == 0) {
      tv::tv_join(node, 0);
      CHECK(tv::tv_receive(node, 0, tv::tv_hello("early", false)));
    }
    if (i == blocks.size() / 2) {
      late_catch_up = tv::tv_catch_up_blocks(node);
      tv::tv_join(node, 1);
      CHECK(tv::tv_receive(node, 1, tv::tv_hello("late", false)));
    }
    tv::tv_update(node, now, send);
  }
  CHECK(early.ended && late.ended);
  CHECK(late_catch_up > 10);  // the match from its start, not from a later keyframe
  std::string error;
  for (const tv::Watch* w : watch) {
    std::vector<uint8_t> back;
    CHECK_MSG(cl::unpack_demo(w->stream, back, error), error);
    CHECK(back == rec);
  }
}
