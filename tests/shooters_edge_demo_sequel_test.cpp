// client/demo_sequel.cpp: the sequel's client recording read frame by frame into the demo state the
// window plays: the map off the settings strings, the eye off the player's state, the players by
// slot and the other entities by number, each a delta from the frame before; sounds, prints and
// effects for their frame alone; a cut recording ending at its last whole block, damage refused
// with its place, each game's adapter opening only its own recordings, a recording packed as a
// replay file version 2 read the same, and an extended recording (protocol 3436: the longer
// settings lists, wide entity numbers and models, int23 points, alpha and scale, 64 stats, fog).
//
// ge:summary Test: the sequel's .dm2 recording read frame by frame into the demo state
// ge:keywords test sequel dm2 demo playback frame delta configstring adapter replay-v2
// ge:keywords extended-limits protocol-3436 int23
// ge:skill shooter replay
#include <cstdint>
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

// A hand-written protocol 3436 recording: a signon, then two frames that move entity 2000 and the
// player, with a sound by a word's index and an explosion that gives no light.
struct Bytes {
  std::vector<uint8_t> b;
  void u8(int v) { b.push_back(static_cast<uint8_t>(v)); }
  void u16(int v) {
    u8(v & 0xFF);
    u8((v >> 8) & 0xFF);
  }
  void i32(int32_t v) {
    u16(v & 0xFFFF);
    u16((v >> 16) & 0xFFFF);
  }
  void text(const std::string& s) {
    b.insert(b.end(), s.begin(), s.end());
    u8(0);
  }
  // An int23 point in 1/8 unit: the value itself (bit 0 set, three bytes)...
  void whole23(int32_t eighths) {
    const uint32_t w = (static_cast<uint32_t>(eighths) << 1 | 1u) & 0xFFFFFFu;
    u16(static_cast<int>(w & 0xFFFF));
    u8(static_cast<int>(w >> 16));
  }
  // ...or a delta from the last (bit 0 clear, a word).
  void delta23(int32_t eighths) {
    u16(static_cast<int>((static_cast<uint32_t>(eighths) << 1) & 0xFFFF));
  }
};

void block(std::vector<uint8_t>& file, const Bytes& w) {
  const auto n = static_cast<uint32_t>(w.b.size());
  for (int k = 0; k < 4; k++) file.push_back(static_cast<uint8_t>(n >> (8 * k)));
  file.insert(file.end(), w.b.begin(), w.b.end());
}

std::vector<uint8_t> extended_fixture() {
  std::vector<uint8_t> file;
  Bytes w;
  w.u8(12);  // the server data
  w.i32(3436);
  w.i32(0x10001);
  w.u8(0);
  w.text("");
  w.u16(0);
  w.text("The Big Yard");
  const auto config = [&](int index, const std::string& text) {
    w.u8(13);
    w.u16(index);
    w.text(text);
  };
  config(60, "2");                          // the players, past protocol 34's slot for them
  config(63, "maps/big.bsp");               // model 1, the world
  config(62 + 300, "models/big/tris.md2");  // model 300, past a byte
  config(8254 + 1000, "big/hum.wav");       // sound 1000, past a byte
  block(file, w);
  // The baseline of entity 2000: a model by a short, a whole int23 origin, alpha and scale (the
  // fifth header byte).
  w = {};
  w.u8(14);
  const uint64_t bits = (1ull << 0) | (1ull << 1) | (1ull << 7) | (1ull << 8) | (1ull << 9) |
                        (1ull << 11) | (1ull << 15) | (1ull << 23) | (1ull << 28) | (1ull << 30) |
                        (1ull << 31) | (1ull << 32);
  for (int k = 0; k < 5; k++) w.u8(static_cast<int>(bits >> (8 * k) & 0xFF));
  w.u16(2000);
  w.u16(300);            // the model
  w.whole23(40000 * 8);  // x 40000, past a short's reach
  w.whole23(-100 * 8);   // y
  w.whole23(16 * 8);     // z
  w.u8(128);             // alpha
  w.u8(32);              // scale: twice
  block(file, w);
  for (int f = 1; f <= 2; f++) {
    w = {};
    w.u8(20);  // the frame
    w.i32(f);
    w.i32(f == 1 ? -1 : f - 1);
    w.u8(0);
    w.u8(1);
    w.u8(0xFF);
    w.u8(17);  // the player: origin, view angles, gun model, and the third byte for fog
    w.u16((1 << 1) | (1 << 8) | (1 << 12) | (1 << 15));
    w.u8(1);  // flag 16: fog
    if (f == 1) {
      w.whole23(30000 * 8);
      w.whole23(0);
      w.whole23(64 * 8);
    } else {
      w.delta23(80);  // ten units on in x
      w.delta23(0);
      w.delta23(0);
    }
    w.u16(0);
    w.u16(16384);  // yaw 90
    w.u16(0);
    w.u16(300);  // the gun model, a short
    w.u8(1);     // fog: its colour alone
    w.u8(10);
    w.u8(20);
    w.u8(30);
    // The stats: health (1) and stat 40, under a varint mask.
    uint64_t mask = (1ull << 1) | (1ull << 40);
    while (mask >= 128) {
      w.u8(static_cast<int>(mask & 127) | 128);
      mask >>= 7;
    }
    w.u8(static_cast<int>(mask));
    w.u16(100 - f);
    w.u16(7);
    w.u8(18);  // the entities: 2000 moves 4 units in x by an int23 delta
    w.u8((1 << 0) | (1 << 7));
    w.u8(1);  // bit 8: a short number
    w.u16(2000);
    w.delta23(32);
    w.u8(0);
    w.u8(0);
    w.u8(9);  // a sound by a word's index, at a point
    w.u8(32 | 4);
    w.u16(1000);
    w.whole23(30000 * 8);
    w.whole23(0);
    w.whole23(0);
    w.u8(3);  // an explosion without light, at an int23 point
    w.u8(62);
    w.whole23(-50000 * 8);
    w.whole23(8);
    w.whole23(0);
    block(file, w);
  }
  for (int k = 0; k < 4; k++) file.push_back(0xFF);
  return file;
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

TEST(an_extended_recording_reads_its_wider_fields) {
  const std::vector<uint8_t> file = extended_fixture();
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(file, d, error), error);
  CHECK(d.state.format == cl::DemoFormat::sequel);
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
  std::vector<uint8_t> file = extended_fixture();
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
