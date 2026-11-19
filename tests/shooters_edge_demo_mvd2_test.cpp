// The sequel's multiview recording (games/shooters_edge/client/demo_mvd2.hpp): a match of two
// players written as `.mvd2` opens through `open_demo` as the lowest player's view and plays frame
// by frame with his eye, every player's entity placed from his own state (origin, a third of the
// pitch, the yaw); `mvd2_follow` changes the view to the other player at the frame it is at;
// `demo_track` (the demo's `track`) does the same from the next frame; `convert_demo` writes any
// player's view as a protocol 34 `.dm2` that the sequel's own rewriter passes through byte for
// byte; a player who leaves keeps his last view; a print and a message to one player reach only the
// views they should; damage and the extended limits are refused in words.
//
// ge:summary Tests .mvd2: open as a view, follow another player, convert to .dm2, refusals
// ge:keywords test mvd2 multiview sequel dm2 follow-player pov convert
// ge:skill shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_autotrack.hpp"
#include "shooters_edge/client/demo_convert.hpp"
#include "shooters_edge/client/demo_highlights.hpp"
#include "shooters_edge/client/demo_mvd2.hpp"
#include "shooters_edge/client/demo_radar.hpp"
#include "shooters_edge/client/demo_scan.hpp"
#include "shooters_edge/server/sequel_mvd2.hpp"

namespace cl = shooters_edge::client;
namespace sv = shooters_edge::server;

namespace {

constexpr int kFrames = 6;

// Player `c`'s origin at frame `k` in eighths of a unit.
int16_t px(int c, int k) { return static_cast<int16_t>((c == 0 ? 100 : -300) * 8 + k * 64); }

sv::Mvd2Frame frame_at(int k) {
  sv::Mvd2Frame f;
  f.players.assign(4, sv::Mvd2Player{});
  f.entities.assign(sv::kSequelEntities, sv::SequelEntityState{});
  for (int c = 0; c < 2; c++) {
    if (c == 1 && k >= 4) continue;  // the second player leaves at frame 4
    sv::Mvd2Player& p = f.players[size_t(c)];
    p.in = true;
    p.s.origin[0] = px(c, k);
    p.s.origin[1] = static_cast<int16_t>(c * 512);
    p.s.origin[2] = 24 * 8;
    p.s.view_offset[2] = 22 * 4;
    p.s.view_angles[0] = static_cast<int16_t>(-3000);                 // looking up a little
    p.s.view_angles[1] = static_cast<int16_t>(c * 16384 + k * 1024);  // turning
    p.s.fov = 90;
    p.s.gun_index = 3;
    p.s.stats[1] = static_cast<int16_t>(100 - k - c * 50);
    sv::SequelEntityState& e = f.entities[size_t(c) + 1];
    e.present = true;
    e.model[0] = 255;
    e.solid = 31;
    // The writer leaves out a walking player's place; the reader rebuilds it.
  }
  sv::SequelEntityState& item = f.entities[10];
  item.present = true;
  item.model[0] = 2;
  item.origin[0] = 64 * 8;
  item.origin[1] = 32 * 8;
  item.origin[2] = 16 * 8;
  item.effects = 1;
  if (k == 2) {
    f.prints.emplace_back(2, "Bravo was here\n");
    f.unicasts.push_back({1, {15, 'f', 'o', 'r', ' ', 'b', 0}});  // a centre print to player 1
    f.sounds.push_back({5, 255, 64, 0, 1, 2});
  }
  return f;
}

std::vector<uint8_t> match() {
  sv::Mvd2Level level;
  level.server_count = 7;
  level.config_strings.assign(sv::kSequelConfigStrings, "");
  level.config_strings[0] = "The Test Hall";
  level.config_strings[30] = "4";
  level.config_strings[33] = "maps/q2dm1.bsp";
  level.config_strings[34] = "models/items/armor/tris.md2";
  level.config_strings[288 + 5] = "items/pkup.wav";
  level.config_strings[1312] = "Alpha\\male/grunt";
  level.config_strings[1313] = "Bravo\\female/athena";
  level.base = frame_at(0);
  std::vector<sv::Mvd2Frame> frames;
  for (int k = 1; k < kFrames; k++) frames.push_back(frame_at(k));
  return sv::mvd2_write(level, frames);
}

// A duel to a kill: Alpha (0) fires a rocket at Bravo (1), who is blown up at frame 12 and
// Alpha's frags go to 1.
sv::Mvd2Frame duel_at(int k) {
  sv::Mvd2Frame f;
  f.players.assign(4, sv::Mvd2Player{});
  f.entities.assign(sv::kSequelEntities, sv::SequelEntityState{});
  for (int c = 0; c < 2; c++) {
    sv::Mvd2Player& p = f.players[size_t(c)];
    p.in = true;
    p.s.origin[0] = static_cast<int16_t>((c == 0 ? -600 : 600) * 8);
    p.s.origin[2] = 24 * 8;
    p.s.gun_index = 4;  // the rocket launcher's view model
    p.s.stats[1] = static_cast<int16_t>(c == 1 && k >= 12 ? -40 : 100);
    p.s.stats[3] = 20;
    p.s.stats[5] = c == 1 ? 100 : 0;
    p.s.stats[14] = static_cast<int16_t>(c == 0 && k >= 12 ? 1 : 0);
    if (c == 1 && k >= 12) p.s.type = 3;  // gibbed
    sv::SequelEntityState& e = f.entities[size_t(c) + 1];
    e.present = true;
    e.model[0] = 255;
    e.solid = 31;
  }
  if (k >= 2 && k < 12) {  // the rocket, from Alpha to Bravo
    sv::SequelEntityState& r = f.entities[20];
    r.present = true;
    r.model[0] = 3;
    r.origin[0] = static_cast<int16_t>((-560 + (k - 2) * 120) * 8);
    r.origin[2] = 40 * 8;
  }
  return f;
}

std::vector<uint8_t> duel() {
  sv::Mvd2Level level;
  level.config_strings.assign(sv::kSequelConfigStrings, "");
  level.config_strings[30] = "4";
  level.config_strings[33] = "maps/q2dm1.bsp";
  level.config_strings[35] = "models/objects/rocket/tris.md2";
  level.config_strings[36] = "models/weapons/v_rocket/tris.md2";
  level.config_strings[1312] = "Alpha\\male/grunt";
  level.config_strings[1313] = "Bravo\\female/athena";
  level.base = duel_at(0);
  std::vector<sv::Mvd2Frame> frames;
  for (int k = 1; k < 30; k++) frames.push_back(duel_at(k));
  return sv::mvd2_write(level, frames);
}

float eye_x(int c, int k) { return static_cast<float>(px(c, k)) / 8.0f; }

}  // namespace

TEST(an_mvd2_opens_as_the_lowest_players_view_and_plays_frame_by_frame) {
  const std::vector<uint8_t> bytes = match();
  CHECK(sv::is_mvd2(bytes));
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(bytes, d, error), error);
  CHECK(d.state.format == cl::DemoFormat::sequel);
  CHECK(!d.multiview_source.empty());
  int k = 0;
  for (;;) {
    const cl::DemoRead r = cl::next_frame(d, error);
    CHECK_MSG(r != cl::DemoRead::error, error);
    if (r == cl::DemoRead::end) break;
    if (!d.framed) continue;
    CHECK((d.state.player) == (0));
    CHECK((d.state.map) == (std::string("q2dm1")));
    CHECK((d.state.eye.x) == (eye_x(0, k)));
    CHECK((d.state.eye.z) == (24.0f + 22.0f));
    CHECK((d.state.stats[cl::kStatHealth]) == (100 - k));
    // The other player's entity stands where his state says, facing his yaw.
    if (k < 4) {
      CHECK(d.state.players[1].present);
      CHECK((d.state.players[1].origin.x) == (eye_x(1, k)));
      CHECK((d.state.players[1].origin.y) == (64.0f));
      const float yaw = static_cast<float>((64 + k * 4) & 255) * (360.0f / 256.0f);
      CHECK((d.state.players[1].angles.y) == (yaw));
    } else {
      CHECK(!d.state.players[1].present);
    }
    CHECK(d.state.entities.size() > 10 && d.state.entities[10].present);
    k++;
  }
  CHECK((k) == (kFrames));
}

TEST(mvd2_follow_changes_the_view_at_the_frame_it_is_at) {
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(match(), d, error), error);
  int k = 0;
  while (k < 2) {
    CHECK(cl::next_frame(d, error) == cl::DemoRead::frame);
    if (d.framed) k++;
  }
  const int64_t frames = d.state.frames;
  CHECK_MSG(cl::mvd2_follow(d, 1, error), error);
  CHECK((d.state.player) == (1));
  CHECK((d.state.frames) == (frames));
  CHECK((d.state.eye.x) == (eye_x(1, 1)));
  CHECK(cl::next_frame(d, error) == cl::DemoRead::frame);
  CHECK((d.state.eye.x) == (eye_x(1, 2)));
  CHECK((d.state.centre) == (std::string("for b")));  // his own message reaches his view
  CHECK(!cl::mvd2_follow(d, 3, error));               // no one plays in slot 3
  CHECK(error.find("player 3") != std::string::npos);
  // The playback's `demo_track` sets `track`: the next frame is that player's.
  d.track = 0;
  CHECK(cl::next_frame(d, error) == cl::DemoRead::frame);
  CHECK((d.state.player) == (0));
  CHECK((d.state.eye.x) == (eye_x(0, 3)));
  d.track = 3;  // no one there: the view stays
  CHECK(cl::next_frame(d, error) == cl::DemoRead::frame);
  CHECK((d.state.player) == (0));
}

TEST(any_players_view_converts_to_a_dm2_the_rewriter_passes_through) {
  for (const int pov : {0, 1}) {
    cl::ConvertOptions o;
    o.pov = pov;
    std::vector<uint8_t> view;
    cl::ConvertNotes notes;
    std::string error;
    CHECK_MSG(cl::convert_demo(match(), cl::DemoFormat::sequel, o, view, notes, error), error);
    CHECK((notes.frames) == (int64_t{kFrames}));
    CHECK(notes.dropped.empty());
    cl::Demo d;
    CHECK_MSG(cl::open_demo(view, d, error), error);
    CHECK(d.multiview_source.empty());
    CHECK(cl::next_frame(d, error) == cl::DemoRead::frame);
    CHECK((d.state.player) == (pov));
    std::vector<uint8_t> again;
    CHECK_MSG(cl::convert_demo(view, cl::DemoFormat::sequel, {}, again, notes, error), error);
    CHECK(again == view);
    // The second player leaves at frame 4: his view holds his last state to the end.
    int frames = 0;
    for (cl::DemoRead r = cl::DemoRead::frame; r == cl::DemoRead::frame;
         r = cl::next_frame(d, error))
      if (d.framed) frames++;
    CHECK((frames) == (kFrames));
    CHECK((d.state.eye.x) == (eye_x(pov, pov == 0 ? kFrames - 1 : 3)));
  }
  CHECK((cl::format_words(cl::DemoFormat::sequel_multiview)) ==
        (std::string("sequel multiview recording (.mvd2)")));
  cl::DemoFormat f{};
  CHECK(cl::format_of_name("match.mvd2", f) && f == cl::DemoFormat::sequel_multiview);
}

TEST(a_damaged_or_extended_mvd2_is_refused_in_words) {
  std::string error;
  cl::Demo d;
  std::vector<uint8_t> cut = match();
  cut.resize(40);
  CHECK(!cl::open_demo(cut, d, error));
  CHECK_MSG(error.find("no one plays") != std::string::npos, error);
  std::vector<uint8_t> v = match();
  v[7 + 4] = 0xD0;  // the minor version (after the magic, the length and the major) as 2000
  v[8 + 4] = 0x07;
  CHECK(!cl::open_demo(v, d, error));
  CHECK_MSG(error.find("37.2000") != std::string::npos, error);
  v = match();
  v[6] = static_cast<uint8_t>(4 | (4 << 5));  // the extended limits in the command's top bits
  CHECK(!cl::open_demo(v, d, error));
  CHECK_MSG(error.find("extended limits") != std::string::npos, error);
}

TEST(an_mvd2_s_kills_are_scanned_into_highlights_for_every_player) {
  cl::Demo d;
  std::string error;
  CHECK_MSG(cl::open_demo(duel(), d, error), error);
  cl::DemoScan scan;
  CHECK_MSG(cl::scan_demo(d, scan, error), error);
  CHECK(scan.rockets == 1);
  CHECK((scan.kills.size()) == (size_t(1)));
  if (scan.kills.empty()) return;
  const cl::DemoKill& k = scan.kills[0];
  CHECK(k.killer == 0 && k.victim == 1);  // the view follows Alpha, Bravo's death is seen too
  CHECK(k.weapon == shooters_edge::hud::Weapon::rocket_launcher);
  CHECK(k.gib);
  CHECK(k.shot && k.rocket_flight > 1000);
  CHECK(k.stack == 200);  // Bravo's health and armour
  CHECK((scan.frags[0]) == (1));
  const auto h = cl::demo_highlights(scan, -1);
  bool long_shot = false;
  for (const auto& m : h.list)  // merged with the annihilated stack of the same moment
    for (const std::string& kind : m.kinds) long_shot = long_shot || kind == "long-shot";
  CHECK_MSG(long_shot, std::to_string(h.list.size()) + " highlights, none a long shot");
  // Autotrack rates every player off the same stats: it starts on someone and, once Bravo is
  // dead, stays with the killer.
  cl::AutotrackPlan plan;
  CHECK_MSG(cl::autotrack_plan(d, scan, {}, plan, error), error);
  CHECK(!plan.cuts.empty());
  CHECK(cl::autotrack_at(plan, plan.length_s) == 0);
}

TEST(an_mvd2_converted_for_a_stock_client_carries_no_player_stats) {
  std::vector<uint8_t> view, ours;
  std::string error;
  CHECK_MSG(cl::mvd2_view(duel(), 0, view, error), error);
  CHECK_MSG(cl::mvd2_view(duel(), 0, ours, error, true), error);
  CHECK(ours.size() > view.size());  // the demo player's own message after each frame
  cl::Demo d;
  CHECK_MSG(cl::open_demo(view, d, error), error);
  while (cl::next_frame(d, error) == cl::DemoRead::frame) {
  }
  CHECK(d.player_stats.empty());
}

TEST(an_mvd2_gives_the_hub_its_radar_with_every_player) {
  std::vector<ge::replay::Chunk> chunks;
  ge::replay::Meta meta;
  std::string error;
  CHECK_MSG(cl::demo_radar(duel(), chunks, meta, error), error);
  CHECK(chunks.size() > 4);  // the roster, then the places four times a second
  int players = 0;
  for (const auto& [key, value] : meta) players += key == "player" ? 1 : 0;
  CHECK((players) == (2));
  bool map = false;
  for (const auto& [key, value] : meta) map = map || (key == "map" && value == "q2dm1");
  CHECK(map);
}
