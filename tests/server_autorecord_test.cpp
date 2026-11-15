// Tests: the match's phases seen by the server core (ServerGame::phase) and every match recorded by
// itself (engine/server/autorecord.hpp). The core's sample game, given a script of phases (warmup,
// countdown, match, intermission, twice), keeps each change at its frame, says the phase in its
// status record, and records two matches, each from the frame after it began to its last, played
// back to every player's view as the live game gave it; a game with no phases records from its
// first frame to the server's stop. A match's file is named by its map and its start.
//
// ge:summary Tests: the match's phases in the server core and every match recorded by itself
// ge:keywords test phase-events autorecord sv_autorecord match-record playback status-phase
// ge:skill replay net testing
#include <string>
#include <vector>

#include "check.hpp"
#include "net/status.hpp"
#include "server/autorecord.hpp"
#include "server/playback.hpp"
#include "server/sample.hpp"
#include "server/server.hpp"

namespace sv = ge::server;
namespace gn = ge::net;

namespace {

using Views = std::vector<std::vector<uint8_t>>;

// The script: by the frame the core is about to run, in seconds of 60 frames.
int64_t g_frame = 0;
sv::LobbyPhase scripted(const void*) {
  const int64_t sec = g_frame / 60;
  if (sec < 2) return sv::LobbyPhase::warmup;
  if (sec < 3) return sv::LobbyPhase::countdown;
  if (sec < 8) return sv::LobbyPhase::match;
  if (sec < 9) return sv::LobbyPhase::intermission;
  if (sec < 10) return sv::LobbyPhase::warmup;
  if (sec < 11) return sv::LobbyPhase::countdown;
  if (sec < 15) return sv::LobbyPhase::match;
  return sv::LobbyPhase::intermission;
}

sv::ServerGame phased_game() {
  sv::ServerGame g = sv::sample_game();
  g.phase = scripted;
  return g;
}

Views views(const sv::Server& s) {
  Views row(16);
  for (int p = 0; p < 16; p++) s.game->player_view(s.state, p, row[static_cast<size_t>(p)]);
  return row;
}

sv::ServerConfig ring() {
  sv::ServerConfig c;
  c.map = "ring";
  c.bots = 8;
  c.maxclients = 16;
  c.seed = 5;
  c.offline = true;
  return c;
}

}  // namespace

TEST(each_match_is_recorded_by_itself_from_its_start_to_its_end) {
  static const sv::ServerGame game = phased_game();
  g_frame = 0;
  sv::Server s;
  std::string error;
  CHECK_MSG(sv::server_open(s, game, ring(), nullptr, nullptr, error), error);
  CHECK(s.phase == sv::LobbyPhase::warmup);
  CHECK(sv::server_public_status(s).phase == gn::StatusPhase::lobby);
  sv::MatchRecorder m;
  m.keyframe_seconds = 2;
  std::vector<std::vector<uint8_t>> files;
  std::vector<std::vector<Views>> live;  // each match's views, from the frame it began
  int started = 0;
  for (int f = 0; f < 16 * 60; f++) {
    g_frame = f + 1;  // the phase the frame leaves
    sv::server_frame(s);
    switch (sv::match_record_frame(m, s, error)) {
      case sv::MatchRecord::started:
        started++;
        live.push_back({views(s)});
        CHECK(sv::server_public_status(s).phase == gn::StatusPhase::playing);
        break;
      case sv::MatchRecord::recording:
        live.back().push_back(views(s));
        break;
      case sv::MatchRecord::finished:
        files.push_back(m.rec.bytes);
        CHECK(sv::server_public_status(s).phase == gn::StatusPhase::intermission);
        break;
      case sv::MatchRecord::failed:
        CHECK_MSG(false, error);
        break;
      case sv::MatchRecord::none:
        if (s.phase == sv::LobbyPhase::countdown)
          CHECK(sv::server_public_status(s).phase == gn::StatusPhase::countdown);
        break;
    }
  }
  CHECK(!sv::match_record_stop(m, s));  // the second match ended at 15 s: nothing open
  // The changes, each kept at its frame.
  CHECK(s.phase_changed == 7);
  CHECK(s.phase_changes.size() == 7);
  CHECK(s.phase_changes[0].from == sv::LobbyPhase::warmup &&
        s.phase_changes[0].to == sv::LobbyPhase::countdown && s.phase_changes[0].frame == 120);
  CHECK(s.phase_changes[1].to == sv::LobbyPhase::match && s.phase_changes[1].frame == 180);
  CHECK(s.phase_changes[2].to == sv::LobbyPhase::intermission &&
        s.phase_changes[2].now_us == 8000000);
  CHECK(s.phase == sv::LobbyPhase::intermission && s.phase_since_us == 15000000);
  // Two matches, each its own file, played back as the live game gave it from its first frame.
  CHECK(started == 2 && m.matches == 2);
  CHECK(files.size() == 2);
  const int64_t lengths[] = {5 * 60 - 1, 4 * 60 - 1};  // from the frame after the turn
  for (size_t k = 0; k < files.size() && k < live.size(); k++) {
    sv::Playback pb;
    CHECK_MSG(sv::playback_open(pb, game, files[k], error), error);
    CHECK_MSG(static_cast<int64_t>(pb.frames.size()) == lengths[k] - 1,
              "match " + std::to_string(k) + ": " + std::to_string(pb.frames.size()) + " frames");
    int wrong = 0;
    const auto same = [&](int64_t at) {
      for (int p = 0; p < 16; p++) {
        std::vector<uint8_t> v;
        sv::playback_view(pb, p, v);
        wrong += v != live[k][static_cast<size_t>(at)][static_cast<size_t>(p)] ? 1 : 0;
      }
    };
    same(0);
    while (sv::playback_step(pb, error)) same(pb.at);
    CHECK_MSG(wrong == 0, std::to_string(wrong) + " views differ in match " + std::to_string(k));
  }
  sv::server_close(s);
}

TEST(a_game_with_no_phases_is_recorded_from_its_first_frame_to_the_stop) {
  sv::Server s;
  std::string error;
  CHECK_MSG(sv::server_open(s, sv::sample_game(), ring(), nullptr, nullptr, error), error);
  CHECK(s.phase == sv::LobbyPhase::match);
  sv::MatchRecorder m;
  int recording = 0;
  sv::server_frame(s);
  CHECK(sv::match_record_frame(m, s, error) == sv::MatchRecord::none);  // the turn, seen
  sv::server_frame(s);
  CHECK(sv::match_record_frame(m, s, error) == sv::MatchRecord::started);
  for (int f = 0; f < 120; f++) {
    sv::server_frame(s);
    recording += sv::match_record_frame(m, s, error) == sv::MatchRecord::recording ? 1 : 0;
  }
  CHECK(recording == 120 && s.phase_changed == 0);
  CHECK(sv::match_record_stop(m, s) && m.matches == 1 && !m.on);
  sv::Playback pb;
  CHECK_MSG(sv::playback_open(pb, sv::sample_game(), m.rec.bytes, error), error);
  CHECK(pb.frames.size() == 120);
  sv::server_close(s);
}

TEST(a_matchs_file_is_named_by_its_map_and_its_start) {
  CHECK(sv::match_record_name("dm4", 1794732612) == "dm4-20261115-085012.ged");
  CHECK(sv::match_record_name("q2dm1", 946684799) == "q2dm1-19991231-235959.ged");
  CHECK(sv::match_record_name("maps/odd map", 951825600) == "maps_odd_map-20000229-120000.ged");
  CHECK(sv::match_record_name("", 0) == "match-19700101-000000.ged");
}
