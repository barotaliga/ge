// The QuakeWorld game's broadcast and watch hooks, defined (qw_game_tv.hpp).
//
// ge:summary The QuakeWorld game's broadcast and watch hooks, defined
// ge:keywords tv-frame tv-keyframe watch-hooks multiview catch-up player-view
// ge:skill net replay shooter
// ge:test tests/shooters_edge_qw_record_server_test.cpp
#include "shooters_edge/net/qw_game_tv.hpp"

#include <algorithm>
#include <span>
#include <string>

#include "shooters_edge/net/qw_server.hpp"

namespace shooters_edge::net {

namespace {

// Every whole record of `bytes` applied to the kept match. False with `error` when one is not.
bool keep(MvState& m, std::span<const uint8_t> bytes, std::string& error) {
  size_t at = 0;
  while (at < bytes.size()) {
    MvRecord h;
    const MvSplit split = mv_record(bytes.subspan(at), h);
    if (split != MvSplit::ok) {
      error = "a multiview record at byte " + std::to_string(at) + " of " +
              std::to_string(bytes.size()) +
              (split == MvSplit::more ? " is cut short" : " is damaged");
      return false;
    }
    mv_apply(m, bytes.subspan(at, h.whole));
    at += h.whole;
  }
  return true;
}

void* watch_open() { return new MvState(); }
void watch_close(void* w) { delete static_cast<MvState*>(w); }

bool watch_keyframe(void* w, std::span<const uint8_t> bytes, std::string& error) {
  MvState& m = *static_cast<MvState*>(w);
  m = MvState{};
  return keep(m, bytes, error);
}

bool watch_frame(void* w, std::span<const uint8_t> bytes, std::string& error) {
  return keep(*static_cast<MvState*>(w), bytes, error);
}

int watch_players(const void*) { return 32; }

void watch_view(const void* w, int player, std::vector<uint8_t>& out) {
  mv_player_view(*static_cast<const MvState*>(w), player, out);
}

}  // namespace

void qw_tv_frame(QwTv& tv, QwServer& s, std::vector<uint8_t>& out) {
  (void)s;
  if (!tv.on || tv.rec.bytes.empty()) return;
  std::string error;
  keep(tv.kept, tv.rec.bytes, error);  // the recorder writes whole records only
  out.insert(out.end(), tv.rec.bytes.begin(), tv.rec.bytes.end());
  tv.rec.bytes.clear();
}

void qw_tv_keyframe(QwTv& tv, QwServer& s, std::vector<uint8_t>& out) {
  if (!tv.on && s.game != nullptr) {
    mvd_start(tv.rec, false, 0);
    tv.rec.stream = true;
    s.streams.push_back(&tv.rec);
    tv.on = true;
    // The level and every player now, so the first keyframe is a whole match too.
    mvd_frame(s, tv.rec);
    std::string error;
    keep(tv.kept, tv.rec.bytes, error);
    tv.rec.bytes.clear();
  }
  const std::vector<uint8_t> catch_up = mv_catch_up(tv.kept);
  out.insert(out.end(), catch_up.begin(), catch_up.end());
}

void qw_tv_stop(QwTv& tv, QwServer& s) {
  if (!tv.on) return;
  std::erase(s.streams, &tv.rec);
  tv.on = false;
}

void qw_watch_hooks(ge::server::ServerGame& g) {
  g.watch_open = watch_open;
  g.watch_close = watch_close;
  g.watch_keyframe = watch_keyframe;
  g.watch_frame = watch_frame;
  g.watch_players = watch_players;
  g.watch_view = watch_view;
}

}  // namespace shooters_edge::net
