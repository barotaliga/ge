// client/demo_sequel.cpp: the sequel's client recording read frame by frame into the demo state the
// window plays: the map off the settings strings, the eye off the player's state, the players by
// slot and the other entities by number, each a delta from the frame before; sounds, prints and
// effects for their frame alone; a cut recording ending at its last whole block, damage refused
// with its place, each game's adapter opening only its own recordings, and a recording packed as
// a replay file version 2 read the same.
//
// ge:summary Test: the sequel's .dm2 recording read frame by frame into the demo state
// ge:keywords test sequel dm2 demo playback frame delta configstring adapter replay-v2
// ge:skill shooter replay
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/sequel_demo.hpp"

namespace cl = shooters_edge::client;

namespace {

struct Read {
  int frames = 0, sounds = 0, prints = 0, effects = 0;
  double first = -1, last = -1;
  std::vector<float> eye_x;
};

Read read_all(cl::Demo& d, cl::DemoRead& last, std::string& error) {
  Read r;
  for (;;) {
    last = cl::next_frame(d, error);
    if (last != cl::DemoRead::frame) break;
    r.frames++;
    if (r.first < 0) r.first = d.state.time;
    r.last = d.state.time;
    r.sounds += static_cast<int>(d.state.started.size());
    r.prints += static_cast<int>(d.state.prints.size());
    r.effects += static_cast<int>(d.state.effects.size());
    r.eye_x.push_back(d.state.eye.x);
  }
  return r;
}

}  // namespace

TEST(a_sequel_recording_plays_frame_by_frame) {
  const std::vector<uint8_t> file = cl::sequel_fixture(12, 7);
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(file, d, error), error);
  CHECK(d.state.format == cl::DemoFormat::sequel);
  cl::DemoRead end{};
  const Read r = read_all(d, end, error);
  CHECK_MSG(end == cl::DemoRead::end, error);
  CHECK(r.frames == 120);  // ten a second; the signon's blocks are not frames
  CHECK(r.first == 10.0);  // the server's frame 100
  CHECK(r.last > 21.8 && r.last < 22.0);
  const cl::DemoState& s = d.state;
  CHECK(s.map == "yard");
  CHECK(s.levels == 1);
  CHECK(s.models.size() == 40 && s.models[1] == "models/things/thing1/tris.md2");
  CHECK(s.stats[cl::kStatHealth] > 0);
  CHECK(s.stats[cl::kStatWeapon] == 7);  // the view model's index
  CHECK(r.sounds > 0 && r.effects > 0 && r.prints > 0);
  // The eye moves with the player's state; the server named no player count, so the others are
  // kept as entities.
  bool moved = false;
  for (size_t i = 1; i < r.eye_x.size(); i++) moved = moved || r.eye_x[i] != r.eye_x[0];
  CHECK(moved);
  CHECK(s.players.size() == 1 && s.players[0].present);
  CHECK(s.entities[2].present && s.entities[3].present && s.entities[4].present);
  // The other entities that moved, from their baselines by their deltas.
  int things = 0;
  for (size_t n = 33; n < s.entities.size(); n++) things += s.entities[n].present ? 1 : 0;
  CHECK(things > 4);
}

TEST(a_cut_recording_ends_at_its_last_whole_block) {
  std::vector<uint8_t> file = cl::sequel_fixture(4, 3);
  file.resize(file.size() * 2 / 3);
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(file, d, error), error);
  cl::DemoRead end{};
  const Read r = read_all(d, end, error);
  CHECK(end == cl::DemoRead::end);
  CHECK(r.frames > 10 && r.frames < 40);
}

TEST(damage_is_refused_with_its_place) {
  std::vector<uint8_t> file = cl::sequel_fixture(2, 5);
  // The first frame's block: a message id the game does not have in place of the frame's.
  const std::vector<std::span<const uint8_t>> blocks = cl::sequel_blocks(file);
  const size_t at = static_cast<size_t>(blocks[2].data() - file.data());
  file[at] = 77;
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(file, d, error), error);
  cl::DemoRead end{};
  read_all(d, end, error);
  CHECK(end == cl::DemoRead::error);
  CHECK_MSG(
      error.find("message 77") != std::string::npos && error.find("at byte") != std::string::npos,
      error);
}

TEST(each_game_opens_only_its_own_recordings) {
  const std::vector<uint8_t> file = cl::sequel_fixture(1, 9);
  cl::Demo d;
  std::string error;
  const cl::GameAdapter& sequel = cl::sequel_adapter();
  const cl::GameAdapter& classic = cl::classic_adapter();
  CHECK_MSG(sequel.open_demo(sequel, file, d, error), error);
  CHECK(!classic.open_demo(classic, file, d, error));
  CHECK(error.find("another game") != std::string::npos);
  CHECK(error.find("next:") == std::string::npos);  // a player's words
}

TEST(a_packed_sequel_recording_reads_the_same) {
  const std::vector<uint8_t> file = cl::sequel_fixture(6, 11);
  std::vector<uint8_t> packed;
  std::string error;
  CHECK_MSG(cl::pack_demo(file, packed, error), error);
  cl::Demo a, b;
  CHECK_MSG(cl::open_demo(file, a, error), error);
  CHECK_MSG(cl::open_demo(packed, b, error), error);
  CHECK(b.replay_v2 && b.state.format == cl::DemoFormat::sequel);
  cl::DemoRead ea{}, eb{};
  const Read ra = read_all(a, ea, error), rb = read_all(b, eb, error);
  CHECK(ra.frames == rb.frames && ra.eye_x == rb.eye_x);
}
