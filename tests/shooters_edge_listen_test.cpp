// Tests: the listen server the `shooters_edge` window runs, driven headless. The baseline mod
// compiled from the sources built into the game; the fixture map (plugins/brush) found in a pack
// through the search path (a later pack overrides an earlier one); the player spawned in its hall
// on a rocket launcher, who walks, jumps, and then ROCKET-JUMPS: looking straight down, he jumps
// and fires at the floor in the same frame and rises far higher than his jump alone takes him,
// for the rocket's health. The run is held to the bit by its golden rows. Also the keys and the
// mouse as commands, the HUD's status off the player, and the weapon in his hand: the launcher
// raised, one shot seen for one rocket and none for the spawn. The powers: the quad's and the
// shield's glow round their holder, the shield taking a blast, both running out.
//
// ge:summary Tests: the listen server headless: a fixture map from a pack, a rocket jump, golden
// ge:keywords test listen-server rocket-jump search-path pak baseline-mod controls hud-status
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

#include "brush/write.hpp"
#include "check.hpp"
#include "golden.hpp"
#include "progs/vm.hpp"
#include "save/file.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/held.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/settings.hpp"
#include "shooters_edge/client/view.hpp"
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

// The listen server on the halls, found in the second of two packs.
bool start(cl::Listen& l) {
  cl::SearchPath files;
  std::string error;
  CHECK(cl::add_archive(files, "pak0.pak", pack({{"maps/halls.bsp", {1, 2, 3}}}), error));
  CHECK(cl::add_archive(files, "pak1.pak", pack({{"maps/halls.bsp", halls()}}), error));
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, "halls", error);
  CHECK_MSG(map != nullptr, error);
  if (!map) return false;
  const bool ok = cl::listen_start(l, baseline(), std::move(map), {}, error);
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
  CHECK(cmd.forward == 400 && cmd.side == -350);
  CHECK(cmd.buttons == (sh::kButtonAttack | sh::kButtonJump));
  CHECK(cmd.impulse == 7);
  sh::Vec3f view{0, 10, 0};
  cl::turn_view(in, c, view);
  CHECK(std::fabs(view.y - (10 - 6.6f)) < 1e-3f);  // right is clockwise
  CHECK(std::fabs(view.x + 3.3f) < 1e-3f);         // up is a negative pitch
}

TEST(the_binds_buttons_make_the_command_walk_run_and_sidestep) {
  ge::console::Console con;
  cl::ControlSettings c;
  cl::add_cvars(con, c);
  cl::default_binds(con);
  CHECK(con.binds().get(ge::console::BindMode::game, "w") == "+forward");
  CHECK(con.binds().get(ge::console::BindMode::game, "mouse1") == "+attack");
  con.execute("bind w +back");  // the player's own bind is kept over the default
  cl::default_binds(con);
  CHECK(con.binds().get(ge::console::BindMode::game, "w") == "+back");
  con.key("w", true);
  con.key("mouse1", true);
  con.key("7", true);
  con.run();
  ge::input::InputState in;
  sh::MoveCmd cmd = cl::read_controls(cl::button_moves(c), in, c);
  CHECK(cmd.forward == -400);  // cl_run 1: running
  CHECK(cmd.buttons == sh::kButtonAttack);
  CHECK(cmd.impulse == 7);
  CHECK(cl::button_moves(c).impulse == 0);  // the impulse is sent once
  con.key("shift", true);                   // +speed walks
  con.run();
  CHECK(cl::read_controls(cl::button_moves(c), in, c).forward == -200);
  con.execute("cl_run 0");  // off, +speed runs
  CHECK(cl::read_controls(cl::button_moves(c), in, c).forward == -400);
  con.key("shift", false);
  con.run();
  CHECK(cl::read_controls(cl::button_moves(c), in, c).forward == -200);
  // lookstrafe: the mouse's sideways counts sidestep and leave the yaw alone.
  in.mouse.dx = 50;
  sh::Vec3f view{0, 90, 0};
  con.execute("lookstrafe 1");
  const cl::Moves m = cl::button_moves(c);
  cl::turn_view(in, m, c, 0.01, view);
  CHECK(view.y == 90);
  CHECK(cl::read_controls(m, in, c).side > 0);
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
    *ge::progs::entity_field(l.server->q.vm, entity, d->offset) = std::bit_cast<uint32_t>(value);
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
