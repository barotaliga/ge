// Tests: the listen server the `shooters_edge` window runs, driven headless. The baseline mod
// compiled from the sources built into the game; the fixture map (plugins/brush) found in a pack
// through the search path (a later pack overrides an earlier one); the player spawned in its hall
// on a rocket launcher, who walks, jumps, and then ROCKET-JUMPS: looking straight down, he jumps
// and fires at the floor in the same frame and rises far higher than his jump alone takes him,
// for the rocket's health. The run is held to the bit by its golden rows. Also the keys and the
// mouse as commands, the HUD's status off the player, and the weapon in his hand: the launcher
// raised, one shot seen for one rocket and none for the spawn. The powers: the quad's and the
// shield's glow round their holder, the shield taking a blast, both running out. A game stopped
// lets go of its servers and map and starts again. A mod's named effects (pointparticles,
// trailparticles) reach the player by name, or as the classic effect of a classic name.
//
// ge:summary Tests: the listen server headless: a fixture map from a pack, a rocket jump, golden
// ge:keywords test listen-server rocket-jump search-path pak baseline-mod controls hud-status
// ge:keywords listen_stop pointparticles trailparticles named-effect
// ge:keywords golden fixture held-weapon step_held quad shield powers effects
// ge:skill shooter progs testing
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "audio/synth.hpp"
#include "audio/wav.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "classic/q2_move.hpp"
#include "golden.hpp"
#include "progs/vm.hpp"
#include "save/file.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/held.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/listen_sound.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/remix_bar.hpp"
#include "shooters_edge/client/settings.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/weapon_groups.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_native.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/qvm_server.hpp"
#include "shooters_edge/server/qvm_test_mod.hpp"
#include "shooters_edge_campaign_fixture.hpp"
#include "vfs/pak.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace br = ge::brush;
namespace sh = ge::shooter;

constexpr const char* kGolden = "tests/golden/shooters_edge_listen.txt";

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

br::Entity entity(std::initializer_list<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  for (const auto& p : pairs) e.pairs.push_back(p);
  return e;
}

// The fixture halls with a deathmatch spot in the hall facing east, a rocket launcher under it.
std::vector<uint8_t> halls() {
  br::Fixture f = br::sample_fixture();
  f.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "128 192 40"}, {"angle", "0"}}));
  f.entities.push_back(entity({{"classname", "weapon_rocketlauncher"}, {"origin", "128 192 0"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_bsp(f, br::Family::v29, &bytes, &error), error);
  return bytes;
}

std::vector<uint8_t> pack(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files) {
  std::vector<uint8_t> out;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak(files, &out, &error), error);
  return out;
}

const ge::progs::Progs& baseline() {
  static const ge::progs::Progs p = [] {
    ge::progs::Progs out;
    std::string error;
    CHECK_MSG(cl::baseline_progs(out, error), error);
    return out;
  }();
  return p;
}

// The listen server on the halls, found in the second of two packs; `lines` run on its server.
bool start(cl::Listen& l, const std::vector<std::string>& lines = {},
           const std::vector<uint8_t>* module = nullptr, const std::string* library = nullptr) {
  cl::SearchPath files;
  std::string error;
  CHECK(cl::add_archive(files, "pak0.pak", pack({{"maps/halls.bsp", {1, 2, 3}}}), error));
  CHECK(cl::add_archive(files, "pak1.pak", pack({{"maps/halls.bsp", halls()}}), error));
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, "halls", error);
  CHECK_MSG(map != nullptr, error);
  if (!map) return false;
  cl::ListenOptions opt = shooters_edge::net::listen_options();
  opt.arsenal.insert(opt.arsenal.end(), lines.begin(), lines.end());
  opt.module = module;
  opt.native_library = library;
  const bool ok = cl::listen_start(l, baseline(), std::move(map), opt, error);
  CHECK_MSG(ok, error);
  return ok;
}

struct Rjump {
  std::vector<std::vector<uint64_t>> rows;  // every 10th frame: frame, x, y, z, vz, health
  float ground_z = 0;
  float jump_peak = 0;    // the plain jump's highest origin
  float rocket_peak = 0;  // the rocket jump's
  float health_before = 0, health_after = 0;
  float rockets_before = 0, rockets_after = 0;
  int weapon = 0;
  std::vector<std::string> sounds;  // what the server started for the player, by name, in order
};

uint64_t bits(float f) {
  uint32_t u = 0;
  std::memcpy(&u, &f, 4);
  return u;
}

Rjump rocket_jump() {
  Rjump r;
  cl::Listen l;
  if (!start(l)) return r;
  sh::MoveCmd idle;
  const auto run = [&](int frames, sh::MoveCmd cmd, float* peak) {
    for (int i = 0; i < frames; i++) {
      cl::listen_frame(l, cmd);
      cmd.impulse = 0;
      for (const cl::DemoSound& d : l.sounds)
        r.sounds.push_back(l.server->sounds[static_cast<size_t>(d.sound)]);
      l.sounds.clear();
      const sh::Edict& me = l.server->sv.edicts[1];
      if (peak) *peak = std::max(*peak, me.origin.z);
      if (l.frames % 10 == 0)
        r.rows.push_back({static_cast<uint64_t>(l.frames), bits(me.origin.x), bits(me.origin.y),
                          bits(me.origin.z), bits(me.velocity.z), bits(me.health)});
    }
  };
  run(80, idle, nullptr);  // the spawn, the fall onto the launcher, picked up
  l.impulse = 7;           // the rocket launcher in hand
  run(80, idle, nullptr);  // and ready to fire
  r.ground_z = l.server->sv.edicts[1].origin.z;
  r.weapon = static_cast<int>(cl::field_named(l, 1, "weapon"));
  sh::MoveCmd walk;
  walk.forward = 400;
  run(15, walk, nullptr);  // a few steps into the hall (the east doorway's low lintel is far)
  run(60, idle, nullptr);  // and stopped
  sh::MoveCmd jump;
  jump.buttons = sh::kButtonJump;
  r.jump_peak = r.ground_z;
  run(1, jump, &r.jump_peak);
  run(80, idle, &r.jump_peak);  // up and down again
  // The rocket jump: straight down, jump and fire together.
  r.health_before = l.server->sv.edicts[1].health;
  r.rockets_before = cl::field_named(l, 1, "ammo_rockets");
  l.view.x = 89;
  sh::MoveCmd both;
  both.buttons = sh::kButtonJump | sh::kButtonAttack;
  r.rocket_peak = r.ground_z;
  run(2, both, &r.rocket_peak);
  run(120, idle, &r.rocket_peak);
  r.health_after = l.server->sv.edicts[1].health;
  r.rockets_after = cl::field_named(l, 1, "ammo_rockets");
  return r;
}

const Rjump& the_jump() {
  static const Rjump r = rocket_jump();
  return r;
}

}  // namespace

TEST(the_search_path_takes_the_newest_pack_s_file) {
  cl::SearchPath files;
  std::string error;
  CHECK(cl::add_archive(files, "pak0.pak", pack({{"a.txt", {1}}, {"b.txt", {2}}}), error));
  CHECK(cl::add_archive(files, "pak1.pak", pack({{"a.txt", {3, 4}}}), error));
  std::vector<uint8_t> store;
  CHECK(cl::find_file(files, "a.txt", store).size() == 2);
  CHECK(cl::find_file(files, "b.txt", store).size() == 1);
  CHECK(cl::find_file(files, "c.txt", store).empty());
  CHECK(!cl::add_archive(files, "bad.pak", {'P', 'A', 'C', 'K'}, error));
  CHECK(!cl::open_game_dir("no-such-base-folder", "id1", files, error));
  CHECK(error.find("-basedir") != std::string::npos);
}

TEST(a_save_slot_reads_as_our_title_or_the_classic_comment) {
  namespace fs = std::filesystem;
  cl::SearchPath files;
  files.folder = (fs::temp_directory_path() / "ge_se_slot_test").string();
  fs::remove_all(files.folder);
  std::string error;
  CHECK(cl::save_label(files, "s0").empty());  // an empty slot
  std::vector<uint8_t> ours;
  ge::save::Meta m;
  m.game = "shooters_edge";
  m.title = "the hall, kills 3/12";
  ge::save::FileWriter w(ours, m);
  w.finish();
  CHECK_MSG(cl::write_save(files, "s0", ours, error), error);
  CHECK(cl::save_label(files, "s0") == "the hall, kills 3/12");
  const std::string text = "5\nthe_far_room kills: 1/9\n";
  CHECK_MSG(
      cl::write_save(files, "s1", {reinterpret_cast<const uint8_t*>(text.data()), text.size()},
                     error, true),
      error);
  CHECK(cl::save_label(files, "s1") == "the far room kills: 1/9");
  fs::remove_all(files.folder);
}

TEST(a_map_the_packs_do_not_have_is_named_with_the_next_step) {
  cl::SearchPath files;
  std::string error;
  CHECK(cl::load_named_map(files, "nowhere", error) == nullptr);
  CHECK(error.find("maps/nowhere.bsp") != std::string::npos);
  CHECK(cl::load_named_map(files, "arena", error) != nullptr);
}

TEST(a_game_stopped_lets_go_and_starts_again) {
  cl::Listen l;
  if (!start(l)) return;
  l.opt.console.emplace_back("timelimit 10");
  cl::listen_stop(l);
  CHECK(l.server == nullptr && !l.match && !l.map && l.progs == nullptr);
  CHECK(l.opt.console.back() == "timelimit 10");  // the options kept
  CHECK(start(l));
}

// `net_restart` and `quit`'s farewell in the window: with no match others can join (a game alone,
// or none at all) there is nothing to open again and nobody to tell.
TEST(net_restart_and_the_farewell_need_a_match_others_join) {
  cl::Listen l;
  int port = 27910;
  std::string said;
  CHECK(!cl::listen_net_restart(l, port, said));
  CHECK(said.find("No game is hosted") == 0 && port == 27910);
  cl::listen_farewell(l, "maintenance", true);  // nothing hosted: nothing happens
  if (!start(l)) return;
  said.clear();
  CHECK(!cl::listen_net_restart(l, port, said));  // played alone: no socket to open again
  CHECK(said.find("No game is hosted") == 0);
  cl::listen_farewell(l, "", false);
  CHECK(l.match != nullptr);  // the game goes on until the window closes
}

TEST(the_player_rocket_jumps_off_the_hall_floor) {
  const Rjump& r = the_jump();
  CHECK_MSG(std::fabs(r.ground_z - 24) < 0.1f, "stands at z " + std::to_string(r.ground_z));
  CHECK_MSG(r.weapon == 32,
            "the rocket launcher in hand (weapon " + std::to_string(r.weapon) + ")");
  const float jump = r.jump_peak - r.ground_z, rocket = r.rocket_peak - r.ground_z;
  CHECK_MSG(jump > 30 && jump < 60, "a plain jump rises " + std::to_string(jump) + " units");
  CHECK_MSG(rocket > 2 * jump, "the rocket jump rises " + std::to_string(rocket) +
                                   " units, a plain jump " + std::to_string(jump));
  CHECK_MSG(r.health_after < r.health_before, "the blast hurts its firer");
  CHECK(r.health_after > 0);
  CHECK(r.rockets_after == r.rockets_before - 1);
}

// The health a hard landing takes in the hall: dropped from near its ceiling already falling at
// 400 down, under the arena's `sv_falldamage` (classic remix with Quake II's physics) or not.
float fall_hurt(bool falldamage) {
  cl::Listen l;
  if (!start(l, falldamage ? std::vector<std::string>{"set sv_falldamage 1"}
                           : std::vector<std::string>{}))
    return -1;
  for (int i = 0; i < 80; i++) cl::listen_frame(l, {});
  sh::Edict& me = l.server->sv.edicts[1];
  me.origin.z = 200;
  me.velocity = {0, 0, -400};
  const float before = me.health;
  for (int i = 0; i < 60; i++) cl::listen_frame(l, {});
  return before - l.server->sv.edicts[1].health;
}

// A mod's compiled module plays the window's match in place of the program (a mod folder's
// qwprogs.qvm; classic remix's lines for weapons and physics are left out, the mod's own standing).
TEST(a_mods_compiled_module_plays_the_windows_match) {
  const std::vector<uint8_t> module = shooters_edge::server::qvm_test_mod();
  cl::Listen l;
  if (!start(l, {"sv_maxspeed 300", "set sv_falldamage 1"}, &module)) return;
  for (int i = 0; i < 20; i++) cl::listen_frame(l, {});
  CHECK(l.rules != nullptr && shooters_edge::server::qvm_of(*l.rules) != nullptr);
  CHECK_MSG(l.server->sv.edicts[1].health == 100, "the module put the player in with 100 health");
  CHECK_MSG(l.server->sv.set.move.maxspeed == 320, "a remix line reached the module's server");
}

#if defined(GE_NATIVE_ROOT)
// A mod's game library of Quake II's interface (our own, tests/q2_native_fixture_game.cpp)
// plays the window's match on a classic map: it traces the map's clip hulls, its player stands on
// the floor with its numbers, and its prints, sounds and sparks reach the window.
TEST(a_game_library_plays_the_windows_match_on_a_classic_map) {
  std::filesystem::path found;
  CHECK(shooters_edge::server::find_native_library(GE_NATIVE_ROOT, "fixturemod", "", found));
  const std::string library = found.string();
  cl::Listen l;
  if (!start(l, {}, nullptr, &library)) return;
  for (int i = 0; i < 80; i++) cl::listen_frame(l, {});
  CHECK(l.rules != nullptr && shooters_edge::server::native_q2_of(*l.rules) != nullptr);
  if (l.server == nullptr) return;
  CHECK(std::find(l.prints.begin(), l.prints.end(), "player 1 entered the game\n") !=
        l.prints.end());
  CHECK(l.server->world.clients[0].numbers.health == 100);
  const float z = l.server->sv.edicts[1].origin.z;
  for (int i = 0; i < 40; i++) cl::listen_frame(l, {});
  const float z2 = l.server->sv.edicts[1].origin.z;
  CHECK_MSG(z2 > 0 && z2 < 64 && z2 == z,
            "standing on the hall's floor: z " + std::to_string(z) + " then " + std::to_string(z2));
  l.sounds.clear();
  l.effects.clear();
  sh::MoveCmd fire;
  fire.buttons = 1;
  for (int i = 0; i < 2; i++) cl::listen_frame(l, fire);
  bool spark = false;
  for (const cl::DemoSound& d : l.sounds)
    spark = spark || l.server->sounds[static_cast<size_t>(d.sound)] == "misc/spark.wav";
  CHECK_MSG(spark, "the game's sound on attack reached the window");
  CHECK_MSG(!l.effects.empty(), "the game's spark (a temporary entity) reached the window");
  // Its own bar, run by the window in place of the classic one.
  int64_t frame = 0;
  const shooters_edge::server::RulesWorld* world = cl::q2_bar_world(l.rules, frame);
  CHECK(world != nullptr && world->status_bar == "yb -24 xv 0 hnum");
  if (world == nullptr) return;
  cl::Q2Bar bar;
  cl::run_q2_bar(bar, *world, 0, frame, 640, 480);
  CHECK_MSG(!bar.ops.empty(), "the game's bar program drew its health number");
  // The hand holds the gun the game names (Quake II's first, by its view model's name).
  cl::ViewState shown;
  cl::HeldState held;
  cl::frame_view(l, shown, 1, 1, 0.013);
  const shooters_edge::view::HandFrame hand = cl::step_held(l, shown, held, {}, {}, 1, 0.013);
  CHECK_MSG(hand.weapon == shooters_edge::view::Weapon{} && hand.lowered < 1, "the gun is up");
  const auto& nb = world->clients[0].numbers;
  CHECK(world->models[static_cast<size_t>(nb.weapon_model)] == "models/weapons/v_blast/tris.md2");
  CHECK(nb.weapon_frame == 3);
}
#endif

TEST(a_hard_landing_hurts_only_under_sv_falldamage) {
  const float hurt = fall_hurt(true);
  // 400 down and 176 more of fall: about 660 on landing, so (0.0001 x 660^2 - 30) / 2, about 6.
  CHECK_MSG(hurt >= 4 && hurt <= 8, "a hard landing took " + std::to_string(hurt));
  CHECK_MSG(fall_hurt(false) == 0, "a landing hurt with sv_falldamage off");
}

TEST(the_rocket_jump_holds_its_golden_rows) {
  const Rjump& r = the_jump();
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# games/shooters_edge/client/listen.hpp's own record: frame, the player's x y z, "
           "vz, health as float bits (tests/shooters_edge_listen_test.cpp)\n";
    for (const auto& row : r.rows) {
      for (size_t i = 0; i < row.size(); i++)
        out << (i ? " " : "") << std::hex << row[i] << std::dec;
      out << "\n";
    }
    std::printf("wrote %s (%zu rows)\n", kGolden, r.rows.size());
    return;
  }
  const auto rows = ge::test::golden_rows(kGolden);
  CHECK_MSG(rows.size() == r.rows.size(), std::string(kGolden) + " has the wrong row count");
  for (size_t i = 0; i < rows.size() && i < r.rows.size(); i++)
    CHECK_MSG(rows[i] == r.rows[i], "the rocket jump moved off its golden row at frame " +
                                        std::to_string(r.rows[i][0]) +
                                        ": if the change is meant, rewrite with "
                                        "GE_GOLDEN_WRITE=1 and say why in the PR");
}

TEST(keys_and_the_mouse_make_the_command_and_turn_the_view) {
  ge::input::InputState in;
  in.keys_held[26] = true;     // W
  in.keys_held[4] = true;      // A
  in.keys_held[44] = true;     // space
  in.keys_pressed[36] = true;  // 7
  in.mouse.buttons.held = 1;
  in.mouse.dx = 100;
  in.mouse.dy = -50;
  cl::ControlSettings c;
  const sh::MoveCmd cmd = cl::read_controls(in, c);
  CHECK(cmd.forward == 800 && cmd.side == -800);  // the keys' speeds, running (the server caps)
  CHECK(cmd.buttons == (sh::kButtonAttack | sh::kButtonJump));
  CHECK(cmd.impulse == 7);
  sh::Vec3f view{0, 10, 0};
  cl::turn_view(in, c, view);
  CHECK(std::fabs(view.y - (10 - 6.6f)) < 1e-3f);  // right is clockwise
  CHECK(std::fabs(view.x + 3.3f) < 1e-3f);         // up is a negative pitch
}

TEST(bind_defaults_brings_back_every_key_a_saved_unbindall_took) {
  ge::console::Console con;
  cl::ControlSettings c;
  cl::add_cvars(con, c);
  cl::default_binds(con);
  // A saved config that starts from no binds and keeps few (the player's binds gone).
  con.execute("unbindall; bind ` toggleconsole; bind w +back; bind k +jump");
  con.run();
  CHECK(con.binds().get(ge::console::BindMode::game, "mouse1").empty());
  con.execute("bind_defaults");
  con.run();
  CHECK(con.binds().get(ge::console::BindMode::game, "w") == "+forward");
  CHECK(con.binds().get(ge::console::BindMode::game, "mouse1") == "+attack");
  CHECK(con.binds().get(ge::console::BindMode::game, "k").empty());
  CHECK(con.binds().get(ge::console::BindMode::game, "`") == "toggleconsole");
}

TEST(the_binds_buttons_make_the_command_always_run_and_sidestep) {
  ge::console::Console con;
  cl::ControlSettings c;
  cl::add_cvars(con, c);
  cl::default_binds(con);
  CHECK(con.binds().get(ge::console::BindMode::game, "w") == "+forward");
  CHECK(con.binds().get(ge::console::BindMode::game, "mouse1") == "+attack");
  CHECK(con.binds().get(ge::console::BindMode::game, "f12") == "screenshot");
  CHECK(con.binds().get(ge::console::BindMode::game, "tab") == "+showscores");
  con.key("tab", true);  // the scores key held shows the board, released hides it
  con.run();
  CHECK(cl::scores_held(c) && !cl::team_scores_held(c));
  con.key("tab", false);
  con.execute("+showteamscores");
  con.run();
  CHECK(!cl::scores_held(c) && cl::team_scores_held(c));
  con.execute("-showteamscores");
  con.execute("bind w +back");  // the player's own bind is kept over the default
  cl::default_binds(con);
  CHECK(con.binds().get(ge::console::BindMode::game, "w") == "+back");
  con.key("w", true);
  con.key("mouse1", true);
  con.key("7", true);
  con.run();
  ge::input::InputState in;
  sh::MoveCmd cmd = cl::read_controls(cl::button_moves(c), in, c);
  CHECK(cmd.forward == -800);  // always running: cl_backspeed times cl_movespeedkey
  CHECK(cmd.buttons == sh::kButtonAttack);
  CHECK(cmd.impulse == 7);
  CHECK(cl::button_moves(c).impulse == 0);  // the impulse is sent once
  // Nobody walks: cl_run is on by default and no key walks (+speed is bound to nothing); a config
  // that binds it walks at the keys' own speeds.
  CHECK(con.cvars().find("cl_run") != nullptr);
  CHECK(con.binds().get(ge::console::BindMode::game, "shift").empty());
  con.key("shift", true);
  con.run();
  CHECK(cl::read_controls(cl::button_moves(c), in, c).forward == -800);
  con.key("shift", false);
  con.execute("bind shift +speed");
  con.key("shift", true);
  con.run();
  CHECK(cl::read_controls(cl::button_moves(c), in, c).forward == -400);
  con.key("shift", false);
  con.run();
  // No crouch in Quake: C is bound to nothing; +movedown swims down where bound.
  CHECK(con.binds().get(ge::console::BindMode::game, "c").empty());
  con.execute("bind c +movedown");
  con.key("c", true);
  con.run();
  CHECK(cl::read_controls(cl::button_moves(c), in, c).up == -800);
  con.key("c", false);
  con.run();
  CHECK(cl::read_controls(cl::button_moves(c), in, c).up == 0);
  // +moveup swims up, and in Quake II's move an up of 10 or more on the ground is its jump.
  con.execute("bind v +moveup");
  con.key("v", true);
  con.run();
  CHECK(cl::read_controls(cl::button_moves(c), in, c).up >= 10);
  con.key("v", false);
  con.run();
  CHECK(cl::read_controls(cl::button_moves(c), in, c).up == 0);
  // lookstrafe: the mouse's sideways counts sidestep and leave the yaw alone.
  in.mouse.dx = 50;
  sh::Vec3f view{0, 90, 0};
  con.execute("lookstrafe 1");
  const cl::Moves m = cl::button_moves(c);
  cl::turn_view(in, m, c, 0.01, view);
  CHECK(view.y == 90);
  CHECK(cl::read_controls(m, in, c).side > 0);
  // m_side scales it: 50 counts at sensitivity s and m_side 2 are 100 s units sideways.
  con.execute("m_side 2");
  const double sens = c.sensitivity;
  CHECK(cl::read_controls(m, in, c).side == static_cast<int>(50 * sens * 2));
  con.execute("m_side 0.8");
  // The menu took the keys: everything let go.
  cl::release_buttons(c);
  CHECK(cl::read_controls(cl::button_moves(c), {}, c).forward == 0);
  // +left turns at cl_yawspeed.
  con.execute("lookstrafe 0");
  con.key("left", true);
  con.run();
  sh::Vec3f turned{0, 90, 0};
  cl::turn_view({}, cl::button_moves(c), c, 0.5, turned);
  CHECK(std::fabs(turned.y - 160) < 1e-3f);
  con.key("left", false);
  // The classic keys' speeds and looking: cl_forwardspeed, +lookdown at cl_pitchspeed, +klook's
  // keys look, freelook 0 moves you with the mouse's up and down, centerview levels the view.
  cl::release_buttons(c);
  con.execute("cl_forwardspeed 200; cl_movespeedkey 1.5; +forward");
  con.run();
  CHECK(cl::read_controls(cl::button_moves(c), {}, c).forward == 300);
  con.execute("+klook");
  sh::Vec3f looked{0, 90, 0};
  cl::Moves km = cl::button_moves(c);
  CHECK(cl::read_controls(km, {}, c).forward == 0);
  cl::turn_view({}, km, c, 0.1, looked);
  CHECK(std::fabs(looked.x + 15) < 1e-3f);  // up at 150 degrees a second
  con.execute("-klook; -forward");
  con.execute("+lookdown");
  cl::turn_view({}, cl::button_moves(c), c, 0.1, looked);
  CHECK(std::fabs(looked.x) < 1e-3f);
  con.execute("-lookdown; freelook 0");
  ge::input::InputState mouse;
  mouse.mouse.dy = -10;
  cl::Moves mm = cl::button_moves(c);
  cl::turn_view(mouse, mm, c, 0.01, looked);
  CHECK(std::fabs(looked.x) < 1e-3f);                    // no look up
  CHECK(cl::read_controls(mm, mouse, c).forward == 30);  // moved: 10 counts, sensitivity 3
  looked.x = 20;
  con.execute("centerview");
  cl::turn_view({}, cl::button_moves(c), c, 0.01, looked);
  CHECK(looked.x == 0);
}

TEST(setup_s_name_and_colours_reach_the_server_as_they_change) {
  cl::Listen l;
  if (!start(l)) return;
  ge::console::Console con;
  con.cvars().add({.name = "name", .fallback = "player", .kind = ge::console::CvarKind::text});
  cl::UserInfo info;
  cl::add_user_info_cvars(con, info);
  con.execute("name ranger; topcolor 4; bottomcolor 12");
  cl::send_user_info(l, con, info);
  const auto& ui = l.server->clients[0].userinfo;
  CHECK(shooters_edge::server::info_value(ui, "name") == "ranger");
  CHECK(shooters_edge::server::info_value(ui, "topcolor") == "4");
  CHECK(shooters_edge::server::info_value(ui, "bottomcolor") == "12");
  CHECK(l.opt.player == "ranger");  // the next map's connect uses it
  con.execute("topcolor 99");       // clamped to the 14 colours
  cl::send_user_info(l, con, info);
  CHECK(shooters_edge::server::info_value(l.server->clients[0].userinfo, "topcolor") == "13");
}

TEST(the_hud_reads_the_launcher_and_its_rockets) {
  cl::Listen l;
  if (!start(l)) return;
  for (int i = 0; i < 80; i++) cl::listen_frame(l, {});
  l.impulse = 7;
  for (int i = 0; i < 40; i++) cl::listen_frame(l, {});
  const shooters_edge::hud::Status s = cl::status_of(l, 1.0);
  CHECK(s.weapon == shooters_edge::hud::Weapon::rocket_launcher);
  CHECK(s.has(shooters_edge::hud::Weapon::rocket_launcher));
  CHECK(s.ammo[2] > 0);
  CHECK(s.health == 100);
  ge::camera::FirstPersonRow row;
  const ge::camera::Subject sub = cl::subject_of(l, row);
  CHECK(std::fabs(sub.pos.y) < 0.01);  // the feet on the floor at z 0
  CHECK(row.eye_stand_m > 1.0 && row.eye_stand_m < 1.3);
  CHECK(std::fabs(sub.look_yaw - 1.5707963) < 1e-3);  // facing east (+x) is heading pi/2
}

namespace {

void set_field(cl::Listen& l, int entity, const char* name, float value) {
  const ge::progs::Def* d = ge::progs::find_field(*l.progs, name);
  CHECK_MSG(d != nullptr, std::string("the baseline mod has no field ") + name);
  if (d != nullptr) {
    *ge::progs::entity_field(shooters_edge::server::qc_of(*l.rules)->q.vm, entity, d->offset) =
        std::bit_cast<uint32_t>(value);
  }
}

}  // namespace

TEST(the_powers_glow_round_their_holder_the_shield_takes_the_blast_and_both_run_out) {
  cl::Listen l;
  if (!start(l)) return;
  for (int i = 0; i < 80; i++) cl::listen_frame(l, {});
  l.impulse = 7;
  for (int i = 0; i < 80; i++) cl::listen_frame(l, {});
  const float now = static_cast<float>(l.server->sv.time);
  set_field(l, 1, "super_damage_finished", now + 30);
  set_field(l, 1, "invincible_finished", now + 30);
  cl::listen_frame(l, {});
  // EF_DIMLIGHT | EF_BLUE | EF_RED: the quad's blue and the shield's red round him.
  CHECK(static_cast<int>(cl::field_named(l, 1, "effects")) == (8 | 64 | 128));
  // A rocket at his own feet takes nothing off him under the shield.
  const float health = l.server->sv.edicts[1].health;
  l.view.x = 89;
  sh::MoveCmd fire;
  fire.buttons = sh::kButtonAttack;
  for (int i = 0; i < 2; i++) cl::listen_frame(l, fire);
  for (int i = 0; i < 60; i++) cl::listen_frame(l, {});
  CHECK_MSG(l.server->sv.edicts[1].health == health,
            "the shield let the blast through: health " +
                std::to_string(l.server->sv.edicts[1].health) + " of " + std::to_string(health));
  // Both run out: the glow goes with them.
  const float later = static_cast<float>(l.server->sv.time);
  set_field(l, 1, "super_damage_finished", later + 0.05f);
  set_field(l, 1, "invincible_finished", later + 0.05f);
  for (int i = 0; i < 10; i++) cl::listen_frame(l, {});
  CHECK(static_cast<int>(cl::field_named(l, 1, "effects")) == 0);
}

TEST(the_hand_raises_the_launcher_and_sees_one_shot_a_rocket) {
  cl::Listen l;
  if (!start(l)) return;
  cl::ViewState shown;
  cl::HeldState held;
  const shooters_edge::view::ViewSettings settings;
  constexpr double kTic = 0.013;
  double t = 0;
  shooters_edge::view::HandFrame f;
  const auto run = [&](int frames, sh::MoveCmd cmd) {
    for (int i = 0; i < frames; i++) {
      cl::listen_frame(l, cmd);
      cmd.impulse = 0;
      t += kTic;
      cl::frame_view(l, shown, 1, t, kTic);
      f = cl::step_held(l, shown, held, settings, {}, t, kTic);
    }
  };
  run(80, {});
  CHECK(held.shots == 0);  // the spawn sets attack_finished to now: no shot
  l.impulse = 7;
  run(80, {});
  CHECK(f.weapon == shooters_edge::view::Weapon::rocket_launcher);
  CHECK(!f.switching);
  sh::MoveCmd fire;
  fire.buttons = sh::kButtonAttack;
  run(1, fire);
  run(40, {});
  CHECK(held.shots == 1);
}

// The baseline mod's body cycles (common/player.qc): the run cycle (6-11, a gun in hand) while he
// moves across the plan, a frame every 0.1 s; the stand cycle (12-16) once he is still. The rule
// reads the speed across the plan alone, so a swimmer runs as a walker does (lab se-swim).
TEST(the_player_s_body_runs_while_he_moves_and_stands_when_still) {
  cl::Listen l;
  if (!start(l)) return;
  const auto frame = [&] { return static_cast<int>(cl::field_named(l, 1, "frame")); };
  for (int i = 0; i < 80; i++) cl::listen_frame(l, {});
  CHECK(frame() >= 12 && frame() <= 16);
  sh::MoveCmd walk;
  walk.forward = 200;
  std::vector<int> seen;
  for (int i = 0; i < 60; i++) {  // 0.78 s: the cycle round once and on
    cl::listen_frame(l, walk);
    if (seen.empty() || seen.back() != frame()) seen.push_back(frame());
  }
  CHECK(seen.size() >= 7);
  for (size_t k = 1; k < seen.size(); k++) CHECK(seen[k] >= 6 && seen[k] <= 11);
  for (int i = 0; i < 80; i++) cl::listen_frame(l, {});
  CHECK(frame() >= 12 && frame() <= 16);
}

TEST(a_demo_s_hand_holds_the_recorder_s_weapon_none_without_a_view_model_and_sinks_dead) {
  cl::DemoState s;
  s.stats[cl::kStatHealth] = 100;
  s.stats[cl::kStatWeapon] = 5;         // a view model's index
  s.stats[cl::kStatActiveWeapon] = 32;  // the rocket launcher's item bit
  cl::HeldState held;
  const shooters_edge::view::ViewSettings settings;
  const ge::render::FrameView frame;
  const auto step = [&] { return cl::step_demo_held(s, held, settings, {}, frame, 0, 0.013); };
  const shooters_edge::view::HandFrame f = step();
  CHECK(f.drawn);
  CHECK(f.weapon == shooters_edge::view::Weapon::rocket_launcher);
  s.stats[cl::kStatWeapon] = 0;  // a spectator's recording
  CHECK(!step().drawn);
  s.stats[cl::kStatWeapon] = 5;
  for (int i = 0; i < 60; i++) step();  // up again
  s.stats[cl::kStatHealth] = 0;         // dead: the weapon sinks, then is gone
  s.stats[cl::kStatWeapon] = 0;         // the recording clears it at once
  CHECK(step().drawn);
  for (int i = 0; i < 60; i++) step();  // 0.78 s, past kDeathLowerS
  CHECK(!step().drawn);
  s.stats[cl::kStatHealth] = 100;
  s.stats[cl::kStatWeapon] = 5;
  s.intermission = true;
  CHECK(!step().drawn);
}

TEST(a_predicted_shot_or_the_server_s_shot_sound_fires_the_hand_once) {
  // net/qw_weapon.hpp counts each predicted shot in own_shots: the hand fires on each, once.
  cl::DemoState s;
  s.stats[cl::kStatHealth] = 100;
  s.stats[cl::kStatWeapon] = 5;
  s.stats[cl::kStatActiveWeapon] = 8;  // the super nailgun
  s.format = cl::DemoFormat::client;
  s.players.resize(32);
  cl::HeldState held;
  const shooters_edge::view::ViewSettings settings;
  const ge::render::FrameView frame;
  const auto step = [&] { return cl::step_demo_held(s, held, settings, {}, frame, 0, 0.013); };
  CHECK(!step().fired);
  s.own_shots = 1;
  CHECK(step().fired);
  CHECK(!step().fired);
  s.own_shots = 3;  // two in one window frame: one shot drawn
  CHECK(step().fired);
  // Unpredicted, under rules that never animate the weapon: the server's weapon sound on the
  // player fires the hand, once per frame read (a Quake join, a demo of our server).
  s.sounds = {"", "weapons/spike2.wav", "player/axhit2.wav"};
  cl::DemoSound snd;
  snd.entity = 1;
  snd.channel = 1;
  snd.sound = 2;  // not a shot
  s.started = {snd};
  s.frames = 10;
  CHECK(!step().fired);
  s.started[0].sound = 1;
  s.frames = 11;
  CHECK(step().fired);
  CHECK(!step().fired);  // the same frame read: not again
  // A server whose rules never send a weapon frame other than 0 leaves the frames to the hand.
  CHECK(!held.frames_live);
  s.players[0].weapon_frame = 2;
  step();
  CHECK(held.frames_live);
}

TEST(r_viewmodellastfired_keeps_a_recording_s_last_fired_weapon_while_another_is_looked_at) {
  cl::DemoState s;
  s.stats[cl::kStatHealth] = 100;
  s.stats[cl::kStatWeapon] = 5;
  s.stats[cl::kStatActiveWeapon] = 32;  // the rocket launcher
  s.format = cl::DemoFormat::single;
  cl::HeldState held;
  held.recording = true;
  shooters_edge::view::ViewSettings settings;
  settings.last_fired = true;
  const ge::render::FrameView frame;
  const auto step = [&] { return cl::step_demo_held(s, held, settings, {}, frame, 0, 0.013); };
  s.weapon_frame = 3;  // firing
  CHECK(step().weapon == shooters_edge::view::Weapon::rocket_launcher);
  s.weapon_frame = 0;
  s.stats[cl::kStatActiveWeapon] = 1;  // the shotgun looked at, not fired
  CHECK(held.last_fired == static_cast<int>(shooters_edge::view::Weapon::rocket_launcher));
  CHECK(step().weapon == shooters_edge::view::Weapon::rocket_launcher);
  s.weapon_frame = 1;  // the shotgun fires: it is shown from now on (switched to)
  for (int i = 0; i < 40; i++) step();
  CHECK(held.last_fired == static_cast<int>(shooters_edge::view::Weapon::shotgun));
  s.weapon_frame = 0;
  for (int i = 0; i < 40; i++) step();
  CHECK(step().weapon == shooters_edge::view::Weapon::shotgun);
  // Off, or a server joined: the weapon held.
  s.stats[cl::kStatActiveWeapon] = 32;
  held.recording = false;
  for (int i = 0; i < 40; i++) step();
  CHECK(step().weapon == shooters_edge::view::Weapon::rocket_launcher && held.last_fired == -1);
  // A stuffed text's item-taken lines.
  CHECK(cl::bonus_lines("bf\n") == 1 && cl::bonus_lines("bf\nbf\n") == 2);
  CHECK(cl::bonus_lines("bfx\n") == 0 && cl::bonus_lines("say bf\n") == 0);
}

TEST(the_hud_counts_down_the_quad_and_the_shield_and_drops_them_when_they_run_out) {
  cl::Listen l;
  if (!start(l)) return;
  for (int i = 0; i < 80; i++) cl::listen_frame(l, {});
  namespace hud = shooters_edge::hud;
  CHECK(!cl::status_of(l, 1.0).powered(hud::Power::damage));
  CHECK(!cl::status_of(l, 1.0).powered(hud::Power::shield));
  const float now = static_cast<float>(l.server->sv.time);
  const float items = cl::field_named(l, 1, "items");
  set_field(l, 1, "items", items + 1048576 + 4194304);  // IT_INVULNERABILITY + IT_QUAD
  set_field(l, 1, "super_damage_finished", now + 30);
  set_field(l, 1, "invincible_finished", now + 20);
  cl::listen_frame(l, {});
  const hud::Status s = cl::status_of(l, 1.0);
  CHECK(s.powered(hud::Power::damage) && s.powered(hud::Power::shield));
  const double quad = s.power_s[static_cast<size_t>(hud::Power::damage)];
  const double shield = s.power_s[static_cast<size_t>(hud::Power::shield)];
  CHECK_MSG(quad > 29 && quad <= 30, "the quad's seconds: " + std::to_string(quad));
  CHECK_MSG(shield > 19 && shield <= 20, "the shield's seconds: " + std::to_string(shield));
  // Both run out: the mod takes the bits off, and the HUD drops them.
  const float later = static_cast<float>(l.server->sv.time);
  set_field(l, 1, "super_damage_finished", later + 0.05f);
  set_field(l, 1, "invincible_finished", later + 0.05f);
  for (int i = 0; i < 10; i++) cl::listen_frame(l, {});
  const hud::Status gone = cl::status_of(l, 2.0);
  CHECK(!gone.powered(hud::Power::damage) && !gone.powered(hud::Power::shield));
}

TEST(a_mods_named_effects_reach_the_player_by_name_or_as_the_classic_effect) {
  cl::Listen l;
  if (!start(l)) return;
  for (int i = 0; i < 10; i++) cl::listen_frame(l, {});
  const size_t before = l.effects.size(), booms = l.explosions.size();
  const auto event = [](shooters_edge::server::EventKind k, const char* name) {
    shooters_edge::server::NetEvent ev;
    ev.kind = k;
    ev.to = 1;
    ev.origin = {64, 0, 40};
    ev.values[2] = 1;
    ev.values[3] = 3;
    ev.text = name;
    return ev;
  };
  namespace sv = shooters_edge::server;
  l.server->out.events.push_back(event(sv::EventKind::point_effect, "spark_fountain"));
  l.server->out.events.push_back(event(sv::EventKind::point_effect, "TE_GUNSHOT"));
  l.server->out.events.push_back(event(sv::EventKind::point_effect, "te_explosion"));
  l.server->out.events.push_back(event(sv::EventKind::trail_effect, "spark_trail"));
  sv::NetEvent other = event(sv::EventKind::point_effect, "for_someone_else");
  other.to = 2;
  l.server->out.events.push_back(other);
  cl::listen_frame(l, {});
  CHECK_MSG(l.effects.size() == before + 3, std::to_string(l.effects.size() - before));
  if (l.effects.size() != before + 3) return;
  const cl::Effect& named = l.effects[before];
  CHECK(named.kind == cl::EffectKind::named && named.name == "spark_fountain" && named.count == 3 &&
        named.way.z == 1.0f && named.at.x == 64.0f);
  CHECK(l.effects[before + 1].kind == cl::EffectKind::impact && l.effects[before + 1].count == 3);
  const cl::Effect& trail = l.effects[before + 2];
  CHECK(trail.kind == cl::EffectKind::named_trail && trail.name == "spark_trail" &&
        trail.end.y == 0.0f);
  CHECK(l.explosions.size() == booms + 1);
}

TEST(the_window_hears_its_own_game_the_launcher_picked_up_and_fired) {
  const Rjump& r = the_jump();
  const auto heard = [&r](std::string_view part) {
    return std::any_of(r.sounds.begin(), r.sounds.end(),
                       [part](const std::string& s) { return s.find(part) != std::string::npos; });
  };
  CHECK_MSG(!r.sounds.empty(), "the server started no sound for the player");
  CHECK_MSG(heard("weapons/"), "no weapon's sound among " + std::to_string(r.sounds.size()));
  // Played: every name on the sound list a short tone in the packs, the same game heard.
  cl::Listen l;
  if (!start(l)) return;
  std::vector<float> tone(2400);
  for (size_t i = 0; i < tone.size(); i++) tone[i] = (i / 24) % 2 == 0 ? 0.2f : -0.2f;
  std::vector<std::pair<std::string, std::vector<uint8_t>>> entries;
  for (size_t i = 1; i < l.server->sounds.size(); i++)
    entries.push_back({"sound/" + l.server->sounds[i], ge::audio::wav_bytes(tone, 1, 22050)});
  std::vector<uint8_t> pak;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak(entries, &pak, &error), error);
  cl::SearchPath files;
  CHECK_MSG(cl::add_archive(files, "pak0.pak", std::move(pak), error), error);
  ge::audio::Synth synth;
  cl::ListenHearing hearing(synth);
  const auto run = [&](int frames, sh::MoveCmd cmd) {
    for (int i = 0; i < frames; i++) {
      cl::listen_frame(l, cmd);
      cmd.impulse = 0;
      hearing.frame(l, files, 1.0f / 77);
      CHECK(l.sounds.empty());  // played and emptied
    }
  };
  run(80, {});
  l.impulse = 7;
  run(80, {});
  sh::MoveCmd fire;
  fire.buttons = sh::kButtonAttack;
  run(2, fire);
  run(40, {});
  const cl::DemoSoundStats* st = hearing.stats();
  CHECK(st != nullptr);
  if (st == nullptr) return;
  CHECK_MSG(st->started > 0, "nothing played");
  CHECK_MSG(st->missing == 0, std::to_string(st->missing) + " recordings missing");
  CHECK(hearing.state().sounds == l.server->sounds);
  const sh::Edict& me = l.server->sv.edicts[1];
  CHECK(hearing.state().eye.z > me.origin.z);  // the listener at the eye, above the origin
  cl::listen_stop(l);
  hearing.frame(l, files, 0.01f);  // no game: everything stopped
  CHECK(hearing.stats() == nullptr);
}

TEST(the_campaign_s_looping_sounds_set_off_at_the_map_s_load_are_heard) {
  auto map = std::make_unique<cl::LoadedMap>();
  map->name = map->spec.name = "first";
  map->spec.models = shooters_edge::server::arena_models(true);
  map->spec.entities = R"({
"classname" "worldspawn"
}
{
"classname" "info_player_start"
"origin" "-416 0 24"
}
{
"classname" "ambient_drip"
"origin" "-300 0 64"
}
)";
  cl::Listen l;
  cl::ListenOptions opt = shooters_edge::net::listen_options();
  opt.console = {"deathmatch 0"};
  std::string error;
  CHECK_MSG(cl::listen_start(l, campfix::mod(), std::move(map), opt, error), error);
  if (l.server == nullptr) return;
  CHECK_MSG(l.static_sounds.size() == 1, std::to_string(l.static_sounds.size()) + " loops");
  if (l.static_sounds.size() != 1) return;
  const cl::DemoStaticSound& drip = l.static_sounds[0];
  CHECK(l.server->sounds[static_cast<size_t>(drip.sound)].find("drip") != std::string::npos);
  CHECK(drip.origin.x == -300.0f && drip.volume > 0 && drip.attenuation > 0);
}

// A floored hall of brushes walled round with a start in the middle and a co-op start beside it.
std::vector<uint8_t> brush_hall() {
  br::BrushWorld world;
  world.models.resize(1);
  const auto box = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    world.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/wall"));
  };
  box(-512, -512, -16, 512, 512, 0);
  box(-528, -512, -16, -512, 512, 256);
  box(512, -512, -16, 528, 512, 256);
  box(-528, 512, -16, 528, 528, 256);
  box(-528, -528, -16, 528, -512, 256);
  world.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "0 0 24"}, {"angle", "90"}}));
  world.entities.push_back(
      entity({{"classname", "info_player_coop"}, {"origin", "96 0 24"}, {"angle", "90"}}));
  std::vector<uint8_t> bsp;
  std::string error;
  CHECK_MSG(br::write_brush_world(world, &bsp, &error), error);
  return bsp;
}

// On Quake II's rules outside deathmatch (a single-player or coop game) a level change keeps the
// server, so the body the player leaves with is the one he enters the next level with; in
// deathmatch every map's rules are new and the body starts afresh.
TEST(a_coop_level_change_on_brush_maps_carries_the_players_body) {
  std::string error;
  cl::SearchPath files;
  const std::vector<uint8_t> hall = brush_hall();
  CHECK(cl::add_archive(files, "pak0.pak", pack({{"maps/hall.bsp", hall}, {"maps/next.bsp", hall}}),
                        error));
  const int shotgun = shooters_edge::server::sq_item("weapon_shotgun");
  for (const bool coop : {true, false}) {
    cl::ListenOptions opt = shooters_edge::net::listen_options();
    opt.console = coop ? std::vector<std::string>{"deathmatch 0", "coop 1"}
                       : std::vector<std::string>{"deathmatch 1"};
    cl::Listen l;
    std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, "hall", error);
    CHECK_MSG(map != nullptr, error);
    if (!map) return;
    CHECK_MSG(cl::listen_start(l, baseline(), std::move(map), opt, error), error);
    if (l.rules == nullptr) return;
    for (int i = 0; i < 30; i++) cl::listen_frame(l, {});
    shooters_edge::server::Q2Server* before = shooters_edge::server::q2_of(*l.rules);
    CHECK(before != nullptr);
    if (before == nullptr) return;
    before->players[0].body.counts[shotgun] = 1;
    CHECK(l.rules->console("changelevel next"));
    for (int i = 0; i < 30 && l.pending_map.empty(); i++) cl::listen_frame(l, {});
    CHECK(l.pending_map == "next");
    std::unique_ptr<cl::LoadedMap> next = cl::load_named_map(files, "next", error);
    CHECK_MSG(next != nullptr, error);
    if (!next) return;
    CHECK_MSG(cl::listen_change(l, std::move(next), error), error);
    for (int i = 0; i < 30; i++) cl::listen_frame(l, {});
    CHECK(l.map->name == "next");
    shooters_edge::server::Q2Server* after = shooters_edge::server::q2_of(*l.rules);
    CHECK(after != nullptr);
    if (after == nullptr) return;
    CHECK_MSG((after == before) == coop, coop ? "coop: new rules" : "deathmatch: rules kept");
    CHECK(l.server->clients[0].active && after->players[0].alive);
    CHECK_MSG(after->players[0].body.counts[shotgun] == (coop ? 1 : 0),
              std::string(coop ? "coop" : "deathmatch") + ": shotguns " +
                  std::to_string(after->players[0].body.counts[shotgun]));
  }
}

// In Quake II's single-player game `addbot` seats a companion beside the player: the game turns
// cooperative for it, the companion keeps near him as he walks off, stays seated through a level
// change (the same rules), and the last one sent away makes the game single player again.
TEST(a_companion_joins_q2_s_single_player_game_and_follows_through_a_level_change) {
  std::string error;
  cl::SearchPath files;
  const std::vector<uint8_t> hall = brush_hall();
  CHECK(cl::add_archive(files, "pak0.pak", pack({{"maps/hall.bsp", hall}, {"maps/next.bsp", hall}}),
                        error));
  cl::ListenOptions opt = shooters_edge::net::listen_options();
  opt.console = {"deathmatch 0", "coop 0"};
  cl::Listen l;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, "hall", error);
  CHECK_MSG(map != nullptr, error);
  if (!map) return;
  CHECK_MSG(cl::listen_start(l, baseline(), std::move(map), opt, error), error);
  if (l.rules == nullptr) return;
  for (int i = 0; i < 10; i++) cl::listen_frame(l, {});
  const auto said = [&](const std::string& part) {
    for (const std::string& line : l.prints)
      if (line.find(part) != std::string::npos) return true;
    return false;
  };
  const auto coop = [&] { return l.server->cvars.find("coop")->on(); };
  CHECK(!coop());
  CHECK(cl::listen_lobby(l, "addbot expert"));
  for (int i = 0; i < 4; i++) cl::listen_frame(l, {});
  CHECK_MSG(said("cooperative while you have company"), "no word that the game turned coop");
  CHECK_MSG(said("mate1 (expert) joins the game"), "no word of the companion joining");
  CHECK(coop());
  CHECK(l.server->clients[1].active);
  // The player walks north a while; the companion keeps up.
  sh::MoveCmd walk;
  walk.forward = 400;
  for (int i = 0; i < 160; i++) cl::listen_frame(l, walk);
  for (int i = 0; i < 160; i++) cl::listen_frame(l, {});
  const sh::Vec3f me = l.server->sv.edicts[1].origin, mate = l.server->sv.edicts[2].origin;
  const sh::Vec3f start{0, 0, 24};
  CHECK_MSG(me.y - start.y > 200, "the player walked " + std::to_string(me.y - start.y));
  const float dx = me.x - mate.x, dy = me.y - mate.y;
  const float apart = std::sqrt(dx * dx + dy * dy);
  CHECK_MSG(apart < 300, "the companion is " + std::to_string(apart) + " units off");
  // A level change on the same rules keeps it seated.
  CHECK(l.rules->console("changelevel next"));
  for (int i = 0; i < 30 && l.pending_map.empty(); i++) cl::listen_frame(l, {});
  std::unique_ptr<cl::LoadedMap> next = cl::load_named_map(files, "next", error);
  CHECK_MSG(next != nullptr, error);
  if (!next) return;
  CHECK_MSG(cl::listen_change(l, std::move(next), error), error);
  for (int i = 0; i < 30; i++) cl::listen_frame(l, {});
  CHECK(l.map->name == "next");
  CHECK(l.server->clients[1].active);
  CHECK(cl::listen_lobby(l, "removebot"));
  for (int i = 0; i < 4; i++) cl::listen_frame(l, {});
  CHECK(!l.server->clients[1].active);
  CHECK(!coop());
  CHECK_MSG(said("You play alone again."), "no word that the game is single player again");
}

// A map of the family that carries brushes (version 38) plays on the rules that trace them: the
// window's listen server runs Quake II's runner on it whatever the program, the player spawns at
// the map's start facing its way, stands on its floor and walks across it that way, the turn the
// spawn gave him taken once by the window's view (docs/goals/M18v.md, M18v2).
TEST(a_map_of_brushes_plays_on_the_rules_that_trace_brushes_and_the_player_walks_it) {
  // A floored hall walled round, its brushes solid, a start in the middle.
  br::BrushWorld world;
  world.models.resize(1);
  const auto box = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    world.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/wall"));
  };
  box(-512, -512, -16, 512, 512, 0);
  box(-528, -512, -16, -512, 512, 256);
  box(512, -512, -16, 528, 512, 256);
  box(-528, 512, -16, 528, 528, 256);
  box(-528, -528, -16, 528, -512, 256);
  br::Entity start;
  start.pairs = {{"classname", "info_player_start"}, {"origin", "0 0 24"}, {"angle", "90"}};
  world.entities.push_back(start);
  std::vector<uint8_t> bsp;
  std::string error;
  CHECK_MSG(br::write_brush_world(world, &bsp, &error), error);
  cl::SearchPath files;
  CHECK(cl::add_archive(files, "pak0.pak", pack({{"maps/fixture.bsp", bsp}}), error));
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, "fixture", error);
  CHECK_MSG(map != nullptr, error);
  if (!map) return;
  CHECK(map->spec.brushes != nullptr && map->spec.models.empty());
  cl::Listen l;
  CHECK_MSG(
      cl::listen_start(l, baseline(), std::move(map), shooters_edge::net::listen_options(), error),
      error);
  if (l.rules == nullptr) return;
  CHECK(shooters_edge::server::q2_of(*l.rules) != nullptr);
  const auto me = [&] { return l.server->world.entities[1]; };
  for (int i = 0; i < 120; i++) cl::listen_frame(l, {});  // spawned, fallen onto the floor
  CHECK(!me().free);
  const sh::Vec3f stood = me().origin;
  CHECK((me().flags & sh::kFlagOnGround) != 0);
  sh::MoveCmd walk;
  walk.forward = 200;
  for (int i = 0; i < 60; i++) cl::listen_frame(l, walk);
  const sh::Vec3f walked = me().origin;
  const float dx = walked.x - stood.x, dy = walked.y - stood.y;
  // Along the start's facing (+y): the turn the spawn gave him is taken once, not every frame.
  CHECK_MSG(
      dy > 100.0f && std::fabs(dx) < 1.0f,
      "the player did not walk the start's way: " + std::to_string(dx) + ", " + std::to_string(dy));
  CHECK(std::fabs(walked.z - stood.z) < 1.0f);  // on the same floor
}

// A floor with a plat on Quake II's rules, lowered at the start (its top 8 up), the player
// started at `origin`.
bool open_lift_hall(cl::Listen& l, const char* origin) {
  br::BrushWorld world;
  world.models.resize(2);
  const auto box = [&](size_t m, float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    world.models[m].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/wall"));
  };
  box(0, -512, -512, -16, 512, 512, 0);
  box(1, -48, -48, 0, 48, 48, 136);
  br::Entity start, plat;
  start.pairs = {{"classname", "info_player_start"}, {"origin", origin}, {"angle", "90"}};
  plat.pairs = {{"classname", "func_plat"}, {"model", "*1"}};
  world.entities = {start, plat};
  std::vector<uint8_t> bsp;
  std::string error;
  CHECK_MSG(br::write_brush_world(world, &bsp, &error), error);
  cl::SearchPath files;
  CHECK(cl::add_archive(files, "pak0.pak", pack({{"maps/fixture.bsp", bsp}}), error));
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, "fixture", error);
  CHECK_MSG(map != nullptr, error);
  if (!map) return false;
  CHECK_MSG(
      cl::listen_start(l, baseline(), std::move(map), shooters_edge::net::listen_options(), error),
      error);
  return l.rules != nullptr;
}

// A lift on Quake II's rules moves only in their frames (ten a second), the rider with it; the
// window spreads each frame's rise over the frame, so the drawn eye climbs a little every window
// frame, never standing still between jumps (docs/goals/M18v.md, the lift's ride).
TEST(the_eye_rides_a_q2_lift_up_smoothly) {
  cl::Listen l;
  if (!open_lift_hall(l, "0 0 40")) return;
  cl::ViewState shown;
  constexpr double kTic = 0.013;
  double t = 0;
  std::vector<double> eye;
  eye.reserve(400);
  for (int i = 0; i < 400; i++) {
    cl::listen_frame(l, {});
    t += kTic;
    eye.push_back(cl::frame_view(l, shown, 1, t, kTic).eye[1]);
  }
  CHECK_MSG(eye.back() - eye.front() > 2.5,
            "the lift did not carry him up: " + std::to_string(eye.back() - eye.front()) + " m");
  // The ride: from the first frame that rises to the last.
  size_t first = 1, last = eye.size() - 1;
  while (first < eye.size() && eye[first] - eye[first - 1] < 1e-4) first++;
  while (last > first && eye[last] - eye[last - 1] < 1e-4) last--;
  int still = 0;
  double biggest = 0;
  for (size_t i = first + 1; i < last; i++) {
    const double rise = eye[i] - eye[i - 1];
    if (rise < 1e-4) still++;
    biggest = std::max(biggest, rise);
  }
  const double mean = (eye[last] - eye[first]) / double(last - first);
  CHECK_MSG(still == 0, std::to_string(still) + " drawn frames stood still mid-ride");
  CHECK_MSG(biggest < 2.5 * mean, "a jump of " + std::to_string(biggest) + " m against a mean of " +
                                      std::to_string(mean) + " m a frame");
}

// Quake II's rules roll the view as he sidesteps, a tenth at a time; the window eases each
// tenth's roll across it, so the drawn roll leans a little every window frame.
TEST(the_view_rolls_smoothly_as_he_sidesteps_on_q2_s_rules) {
  cl::Listen l;
  if (!open_lift_hall(l, "-300 0 24")) return;
  cl::ViewState shown;
  constexpr double kTic = 0.013;
  double t = 0;
  const auto frame = [&](sh::MoveCmd cmd) {
    cl::listen_frame(l, cmd);
    t += kTic;
    return cl::frame_view(l, shown, 1, t, kTic).roll;
  };
  for (int i = 0; i < 120; i++) frame({});  // spawned, on the floor
  sh::MoveCmd step;
  step.side = 400;
  std::vector<double> roll(40);
  for (double& r : roll) r = frame(step);
  const double full = roll.back() - roll.front();
  CHECK_MSG(std::fabs(full) > 0.01, "no roll: " + std::to_string(full) + " rad");
  // Leaning in: from the first frame that moves to the first at its full lean.
  size_t first = 1, last = 1;
  while (first < roll.size() && std::fabs(roll[first] - roll[first - 1]) < 1e-6) first++;
  for (last = first; last + 1 < roll.size(); last++)
    if (std::fabs(roll[last] - roll.back()) < 1e-6) break;
  double biggest = 0;
  for (size_t i = first; i <= last; i++)
    biggest = std::max(biggest, std::fabs(roll[i] - roll[i - 1]));
  CHECK_MSG(last > first + 4 && biggest < 0.5 * std::fabs(full),
            "the roll came in " + std::to_string(last - first + 1) + " frames, the biggest step " +
                std::to_string(biggest) + " of " + std::to_string(full) + " rad");
}

// On Quake II's rules the window's status names Quake II's own weapon keys (key k + 1 its
// weapon k), so the weapon groups pick from its eleven: with the shotgun and shells, key 3's group
// (the super shotgun, then the shotgun) asks for the shotgun, key 2.
TEST(the_weapon_groups_pick_q2_s_weapons_by_its_own_keys) {
  cl::Listen l;
  if (!open_lift_hall(l, "300 300 24")) return;
  for (int i = 0; i < 30; i++) cl::listen_frame(l, {});
  namespace sv = shooters_edge::server;
  sv::Q2Player& p = sv::q2_of(*l.rules)->players[0];
  p.body.counts[sv::sq_item("weapon_shotgun")] = 1;
  p.body.ammo[int(sv::SqAmmo::shells)] = 10;
  const shooters_edge::hud::Status s = cl::status_of(l, 1);
  CHECK(s.keyed.set);
  CHECK(s.keyed.in_hand == 1);                            // the blaster
  CHECK(s.keyed.carried == ((1u << 1) | (1u << 2)));      // the blaster and the shotgun
  CHECK(s.keyed.ready == ((1u << 1) | (1u << 2)));        // both can fire
  CHECK(s.keyed.ammo[1] == -1 && s.keyed.ammo[2] == 10);  // the blaster spends none
  cl::WeaponGroups g;
  g.on = true;
  g.game = shooters_edge::profiles::find_game("q2")->weapon_groups;
  cl::Moves m;
  m.impulse = 3;
  cl::group_moves(g, s, 1, m);
  CHECK_MSG(m.impulse == 2, std::to_string(m.impulse));
}

// The window's own game on Quake II's rules throws Quake II's world as a joined client draws
// it: a teleporter pad's rising specks once a rules frame (ten a second, never once per window
// frame), and an item's glow once as it shows again.
TEST(q2_pads_rise_and_its_items_glow_back_in_the_windows_own_game) {
  br::BrushWorld world;
  world.models.resize(1);
  const float lo[3] = {-512, -512, -16}, hi[3] = {512, 512, 0};
  world.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  br::Entity start, pad, exit, item;
  start.pairs = {{"classname", "info_player_start"}, {"origin", "0 0 24"}};
  pad.pairs = {{"classname", "misc_teleporter"}, {"origin", "200 0 24"}, {"target", "t1"}};
  exit.pairs = {
      {"classname", "misc_teleporter_dest"}, {"origin", "-200 0 24"}, {"targetname", "t1"}};
  item.pairs = {{"classname", "item_health"}, {"origin", "0 200 24"}};
  world.entities = {start, pad, exit, item};
  std::vector<uint8_t> bsp;
  std::string error;
  CHECK_MSG(br::write_brush_world(world, &bsp, &error), error);
  cl::SearchPath files;
  CHECK(cl::add_archive(files, "pak0.pak", pack({{"maps/fixture.bsp", bsp}}), error));
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, "fixture", error);
  CHECK_MSG(map != nullptr, error);
  if (!map) return;
  cl::Listen l;
  CHECK_MSG(
      cl::listen_start(l, baseline(), std::move(map), shooters_edge::net::listen_options(), error),
      error);
  if (l.rules == nullptr) return;
  shooters_edge::server::Q2Server* sq = shooters_edge::server::q2_of(*l.rules);
  CHECK(sq != nullptr);
  if (sq == nullptr) return;
  int specks = 0, glows = 0;
  const auto count = [&] {
    for (const cl::Effect& e : l.effects) {
      if (e.name == "sq_fountain") specks++;
      if (e.name == "sq_item_back") glows++;
    }
    l.effects.clear();  // the window drains them each frame
  };
  const int64_t first = sq->frame_number;
  for (int i = 0; i < 77; i++) {  // a second of window frames
    cl::listen_frame(l, {});
    count();
  }
  const int64_t frames = sq->frame_number - first;
  CHECK_MSG(specks >= frames - 1 && specks <= frames + 1,
            std::to_string(specks) + " specks over " + std::to_string(frames) + " frames");
  CHECK(glows == 0);
  for (shooters_edge::server::Q2Entity& e : sq->entities)
    if (e.item >= 0) {
      e.hidden = true;  // as if taken, back in a moment
      e.respawn_at = sq->level_time + 0.3;
    }
  for (int i = 0; i < 77; i++) {
    cl::listen_frame(l, {});
    count();
  }
  CHECK_MSG(glows == 1, std::to_string(glows));
}

TEST(quake_twos_gun_sway_turns_and_moves_the_held_weapon_and_sets_ours_aside) {
  // Its degrees (pitch down, yaw left, roll right side down) as the hand's turn (yaw right, pitch
  // up, roll right side down); its place along the world's axes as right, up and forward of the
  // view; the hand's own lag set aside.
  shooters_edge::view::HandInput in;
  shooters_edge::client::q2_gun_sway({2, 4, 6}, {0, 40, 0}, {0, 90, 0}, in);
  constexpr double kRad = 0.017453292519943295;
  CHECK(in.game_sway);
  CHECK(std::fabs(in.place.game_turn[0] + 4 * kRad) < 1e-6);
  CHECK(std::fabs(in.place.game_turn[1] + 2 * kRad) < 1e-6);
  CHECK(std::fabs(in.place.game_turn[2] - 6 * kRad) < 1e-6);
  // Facing +y (yaw 90), 40 units along +y is a metre forward.
  CHECK_MSG(std::fabs(in.place.game_shift_m[2] - 1.0) < 1e-5 &&
                std::fabs(in.place.game_shift_m[0]) < 1e-5 &&
                std::fabs(in.place.game_shift_m[1]) < 1e-5,
            std::to_string(in.place.game_shift_m[2]));
}
