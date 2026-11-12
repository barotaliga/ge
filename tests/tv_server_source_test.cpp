// Tests: the server as a TV source (engine/tv/server_source.hpp). The core's sample game runs with
// its broadcast fed to a TV node as it is written; a viewer from the start and one who joins after
// the second keyframe each keep the ring off the stream (server/sample.hpp's `SampleWatch`) and
// reach the server's digest at its last frame; the late one is sent the blocks from the newest
// keyframe only. A game with no broadcast is refused with the reason.
//
// ge:summary Tests: the server as a TV source: the sample game watched through a TV node
// ge:keywords test tv server-source broadcast sample-game late-joiner keyframe digest
// ge:skill replay net testing
#include <map>
#include <string>
#include <vector>

#include "check.hpp"
#include "replay/stream.hpp"
#include "server/sample.hpp"
#include "server/server.hpp"
#include "tv/node.hpp"
#include "tv/server_source.hpp"

namespace sv = ge::server;
namespace tv = ge::tv;
namespace rp = ge::replay;

namespace {

// The ring as a viewer keeps it off a stream: the blocks from its first keyframe on.
sv::SampleWatch watch_stream(const std::vector<uint8_t>& stream, int64_t& first_block) {
  sv::SampleWatch w;
  rp::StreamReader r;
  std::string error;
  first_block = -1;
  CHECK_MSG(rp::read_stream_header(stream, r, error), error);
  rp::Block b;
  while (rp::next_block(r, b, error)) {
    if (first_block < 0) first_block = b.index;
    for (const rp::Chunk& c : b.chunks) {
      const bool ok = c.track == tv::kServerKeyframeTrack ? sv::sample_watch_keyframe(w, c.bytes)
                                                          : sv::sample_watch_frame(w, c.bytes);
      CHECK_MSG(ok, w.error);
    }
  }
  CHECK_MSG(error.empty(), error);
  return w;
}

}  // namespace

TEST(viewers_of_the_sample_game_reach_the_servers_digest_through_a_tv_node) {
  sv::ServerConfig c;
  c.map = "ring";
  c.bots = 10;
  c.maxclients = 16;
  c.seed = 7;
  c.offline = true;
  sv::Server s;
  std::string error;
  CHECK_MSG(sv::server_open(s, sv::sample_game(), c, nullptr, nullptr, error), error);
  tv::ServerSource src;
  CHECK_MSG(tv::server_source_begin(src, s, error), error);
  tv::Node node;
  std::map<int, tv::Watch> watch;
  const tv::Send send = [&](int link, std::span<const uint8_t> m) {
    return tv::watch_receive(watch[link], m);
  };
  tv::tv_join(node, 1);
  CHECK(tv::tv_receive(node, 1, tv::tv_hello("early", false)));
  for (int f = 0; f < 25 * 60; f++) {
    sv::server_frame(s);
    tv::server_source_frame(src, s);
    if (f == 22 * 60) {  // after the keyframe at 20 s
      tv::tv_join(node, 2);
      CHECK(tv::tv_receive(node, 2, tv::tv_hello("late", false)));
    }
    CHECK_MSG(tv::tv_feed(node, tv::server_source_fresh(src), s.now_us / 1000), node.error);
    tv::tv_update(node, s.now_us / 1000, send);
  }
  tv::server_source_finish(src, s);
  CHECK(tv::tv_feed(node, tv::server_source_fresh(src), s.now_us / 1000));
  tv::tv_update(node, s.now_us / 1000 + 1000, send);
  CHECK(watch[1].ended && watch[2].ended);
  CHECK(src.keyframes == 3);  // at 0, 10 and 20 s
  for (int link : {1, 2}) {
    int64_t first = -1;
    const sv::SampleWatch w = watch_stream(watch[link].stream, first);
    CHECK(w.frames == s.frame);
    CHECK(sv::sample_watch_hash(w) == sv::server_digest(s));
    CHECK(first == (link == 1 ? 0 : 20));
  }
  CHECK(watch[2].stream.size() * 3 < watch[1].stream.size());
  // The file itself: the trailer holds the frames and the digest.
  const auto t = rp::read_trailer(src.bytes);
  CHECK(t.has_value() && t->index.size() == 25);
  sv::server_close(s);
}

TEST(a_game_with_no_broadcast_is_refused_with_the_reason) {
  sv::ServerGame g = sv::sample_game();
  g.tv_frame = nullptr;
  sv::ServerConfig c;
  c.map = "ring";
  c.bots = 2;
  c.offline = true;
  sv::Server s;
  std::string error;
  CHECK_MSG(sv::server_open(s, g, c, nullptr, nullptr, error), error);
  tv::ServerSource src;
  CHECK(!tv::server_source_begin(src, s, error));
  CHECK_MSG(error.find("has no broadcast") != std::string::npos, error);
  sv::server_close(s);
}
