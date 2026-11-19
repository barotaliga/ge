// The live map (games/shooters_edge/livemap): the plate of a compiled map (its items, teleporter
// links and water), the players followed in turn, the keys, the colours, a taken item's timer,
// that a figure-less match still draws, Quake II's recordings in Quake II's palette, and the
// plate made with no threads (the web viewer's) only once the map is wanted.
//
// ge:summary Tests: the live map's plate, follow order, keys, colours, timers, Quake II palette
// ge:keywords live-map test follow-next demo_map respawn-timer translate q2-palette
// ge:skill map-design testing
#include <array>
#include <string>
#include <vector>

#include "assets/pcx.hpp"
#include "brush/compile/compile.hpp"
#include "brush/compile/plan.hpp"
#include "check.hpp"
#include "shooters_edge/livemap/live_map.hpp"
#include "shooters_edge/livemap/watch.hpp"
#include "ui/draw.hpp"
#include "vfs/pak.hpp"

namespace cl = shooters_edge::client;
namespace lv = shooters_edge::livemap;
namespace mc = ge::brush::mapc;

namespace {

constexpr const char* kPlan = R"(# a hall, a pool and a teleporter
room hall z=0..256 box 0,0 640,512
room pool z=-64..256 box 640,0 1024,512
liquid z=-64..-8 box 640,0 1024,512 kind=water tex=*water1
light 512,256,200 300
entity info_player_deathmatch 64,64,24 angle=0
entity item_armorInv 256,256,24
entity weapon_rocketlauncher 512,128,24
)";

std::vector<uint8_t> compiled() {
  mc::Source src;
  std::string error;
  CHECK_MSG(mc::read_plan(kPlan, &src, &error), error);
  std::vector<uint8_t> bytes;
  mc::CompileReport rep;
  CHECK_MSG(mc::compile_map(src, {}, &bytes, &rep, &error), error);
  return bytes;
}

std::vector<uint8_t> grey_palette() {
  std::vector<uint8_t> p(768);
  for (size_t i = 0; i < 768; ++i) p[i] = uint8_t(i / 3);
  return p;
}

cl::DemoState three_players() {
  cl::DemoState s;
  s.protocol = 28;
  s.models = {"", "progs/player.mdl", "progs/armor.mdl"};
  s.players.assign(32, cl::DemoEntity{});
  for (int i : {1, 4, 9}) {
    s.players[size_t(i)].model = 1;
    s.players[size_t(i)].origin = {float(100 * i), 200, 24};
    s.names[size_t(i)] = "p" + std::to_string(i);
  }
  s.names[6] = "watcher";
  s.spectator[6] = true;
  return s;
}

}  // namespace

TEST(live_map_follows_the_players_in_turn) {
  const cl::DemoState s = three_players();
  CHECK(lv::next_followed(s, -1, 1) == 1);
  CHECK(lv::next_followed(s, 1, 1) == 4);
  CHECK(lv::next_followed(s, 4, 1) == 9);  // the spectator in slot 6 is passed over
  CHECK(lv::next_followed(s, 9, 1) == 1);  // round the end
  CHECK(lv::next_followed(s, 1, -1) == 9);
  CHECK(lv::next_followed(cl::DemoState{}, -1, 1) == -1);
  CHECK(lv::following_text(s, 4) == "Following p4");
}

TEST(live_map_keys_cycle_the_map_and_take_space) {
  lv::Watch w;
  cl::DemoState s = three_players();
  s.player = 1;
  std::array<bool, 64> keys{};
  keys[lv::kKeyM] = true;
  CHECK(lv::watch_keys(w, keys, s, false, true) == -1);
  CHECK(w.settings.mode == 1);
  CHECK(!keys[lv::kKeyM]);
  keys[lv::kKeySpaceBar] = true;
  CHECK(lv::watch_keys(w, keys, s, false, true) == 4);  // the next player
  CHECK(!keys[lv::kKeySpaceBar]);                       // the play bar does not pause too
  // The map off and not on TV: Space is the play bar's.
  w.settings.mode = 0;
  keys[lv::kKeySpaceBar] = true;
  CHECK(lv::watch_keys(w, keys, s, false, true) == -1);
  CHECK(keys[lv::kKeySpaceBar]);
  // On TV Space always follows; on a server joined (not allowed) nothing does.
  CHECK(lv::watch_keys(w, keys, s, true, true) == 4);
  keys[lv::kKeyM] = keys[lv::kKeySpaceBar] = true;
  CHECK(lv::watch_keys(w, keys, s, true, false) == -1);
  CHECK(w.settings.mode == 0);
}

TEST(live_map_m_flies_between_the_map_and_the_eye) {
  const std::vector<uint8_t> bytes = compiled();
  const std::vector<uint8_t> pal = grey_palette();
  lv::Watch w;
  cl::SearchPath none;
  std::string error;
  CHECK_MSG(lv::live_map_level(w.map, none, pal, bytes, "hall", 400, &error), error);
  cl::DemoState s = three_players();
  std::array<bool, 64> keys{};
  // From the eye, M climbs to the whole map.
  keys[lv::kKeyM] = true;
  lv::watch_keys(w, keys, s, false, true);
  CHECK(w.settings.mode == 1);
  CHECK(w.fly.way == ge::overview::FlyWay::up);
  for (int i = 0; i < 400; ++i) lv::watch_fly_step(w, 1.0 / 120);
  CHECK(w.fly.p == 0 && w.settings.mode == 1);
  // From the map, M dives: the demo held, then landed in the eye with the map off, catching up.
  keys[lv::kKeyM] = true;
  lv::watch_keys(w, keys, s, false, true);
  lv::watch_fly_step(w, 0.2);
  CHECK(lv::watch_demo_rate(w) == 0);
  ge::render::FrameView view;
  view.eye[0] = 5;
  view.eye[1] = 1;
  view.eye[2] = 5;
  view.fov_y = 1.2;
  CHECK(lv::watch_fly_view(w, view, 1280, 720, 80));
  CHECK(view.eye[1] > 2);  // still above him
  for (int i = 0; i < 400 && w.settings.mode != 0; ++i) lv::watch_fly_step(w, 1.0 / 120);
  CHECK(w.settings.mode == 0);
  CHECK(lv::watch_demo_rate(w) > 1);  // catching up
  ge::render::FrameView eye;
  CHECK(!lv::watch_fly_view(w, eye, 1280, 720, 80));
}

TEST(live_map_paints_the_players_colours) {
  CHECK(lv::translate(20, 3, -1) == 3 * 16 + 4);        // the shirt
  CHECK(lv::translate(100, -1, 9) == 9 * 16 + 15 - 4);  // the pants, a high colour reversed
  CHECK(lv::translate(50, 3, 9) == 50);                 // elsewhere the skin's own
  CHECK(lv::translate(20, -1, -1) == 20);
}

TEST(live_map_plate_holds_the_items_and_the_water) {
  const std::vector<uint8_t> bytes = compiled();
  const std::vector<uint8_t> pal = grey_palette();
  lv::Plate p;
  std::string error;
  CHECK_MSG(lv::build_plate(bytes, "hall", pal, 400, &p, &error), error);
  CHECK(p.image.width <= 400 && p.image.height <= 400);
  CHECK(p.spots.size() == 2);
  CHECK(!p.water.empty());
  const ge::ui::Rect r = lv::live_map_rect(p, {0, 0, 800, 200});
  CHECK(r.h == 200);
  CHECK(r.x > 0);  // centred
}

TEST(live_map_counts_down_a_taken_item) {
  const std::vector<uint8_t> bytes = compiled();
  const std::vector<uint8_t> pal = grey_palette();
  lv::LiveMap lm;
  cl::SearchPath none;
  std::string error;
  CHECK_MSG(lv::live_map_level(lm, none, pal, bytes, "hall", 400, &error), error);
  cl::DemoState s = three_players();
  s.entities.assign(4, cl::DemoEntity{});
  s.entities[2].model = 2;
  s.entities[2].present = true;
  s.entities[2].origin = {256, 256, 24};
  lv::LiveView v;
  v.area = {0, 0, 800, 400};
  v.follow = 4;
  size_t ra = 0;
  for (size_t i = 0; i < lm.plate->spots.size(); ++i)
    if (lm.plate->spots[i].kind->respawn == 20) ra = i;
  for (int f = 0; f < 10; ++f) {  // held for the first 0.5 s, then taken
    v.now_s = f * 0.1;
    s.entities[2].present = f < 5;
    ge::ui::DrawList dl;
    dl.begin(800, 400);
    lv::draw_live_map(lm, s, v, dl);
    CHECK(dl.size() > 0);
  }
  CHECK(!lm.spots[ra].present);
  CHECK(lm.spots[ra].since == 0.5);
  CHECK(lm.figures_drawn == 0);  // no packs: dots, no figures
}

TEST(live_map_takes_q2_palette_for_its_recordings) {
  // The first game's palette file and Quake II's picture, each a colour of its own.
  ge::assets::IndexedImage pic;
  pic.width = pic.height = 1;
  pic.indices = {0};
  pic.palette.assign(768, 20);
  std::vector<uint8_t> pak;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak({{"gfx/palette.lmp", std::vector<uint8_t>(768, 10)},
                                {"pics/colormap.pcx", ge::assets::encode_pcx(pic)}},
                               &pak, &error),
            error);
  cl::SearchPath files;
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  std::vector<uint8_t> store;
  cl::DemoState s;
  s.format = cl::DemoFormat::multiview;
  std::span<const uint8_t> pal = lv::recording_palette(files, s, store);
  CHECK(pal.size() == 768 && pal[0] == 10);
  s.format = cl::DemoFormat::q2;  // Quake II's recording, of any protocol version
  pal = lv::recording_palette(files, s, store);
  CHECK(pal.size() == 768 && pal[0] == 20);
}

TEST(live_map_without_threads_makes_the_plate_when_first_wanted) {
  // The web viewer has no threads: its plate is deferred and made on the frame the map is first
  // wanted, so a page that never opens the map never pays for it.
  const std::vector<uint8_t> bytes = compiled();
  cl::SearchPath files;
  std::vector<uint8_t> pak;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak({{"gfx/palette.lmp", grey_palette()}}, &pak, &error), error);
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  lv::Watch w;
  w.plate_launch = std::launch::deferred;
  cl::DemoState ds = three_players();
  ds.map = "hall";
  ds.levels = 1;
  lv::watch_level(w, files, bytes, ds);
  lv::watch_level(w, files, bytes, ds);
  CHECK(!w.map.plate);  // the map is off: nothing made
  w.settings.mode = 1;
  lv::watch_level(w, files, bytes, ds);
  CHECK(w.map.plate != nullptr);
  CHECK(w.error.empty());
}
