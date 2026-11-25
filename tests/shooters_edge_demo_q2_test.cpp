// client/demo_q2.cpp: Quake II's client recording read frame by frame into the demo state the
// window plays: the map off the settings strings, the eye off the player's state, the players by
// slot and the other entities by number, each a delta from the frame before; sounds, prints and
// effects for their frame alone; a cut recording ending at its last whole block, damage refused
// with its place, a stuffed command before the server data, each game's adapter opening only its
// own recordings, a recording packed as a replay file version 2 read the same, and an extended
// recording (protocol 3436: the longer settings lists, wide entity numbers and models, int23
// points, alpha and scale, 64 stats, fog).
//
// ge:summary Test: Quake II's .dm2 recording read frame by frame into the demo state
// ge:keywords test q2 dm2 demo playback frame delta configstring adapter replay-v2
// ge:keywords extended-limits protocol-3436 int23
// ge:skill shooter replay
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/q2_demo.hpp"

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

TEST(a_q2_recording_plays_frame_by_frame) {
  const std::vector<uint8_t> file = cl::q2_fixture(12, 7);
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(file, d, error), error);
  CHECK(d.state.format == cl::DemoFormat::q2);
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

TEST(a_recording_with_a_stuffed_command_before_its_server_data_plays) {
  // A recording tool can put a short block first (a stuffed "hand 3"): the server data follows.
  const std::vector<uint8_t> plain = cl::q2_fixture(4, 7);
  const std::string cmd = "hand 3\n";
  std::vector<uint8_t> file = {static_cast<uint8_t>(cmd.size() + 2), 0, 0, 0, 11};
  file.insert(file.end(), cmd.begin(), cmd.end());
  file.push_back(0);
  file.insert(file.end(), plain.begin(), plain.end());
  CHECK(cl::is_q2_demo(file));
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(file, d, error), error);
  cl::DemoRead end{};
  const Read r = read_all(d, end, error);
  CHECK_MSG(end == cl::DemoRead::end, error);
  CHECK(r.frames == 40);
  CHECK(d.state.map == "yard");
  // A block that is not whole before the server data is no recording.
  std::vector<uint8_t> cut(file.begin(), file.begin() + 6);
  CHECK(!cl::is_q2_demo(cut));
}

TEST(an_entity_s_event_plays_its_sound_for_its_frame_alone) {
  // The second player steps each second and lands hard on frame 30: a footstep (one of the four
  // steps, the body channel) and the hard landing (`*fall2.wav`, the player's own), each started
  // in its frame only, on that entity, never carried to the next frame.
  const std::vector<uint8_t> file = cl::q2_fixture(6, 7, 4, 24, true);
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(file, d, error), error);
  int steps = 0, falls = 0, frames = 0, shots = 0;
  for (;;) {
    if (cl::next_frame(d, error) != cl::DemoRead::frame) break;
    frames++;
    for (const cl::DemoSound& o : d.state.started) {
      if (o.name == nullptr || o.entity == -1) continue;  // -1: a hit's ricochet (q2_temp_fx.hpp)
      const std::string name = o.name;
      if (name == "weapons/grenlf1a.wav") continue;  // the third player's entry (below)
      if (name.starts_with("weapons/shotg")) {       // the recorder's own shotgun flash: shot, pump
        CHECK(o.entity == 1);
        shots++;
        continue;
      }
      CHECK(o.entity == 2);
      if (name.starts_with("player/step")) {
        steps++;
        CHECK(o.channel == 4);
      }
      if (name == "*fall2.wav") falls++;
    }
  }
  CHECK(frames == 60);
  CHECK_MSG(steps == 6, std::to_string(steps));  // frames 5, 15, ..., 55: none carried over
  CHECK(falls == 1);
  CHECK(shots % 2 == 0);  // each shot heard with its pump
  // Quake II's own arrivals, each in its frame at its entity: the teleport's burst, the item's
  // return and the third player's entry (effects.hpp's q2_teleport_burst and kin).
  CHECK_MSG(cl::open_demo(file, d, error), error);
  std::vector<std::string> arrivals;
  int frame = 0, flash_heard = 0;
  while (cl::next_frame(d, error) == cl::DemoRead::frame) {
    frame++;
    // The entry's flash is heard once, where the entering player is first seen.
    for (const cl::DemoSound& o : d.state.started)
      if (o.name != nullptr && std::string(o.name) == "weapons/grenlf1a.wav") {
        flash_heard++;
        CHECK(frame == 11 && o.entity == 3);
        const ge::shooter::Vec3f at = d.state.entities[3].origin;
        CHECK(o.origin.x == at.x && o.origin.y == at.y && o.origin.z == at.z);
      }
    for (const cl::DemoEffect& e : d.state.effects) {
      if (!e.name.starts_with("sq_")) continue;
      arrivals.push_back(e.name + "@" + std::to_string(frame));
      const size_t n = static_cast<size_t>(e.entity);
      const ge::shooter::Vec3f at = n - 1 == static_cast<size_t>(d.state.player)
                                        ? d.state.players[n - 1].origin
                                        : d.state.entities[n].origin;
      CHECK(e.at.x == at.x && e.at.y == at.y && e.at.z == at.z);
    }
  }
  std::string got;
  for (const std::string& a : arrivals) got += a + " ";
  CHECK_MSG(got == "sq_login@11 sq_teleport@21 sq_item_back@41 ", got);
  CHECK(flash_heard == 1);
  // Without the events, none but the recorder's own gun's.
  const std::vector<uint8_t> plain = cl::q2_fixture(6, 7);
  cl::Demo q;
  CHECK_MSG(cl::open_demo(plain, q, error), error);
  int named = 0;
  while (cl::next_frame(q, error) == cl::DemoRead::frame)
    for (const cl::DemoSound& o : q.state.started)
      named +=
          o.name != nullptr && o.entity != -1 && !std::string(o.name).starts_with("weapons/shotg")
              ? 1
              : 0;
  CHECK(named == 0);
}

TEST(a_cut_recording_ends_at_its_last_whole_block) {
  std::vector<uint8_t> file = cl::q2_fixture(4, 3);
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
  std::vector<uint8_t> file = cl::q2_fixture(2, 5);
  // The first frame's block: a message id the game does not have in place of the frame's.
  const std::vector<std::span<const uint8_t>> blocks = cl::q2_blocks(file);
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
  const std::vector<uint8_t> file = cl::q2_fixture(1, 9);
  cl::Demo d;
  std::string error;
  const cl::GameAdapter& q2 = cl::q2_adapter();
  const cl::GameAdapter& classic = cl::qw_adapter();
  CHECK_MSG(q2.open_demo(q2, file, d, error), error);
  CHECK(!classic.open_demo(classic, file, d, error));
  CHECK(error.find("another game") != std::string::npos);
  CHECK(error.find("next:") == std::string::npos);  // a player's words
}

TEST(a_packed_q2_recording_reads_the_same) {
  const std::vector<uint8_t> file = cl::q2_fixture(6, 11);
  std::vector<uint8_t> packed;
  std::string error;
  CHECK_MSG(cl::pack_demo(file, packed, error), error);
  cl::Demo a, b;
  CHECK_MSG(cl::open_demo(file, a, error), error);
  CHECK_MSG(cl::open_demo(packed, b, error), error);
  CHECK(b.replay_v2 && b.state.format == cl::DemoFormat::q2);
  cl::DemoRead ea{}, eb{};
  const Read ra = read_all(a, ea, error), rb = read_all(b, eb, error);
  CHECK(ra.frames == rb.frames && ra.eye_x == rb.eye_x);
}

TEST(an_extended_recording_reads_its_wider_fields) {
  const std::vector<uint8_t> file = cl::q2_extended_fixture();
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(file, d, error), error);
  CHECK(d.state.format == cl::DemoFormat::q2);
  // Each frame's sound and effect are its own: read the two frames one by one.
  CHECK(cl::next_frame(d, error) == cl::DemoRead::frame);
  CHECK_MSG(cl::next_frame(d, error) == cl::DemoRead::frame, error);
  const cl::DemoState& s = d.state;
  CHECK(s.protocol == 3436);
  CHECK_MSG(s.map == "big", s.map);
  CHECK(s.models.size() > 300 && s.models[300] == "models/big/tris.md2");
  CHECK(s.sounds.size() > 1000 && s.sounds[1000] == "big/hum.wav");
  CHECK(s.players.size() == 2);
  // Entity 2000 from its baseline by an int23 delta, its look bytes kept.
  CHECK(s.entities.size() > 2000 && s.entities[2000].present);
  const cl::DemoEntity& e = s.entities[2000];
  CHECK(e.model == 300);
  CHECK_MSG(e.origin.x == 40008.0f && e.origin.y == -100.0f && e.origin.z == 16.0f,
            std::to_string(e.origin.x));
  CHECK(e.alpha == 128.0f / 255.0f && e.scale == 2.0f);
  CHECK(s.entities[500].present && s.entities[500].origin.x == 1004.0f);
  // The player past a short's reach, moved by a delta; the gun model a short; 64 stats.
  CHECK_MSG(s.eye.x == 30010.0f, std::to_string(s.eye.x));
  CHECK(s.angles.y == 90.0f);
  CHECK(s.stats[cl::kStatWeapon] == 300);
  CHECK(s.stats[cl::kStatHealth] == 98);
  CHECK(s.started.size() == 1 && s.started[0].sound == 1000);
  CHECK(s.started[0].origin.x == 30000.0f);
  CHECK(s.effects.size() == 1);
  CHECK(s.effects[0].kind == cl::DemoEffectKind::explosion && s.effects[0].at.x == -50000.0f &&
        s.effects[0].at.y == 1.0f);
  CHECK(cl::next_frame(d, error) == cl::DemoRead::end);
}

TEST(an_extended_entity_past_the_limit_is_refused_with_its_place) {
  std::vector<uint8_t> file = cl::q2_extended_fixture();
  // The last frame's entity number 2000 made 9000, past the extended limit of 8191.
  for (size_t i = file.size() - 8; i > 0; i--) {
    if (file[i] == 0xD0 && file[i + 1] == 0x07 && file[i - 1] == 1) {
      file[i] = 0x28;
      file[i + 1] = 0x23;
      break;
    }
  }
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(file, d, error), error);
  cl::DemoRead end{};
  read_all(d, end, error);
  CHECK(end == cl::DemoRead::error);
  CHECK_MSG(
      error.find("entity 9000") != std::string::npos && error.find("8191") != std::string::npos,
      error);
}

TEST(the_music_a_q2_server_names_is_a_track_number_or_none) {
  CHECK(cl::q2_music_track("", 5) == 0);  // empty: the music stops
  CHECK(cl::q2_music_track("0", 5) == 0);
  CHECK(cl::q2_music_track("7", -1) == 7);
  CHECK(cl::q2_music_track("ambient", 7) == 7);  // a file's name: not played, the last kept
  CHECK(cl::q2_music_track("100", 3) == 3);
}
