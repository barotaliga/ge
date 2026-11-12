// The TV node (engine/tv/node.hpp) with no network: the source in any pieces comes out whole to
// a viewer from the start, a late joiner starts at the newest keyframe block, the delay holds
// blocks back in game time, a full link resumes where it stopped, chat goes up and comes down,
// a relay's count adds up, and a source that is not a replay file stops with the reason.
//
// ge:summary Tests the TV node: whole stream, late joiner, delay, back-pressure, chat, count
// ge:keywords test tv node delay catch-up keyframe back-pressure chat viewer-count
// ge:skill replay testing
#include <map>
#include <string>
#include <vector>

#include "check.hpp"
#include "replay/stream.hpp"
#include "tv/node.hpp"

namespace rp = ge::replay;
namespace tv = ge::tv;

namespace {

struct Made {
  std::vector<uint8_t> file;
  size_t header = 0;
  std::vector<size_t> starts;  // each block's offset
};

// `blocks` one-second blocks, a keyframe every `key`, ended by the trailer.
Made make_stream(int blocks, int key, size_t chunk_bytes = 300) {
  Made m;
  rp::StreamWriter w;
  rp::StreamHeader h;
  h.game = "tv-test";
  h.meta = {{"map", "hall"}};
  h.tracks = {{1, rp::TrackKind::inputs, rp::TrackCodec::stored, "inputs"}};
  rp::stream_begin(w, h, m.file);
  m.header = m.file.size();
  for (int b = 0; b < blocks; b++) {
    m.starts.push_back(m.file.size());
    std::vector<uint8_t> bytes(chunk_bytes);
    for (size_t i = 0; i < bytes.size(); i++)
      bytes[i] = static_cast<uint8_t>(static_cast<size_t>(b) * 7 + i * 13);
    rp::stream_add(w, 1, int64_t{b} * 1000, bytes);
    rp::stream_end_block(w, b % key == 0);
  }
  m.starts.push_back(m.file.size());  // the trailer's start
  rp::stream_finish(w, {{"duration", std::to_string(blocks)}});
  return m;
}

std::vector<uint8_t> slice(const std::vector<uint8_t>& b, size_t from, size_t to) {
  return {b.begin() + static_cast<std::ptrdiff_t>(from),
          b.begin() + static_cast<std::ptrdiff_t>(to)};
}

// Every link's watch, fed by the node's sends; `room` caps the messages a link takes an update.
struct Links {
  std::map<int, tv::Watch> watch;
  int room = 1 << 30;
  std::map<int, int> taken;
  tv::Send send() {
    return [this](int link, std::span<const uint8_t> m) {
      if (taken[link]++ >= room) return false;
      return tv::watch_receive(watch[link], m);
    };
  }
  void next_update() { taken.clear(); }
};

void hello(tv::Node& n, int link, const char* name, bool relay = false) {
  tv::tv_join(n, link);
  CHECK(tv::tv_receive(n, link, tv::tv_hello(name, relay)));
}

}  // namespace

TEST(a_viewer_from_the_start_gets_the_whole_stream_fed_in_any_pieces) {
  const Made m = make_stream(12, 4);
  tv::Node n;
  Links l;
  hello(n, 1, "ann");
  for (size_t at = 0; at < m.file.size(); at += 97) {
    const size_t to = std::min(m.file.size(), at + 97);
    CHECK_MSG(tv::tv_feed(n, slice(m.file, at, to), 0), n.error);
    tv::tv_update(n, 0, l.send());
  }
  CHECK(n.finished && n.blocks_in == 12);
  const tv::Watch& w = l.watch[1];
  CHECK(w.ended && w.blocks == 12);
  CHECK(w.stream == slice(m.file, 0, m.starts.back()));
}

TEST(a_late_joiner_starts_at_the_newest_keyframe_block) {
  const Made m = make_stream(12, 4);
  tv::Node n;
  Links l;
  CHECK(tv::tv_feed(n, slice(m.file, 0, m.starts[10]), 0));  // blocks 0..9 out
  tv::tv_update(n, 0, l.send());
  CHECK(tv::tv_catch_up_blocks(n) == 2);  // 8 and 9
  hello(n, 2, "bo");
  tv::tv_update(n, 0, l.send());
  std::vector<uint8_t> want = slice(m.file, 0, m.header);
  const auto rest = slice(m.file, m.starts[8], m.starts[10]);
  want.insert(want.end(), rest.begin(), rest.end());
  CHECK(l.watch[2].stream == want);
  // It reads as a replay file starting at a keyframe.
  rp::StreamReader r;
  std::string error;
  CHECK(rp::read_stream_header(l.watch[2].stream, r, error));
  rp::Block b;
  CHECK(rp::next_block(r, b, error) && b.keyframe && b.index == 8);
  // Blocks before the newest keyframe were let go.
  CHECK(n.first == 8);
}

TEST(the_delay_holds_blocks_back_in_game_time) {
  const Made m = make_stream(10, 5);
  tv::Node n;
  n.config.delay_ms = 3000;
  Links l;
  hello(n, 1, "ann");
  CHECK(tv::tv_feed(n, slice(m.file, 0, m.starts[6]), 1000));  // blocks 0..5 arrive at 1 s
  tv::tv_update(n, 3999, l.send());
  CHECK(l.watch[1].have_header && l.watch[1].blocks == 0);
  tv::tv_update(n, 4000, l.send());  // block 0 (game 0) + 3 s after it arrived
  CHECK(l.watch[1].blocks == 1);
  tv::tv_update(n, 6500, l.send());  // blocks 1 and 2
  CHECK(l.watch[1].blocks == 3);
}

TEST(a_full_link_resumes_where_it_stopped) {
  const Made m = make_stream(8, 8, size_t{150} * 1024);  // blocks bigger than a piece
  tv::Node n;
  Links l;
  l.room = 1;
  hello(n, 1, "ann");
  CHECK(tv::tv_feed(n, m.file, 0));
  for (int i = 0; i < 200 && !l.watch[1].ended; i++) {
    l.next_update();
    tv::tv_update(n, 0, l.send());
  }
  CHECK(l.watch[1].ended);
  CHECK(l.watch[1].stream == slice(m.file, 0, m.starts.back()));
}

TEST(chat_goes_up_and_comes_down_and_relays_add_their_counts) {
  tv::Node n;
  Links l;
  hello(n, 1, "ann");
  hello(n, 2, "relay", true);
  CHECK(tv::tv_receive(n, 2, tv::tv_count(40)));
  CHECK(tv::tv_viewer_count(n) == 41);
  CHECK(tv::tv_receive(n, 1, tv::tv_chat("anyone", "gg")));
  CHECK(tv::tv_receive(n, 2, tv::tv_chat("cid", "nice")));
  CHECK(n.chat.size() == 2 && n.chat[0].who == "ann" && n.chat[1].who == "cid");
  const Made m = make_stream(2, 1);
  CHECK(tv::tv_feed(n, m.file, 0));
  tv::tv_update(n, 0, l.send());
  CHECK(l.watch[1].chat.size() == 2 && l.watch[2].chat.size() == 2);
  CHECK(l.watch[1].viewers == 41);
  // A relay node passes its viewers' lines up instead.
  tv::Node r;
  r.config.relay = true;
  hello(r, 1, "dee");
  CHECK(tv::tv_receive(r, 1, tv::tv_chat("x", "hi")));
  CHECK(r.up.size() == 1 && r.up[0].who == "dee" && r.chat.empty());
}

TEST(a_source_that_is_not_a_replay_file_or_another_games_is_refused) {
  tv::Node n;
  const std::vector<uint8_t> junk(64, 'x');
  CHECK(!tv::tv_feed(n, junk, 0));
  CHECK_MSG(n.error.find("not a replay file") != std::string::npos, n.error);
  tv::Node g;
  g.config.game = "fall_line";
  CHECK(!tv::tv_feed(g, make_stream(1, 1).file, 0));
  CHECK_MSG(g.error.find("tv-test") != std::string::npos, g.error);
  // A viewer speaking another wire version is dropped.
  tv::Node v;
  tv::tv_join(v, 1);
  auto bad = tv::tv_hello("old", false);
  bad[1] = 99;
  CHECK(!tv::tv_receive(v, 1, bad));
}

TEST(a_full_tv_turns_the_next_viewer_away) {
  tv::Node n;
  n.config.max_viewers = 1;
  Links l;
  hello(n, 1, "ann");
  hello(n, 2, "bo");
  CHECK(tv::tv_feed(n, make_stream(1, 1).file, 0));
  tv::tv_update(n, 0, l.send());
  CHECK(l.watch[1].blocks == 1 && l.watch[2].refused == "this TV is full");
}
