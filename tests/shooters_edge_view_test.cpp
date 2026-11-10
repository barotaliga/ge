// Shooter's Edge's held weapons (games/shooters_edge/view): the set is the eight number keys in
// order, every model loads and mounts muzzle forward (a free one at its stated length, a built one
// round its grip hand), the hand puts a weapon away and raises the next on the game's change (and
// snaps it up on a shot), a shot plays the model's fire clip and comes back to rest (a nailgun
// held firing steps it a frame a shot, each shot from its frame's barrel; an axe of two swings
// plays one a shot, the one the game names or one at random), a shot lights
// the muzzle and fades, the player's cvars move the row, and every built weapon's arms stay inside
// the human ranges at rest and through every clip.
//
// ge:summary Tests for Shooter's Edge's held weapons: the set, the mounts, the switch, the flash
// ge:keywords test shooters-edge viewmodel weapons mount switch flash cvars hand fire-clip built
// ge:keywords arms elbow wrist range-of-motion
// ge:skill render testing
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "assets/gltf.hpp"
#include "assets/pose.hpp"
#include "check.hpp"
#include "console/cvar.hpp"
#include "shooters_edge/view/hand.hpp"
#include "shooters_edge/view/weapons.hpp"
#include "viewmodel/arm.hpp"

namespace {

namespace se = shooters_edge::view;
namespace vm = ge::viewmodel;

const std::vector<vm::HeldModel>& models() {
  static const std::vector<vm::HeldModel> m = [] {
    std::vector<vm::HeldModel> out;
    std::string error;
    se::load_weapon_models(std::string(GE_SOURCE_DIR) + "/games/shooters_edge/content/models/view",
                           out, &error);
    return out;
  }();
  return m;
}

ge::camera::Pose eye() {
  ge::camera::Pose v;
  v.eye = {0, 1.6, 0};
  v.fov_y = 1.2;
  return v;
}

}  // namespace

TEST(the_set_is_the_number_keys_in_order) {
  CHECK(se::weapon_looks().size() == static_cast<size_t>(se::kWeapons));
  for (int i = 0; i < se::kWeapons; ++i) {
    se::Weapon w;
    CHECK(se::weapon_of_impulse(i + 1, w) && static_cast<int>(w) == i);
    CHECK(se::weapon_look(w).weapon == w);
  }
  se::Weapon w;
  CHECK(!se::weapon_of_impulse(0, w) && !se::weapon_of_impulse(9, w));
  // Only the melee weapon throws no light.
  for (const se::WeaponLook& l : se::weapon_looks())
    CHECK((l.flash_s > 0) == (l.weapon != se::Weapon::melee));
}

TEST(every_weapon_loads_mounted_muzzle_forward) {
  const std::vector<vm::HeldModel>& m = models();
  CHECK(m.size() == static_cast<size_t>(se::kWeapons));
  const std::string dir = std::string(GE_SOURCE_DIR) + "/games/shooters_edge/content/models/view";
  for (size_t i = 0; i < m.size(); ++i) {
    const se::WeaponLook& look = se::weapon_looks()[i];
    vm::HeldModel held, bare;
    std::string error;
    CHECK(se::load_weapon_model(se::weapon_model_path(dir, look), look, held, &error, &bare));
    float lo[3], hi[3];
    vm::held_bounds(bare, lo, hi, 0);
    const bool built = bare.vertex_count() < held.vertex_count();  // it holds itself
    if (built) {
      // Built round its grip hand at kBuiltScale: the hold (the origin) inside the weapon's box,
      // the hands drawn with it but not in its box.
      CHECK(lo[0] < 0 && hi[0] > 0 && lo[1] < 0 && hi[1] > 0 && lo[2] < 0 && hi[2] > 0);
    } else {
      const float along = look.upright ? hi[1] - lo[1] : hi[2] - lo[2];
      CHECK(std::fabs(along - look.length_m) < 1e-3);
    }
    if (look.upright) continue;
    // The grip at the origin: the muzzle ahead of it, the stock behind.
    float tip[3];
    vm::muzzle_at(m[i], 0, 0, 0, tip);
    CHECK(tip[2] > 0.5f * hi[2] && lo[2] < 0);
  }
}

TEST(a_shot_plays_the_fire_clip_and_comes_back_to_rest) {
  const std::vector<vm::HeldModel>& m = models();
  for (const se::WeaponLook& look : se::weapon_looks()) {
    const vm::HeldModel& model = m[static_cast<size_t>(look.weapon)];
    const int clip = vm::find_held_clip(model, "fire");
    if (clip < 0) continue;
    se::Hand hand;
    se::HandInput in;
    in.weapon = look.weapon;
    const se::ViewSettings s;
    const double dt = 1.0 / 60;
    for (int i = 0; i < 20; ++i) se::step_hand(hand, in, s, m, eye(), dt);
    in.fired = true;
    se::HandFrame f = se::step_hand(hand, in, s, m, eye(), dt);
    in.fired = false;
    CHECK(f.frame_a == model.clips[static_cast<size_t>(clip)].first);
    bool moved = false;
    for (int i = 0; i < 120; ++i) {
      f = se::step_hand(hand, in, s, m, eye(), dt);
      moved = moved || f.frame_a > model.clips[static_cast<size_t>(clip)].first;
    }
    CHECK(moved);
    CHECK(f.frame_a == 0 && f.frame_b == 0);  // played through, back to the rest frame
  }
}

TEST(every_built_weapons_arms_stay_inside_their_ranges) {
  // At rest and every 1/60 s of every clip, each arm's shoulder, elbow, wrist and forearm inside
  // the human ranges (viewmodel/arm.hpp): no elbow pointing up or in, no wrist wrung.
  const std::string dir = std::string(GE_SOURCE_DIR) + "/games/shooters_edge/content/models/view";
  const double right[3] = {-1, 0, 0}, up[3] = {0, 1, 0}, forward[3] = {0, 0, -1};
  int built = 0;
  for (const se::WeaponLook& look : se::weapon_looks()) {
    const std::string path = se::weapon_model_path(dir, look);
    if (path.find(std::string("/") + std::string(look.name) + "/") == std::string::npos) continue;
    ge::assets::LoadResult r = ge::assets::load_gltf_file(path);
    CHECK(r.ok);
    if (!r.ok) continue;
    const ge::assets::Model& m = r.model;
    std::vector<ge::assets::Trs> local(m.joints.size());
    std::vector<ge::assets::Affine> global(m.joints.size());
    auto check_pose = [&](const std::string& when) {
      ge::assets::pose_globals(m, local, global);
      vm::ArmPose p[2];
      const int n = vm::arm_poses(m, global, right, up, forward, p);
      CHECK(n > 0);
      for (int a = 0; a < n; ++a) {
        const std::vector<std::string> bad = vm::arm_faults(vm::arm_angles(p[a]));
        if (!bad.empty())
          std::fprintf(stderr, "%s, %s, the %s arm: %s\n", std::string(look.name).c_str(),
                       when.c_str(), p[a].side > 0 ? "right" : "left", bad[0].c_str());
        CHECK(bad.empty());
      }
    };
    ge::assets::rest_pose(m, local);
    check_pose("at rest");
    for (const ge::assets::Clip& clip : m.clips) {
      for (uint32_t f = 0; f < clip.frames; ++f) {
        ge::assets::sample_clip(m, clip, f / 60.0, false, local);
        check_pose(clip.name + " at frame " + std::to_string(f));
      }
    }
    ++built;
  }
  CHECK(built > 0);
}

TEST(a_change_puts_the_weapon_away_then_raises_the_next) {
  se::Hand hand;
  se::HandInput in;
  in.weapon = se::Weapon::shotgun;
  const se::ViewSettings s;
  const double dt = 1.0 / 60;
  se::HandFrame f = se::step_hand(hand, in, s, models(), eye(), dt);
  CHECK(f.weapon == se::Weapon::shotgun && !f.switching && f.drawn);
  in.weapon = se::Weapon::rocket_launcher;
  bool saw_down = false, saw_old_lowering = false;
  int up_at = -1;
  for (int i = 1; i <= 60 && up_at < 0; ++i) {
    f = se::step_hand(hand, in, s, models(), eye(), dt);
    if (f.weapon == se::Weapon::shotgun && f.switching) saw_old_lowering = true;
    if (!f.drawn) saw_down = true;
    if (f.weapon == se::Weapon::rocket_launcher && !f.switching) up_at = i;
  }
  CHECK(saw_old_lowering && saw_down);
  CHECK(up_at > 0 && up_at * dt <= s.switch_s + 2 * dt);
}

TEST(a_shot_during_a_switch_rushes_the_new_weapon_up) {
  // The old weapon leaves the picture and the new one comes up from low in the picture firing,
  // cl_gunswitch_fire times faster than an unhurried raise.
  const se::ViewSettings s;
  const double dt = 1.0 / 120;
  const auto raise_s = [&](bool fire) {
    se::Hand hand;
    se::HandInput in;
    in.weapon = se::Weapon::nailgun;
    se::step_hand(hand, in, s, models(), eye(), dt);
    in.weapon = se::Weapon::beam_gun;
    se::HandFrame f;
    for (int i = 0; i < 6; ++i) f = se::step_hand(hand, in, s, models(), eye(), dt);
    const double was = f.lowered;
    CHECK(f.weapon == se::Weapon::nailgun && was > 0.1 && was < 1);
    in.fired = fire;
    f = se::step_hand(hand, in, s, models(), eye(), dt);
    if (fire) {
      CHECK(f.weapon == se::Weapon::beam_gun && f.drawn && f.flashing && f.rushed);
      CHECK(f.lowered > 0 &&
            f.lowered < se::kRushFrom);  // on its way up from kRushFrom, already in view
    }
    in.fired = false;
    int frames = 1;
    while ((f.switching || f.weapon != se::Weapon::beam_gun) && frames < 200) {
      f = se::step_hand(hand, in, s, models(), eye(), dt);
      ++frames;
    }
    CHECK(!f.rushed && f.lowered == 0);
    return frames * dt;
  };
  const double calm = raise_s(false), rushed = raise_s(true);
  CHECK(rushed < 0.5 * calm);
  CHECK(rushed <= s.switch_s * (1 - se::kSwitchDownShare) / s.switch_fire + 3 * dt);
}

TEST(cl_gunswitch_0_changes_the_weapon_at_once) {
  se::ViewSettings s;
  s.switch_s = 0;
  se::Hand hand;
  se::HandInput in;
  in.weapon = se::Weapon::shotgun;
  se::step_hand(hand, in, s, models(), eye(), 1.0 / 60);
  in.weapon = se::Weapon::rocket_launcher;
  const se::HandFrame f = se::step_hand(hand, in, s, models(), eye(), 1.0 / 60);
  CHECK(f.weapon == se::Weapon::rocket_launcher && f.drawn && !f.switching && f.lowered == 0);
}

TEST(a_shot_lights_the_muzzle_and_fades) {
  se::Hand hand;
  se::HandInput in;
  in.weapon = se::Weapon::shotgun;
  const se::ViewSettings s;
  const double dt = 1.0 / 60;
  se::step_hand(hand, in, s, models(), eye(), dt);
  in.fired = true;
  se::HandFrame f = se::step_hand(hand, in, s, models(), eye(), dt);
  const se::WeaponLook& look = se::weapon_look(se::Weapon::shotgun);
  CHECK(f.flashing && f.flash.intensity > 0.9f * look.flash_intensity);
  // In front of the eye and in the middle, where the centred barrel is.
  CHECK(f.flash.position[2] > 0.3 && std::fabs(f.flash.position[0]) < 0.05);
  in.fired = false;
  float last = f.flash.intensity;
  int frames = 1;
  while (f.flashing && frames < 60) {
    f = se::step_hand(hand, in, s, models(), eye(), dt);
    if (f.flashing) CHECK(f.flash.intensity < last);
    last = f.flash.intensity;
    ++frames;
  }
  CHECK(!f.flashing && frames * dt <= look.flash_s + 2 * dt);
}

TEST(the_cvars_move_the_row) {
  ge::console::Cvars cvars;
  se::ViewSettings s;
  se::add_view_cvars(cvars, s);
  const se::WeaponLook& look = se::weapon_look(se::Weapon::shotgun);
  const vm::PlaceRow game = se::held_row(look, s);
  // Centred by default: under the crosshair, nearer and a little higher than the place to the
  // right.
  CHECK(s.centre && s.hands);
  CHECK(game.offset_m[0] == 0 && game.fov_y == 0);
  CHECK(std::fabs(game.offset_m[1] - (look.offset_m[1] + se::kCentreRiseM)) < 1e-12);
  CHECK(std::fabs(game.offset_m[2] - (look.offset_m[2] - se::kCentreNearM)) < 1e-12);
  CHECK(cvars.set("cl_gunx", "-0.1", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  CHECK(cvars.set("r_viewmodel_fov", "60", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  CHECK(cvars.set("cl_gunsway", "0", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  CHECK(cvars.set("r_drawviewmodel", "0", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  const vm::PlaceRow moved = se::held_row(look, s);
  CHECK(std::fabs(moved.offset_m[0] - (-0.1)) < 1e-12);
  CHECK(std::fabs(moved.fov_y - 60 * 3.14159265358979 / 180) < 1e-9);
  CHECK(moved.sway_max == 0 && moved.bob_side == 0 && moved.breath_m == 0 &&
        moved.breath_wander_m == 0);
  CHECK(!s.draw);
  // The breath: cl_gunidle scales its depth, cl_gunidle_rate is breaths a minute at rest and
  // cl_gunidle_exertion scales a run's mark on it.
  CHECK(cvars.set("cl_gunsway", "1", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  CHECK(cvars.set("cl_gunidle", "2", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  CHECK(cvars.set("cl_gunidle_rate", "30", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  CHECK(cvars.set("cl_gunidle_exertion", "0", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  const vm::PlaceRow breathing = se::held_row(look, s);
  CHECK(std::fabs(breathing.breath_m - 2 * game.breath_m) < 1e-12 && game.breath_m > 0);
  CHECK(std::fabs(breathing.breath_hz - 0.5) < 1e-12);
  CHECK(breathing.exert_rate == 0 && breathing.exert_depth == 0);
  // The change: cl_gunswitch its time, cl_gunswitch_fire a shot's speed-up.
  CHECK(s.switch_s == 0.2 && s.switch_fire == 4);
  CHECK(cvars.set("cl_gunswitch", "0.1", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  CHECK(cvars.set("cl_gunswitch_fire", "2", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  CHECK(s.switch_s == 0.1 && s.switch_fire == 2);
  // Not drawn: still switching and flashing, unseen.
  se::Hand hand;
  se::HandInput in;
  in.weapon = se::Weapon::shotgun;
  in.fired = true;
  const se::HandFrame f = se::step_hand(hand, in, s, models(), eye(), 1.0 / 60);
  CHECK(!f.drawn && f.flashing);
}

TEST(centre_and_hands_cvars) {
  ge::console::Cvars cvars;
  se::ViewSettings s;
  se::add_view_cvars(cvars, s);
  const se::WeaponLook& look = se::weapon_look(se::Weapon::rocket_launcher);
  CHECK(cvars.set("r_viewmodel_center", "0", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  CHECK(cvars.set("r_viewmodel_hands", "0", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  CHECK(!s.centre && !s.hands);
  const vm::PlaceRow right = se::held_row(look, s);
  CHECK(right.offset_m[0] == look.offset_m[0] && right.offset_m[1] == look.offset_m[1]);
  se::Hand hand;
  se::HandInput in;
  in.weapon = se::Weapon::rocket_launcher;
  const se::HandFrame f = se::step_hand(hand, in, s, models(), eye(), 1.0 / 60);
  CHECK(!f.hands && f.drawn);
}

TEST(built_weapons_mark_their_hands_and_hide_them) {
  for (const se::WeaponLook& look : se::weapon_looks()) {
    const vm::HeldModel& m = models()[static_cast<size_t>(look.weapon)];
    CHECK(m.hand.size() == m.indices.size() / 3);
    size_t hands = 0;
    for (uint8_t h : m.hand) hands += h;
    CHECK(hands > 0 && hands < m.hand.size());
    std::vector<uint8_t> hidden;
    vm::hidden_triangles(m, true, false, hidden);
    CHECK(hidden.empty());  // no flame of its own, the hands kept: nothing hidden
    vm::hidden_triangles(m, true, true, hidden);
    size_t off = 0;
    for (uint8_t h : hidden) off += h;
    CHECK(off == hands);
  }
}

TEST(the_muzzle_rides_the_gun_into_the_world) {
  // The eye looks down +z: the muzzle is ahead of it, under it, pointing ahead, centred by default.
  se::Hand hand;
  se::HandInput in;
  in.weapon = se::Weapon::shotgun;
  const se::ViewSettings s;
  se::HandFrame f;
  for (int i = 0; i < 30; ++i) f = se::step_hand(hand, in, s, models(), eye(), 1.0 / 60);
  const ge::camera::Pose v = eye();
  CHECK(f.muzzle_at[2] > v.eye.z + 0.1);
  CHECK(f.muzzle_at[1] < v.eye.y);
  CHECK(std::fabs(f.muzzle_at[0] - v.eye.x) < 0.05);
  CHECK(f.muzzle_dir[2] > 0.9);
  in.fired = true;
  f = se::step_hand(hand, in, s, models(), eye(), 1.0 / 60);
  CHECK(f.fired);
}

TEST(a_classic_games_own_weapon_is_held_at_the_eye) {
  se::ViewSettings s;
  s.gun_m[1] = -0.02;
  s.idle = 0;  // no breath: the place alone
  const vm::PlaceRow eye_row = se::eye_row(s);
  CHECK(eye_row.offset_m[0] == 0 && eye_row.offset_m[1] == -0.02 && eye_row.offset_m[2] == 0);
  // The same weapon at rest: at the eye it sits where its file put it, on a look it is moved out.
  const std::vector<vm::HeldModel>& m = models();
  se::Hand at_eye, on_look;
  at_eye.at_eye = true;
  se::HandInput in;
  const double dt = 1.0 / 60;
  se::HandFrame a, b;
  for (int i = 0; i < 30; ++i) {
    a = se::step_hand(at_eye, in, s, m, eye(), dt);
    b = se::step_hand(on_look, in, s, m, eye(), dt);
  }
  float o[3] = {0, 0, 0}, pa[3], pb[3];
  vm::place_point(a.placement, o, pa);
  vm::place_point(b.placement, o, pb);
  CHECK(std::fabs(pa[0]) < 1e-4f && std::fabs(pa[1] + 0.02f) < 1e-4f && std::fabs(pa[2]) < 1e-4f);
  CHECK(pb[1] < -0.1f);  // the look's grip, well below the eye
}

TEST(a_nailgun_held_firing_steps_a_frame_a_shot_from_each_barrel_in_turn) {
  // A model of a rest frame and four firing frames, two barrels (vertex 0 on the left, 1 on the
  // right), each firing frame flashing one of them in turn, its clip stepped a frame a shot.
  vm::HeldModel gun;
  gun.frames.resize(5);
  for (vm::HeldFrame& fr : gun.frames) fr.positions = {-0.05f, 0, 0.5f, 0.05f, 0, 0.5f, 0, 0.1f, 0};
  gun.indices = {0, 1, 2};
  gun.muzzle_vertices = {0, 1};
  gun.barrels = {{0}, {1}};
  gun.frame_barrel = {-1, 0, 1, 0, 1};
  gun.clips.push_back({.name = "fire", .first = 1, .count = 4, .fps = 10, .frame_a_shot = true});
  std::vector<vm::HeldModel> set(se::kWeapons, gun);
  se::Hand hand;
  hand.at_eye = true;
  se::HandInput in;
  in.weapon = se::Weapon::nailgun;
  const se::ViewSettings s;
  const double dt = 0.05;  // two display frames a shot
  for (int i = 0; i < 4; ++i) se::step_hand(hand, in, s, set, eye(), dt);
  std::vector<int> frames;
  std::vector<double> xs;
  for (int shot = 0; shot < 6; ++shot) {
    in.fired = true;
    const se::HandFrame f = se::step_hand(hand, in, s, set, eye(), dt);
    in.fired = false;
    const se::HandFrame held = se::step_hand(hand, in, s, set, eye(), dt);
    CHECK(f.frame_a == f.frame_b && held.frame_a == f.frame_a);  // held, not blended on
    frames.push_back(f.frame_a);
    xs.push_back(f.muzzle_at[0]);
  }
  CHECK((frames == std::vector<int>{1, 2, 3, 4, 1, 2}));  // a frame a shot, round the clip
  for (size_t i = 1; i < xs.size(); ++i) CHECK((xs[i] > xs[i - 1]) == (i % 2 == 1));  // L, R, L
  // A frame's time and a half after the last shot it flows back to rest over a frame's time (held
  // there with r_lerpmodels 0); the next burst starts over.
  se::HandFrame f;
  bool flowed = false;
  for (int i = 0; i < 4; ++i) {
    f = se::step_hand(hand, in, s, set, eye(), dt);
    flowed = flowed || (f.frame_a == 2 && f.frame_b == 0 && f.frame_t > 0 && f.frame_t < 1);
  }
  CHECK(flowed);
  for (int i = 0; i < 2; ++i) f = se::step_hand(hand, in, s, set, eye(), dt);
  CHECK(f.frame_a == 0 && f.frame_b == 0);
  in.fired = true;
  CHECK(se::step_hand(hand, in, s, set, eye(), dt).frame_a == 1);
}

TEST(an_axe_of_two_swings_plays_one_swing_a_shot) {
  // A rest frame and two swings of four frames each (1-4 and 5-8), two `fire` takes.
  vm::HeldModel axe;
  axe.frames.resize(9);
  for (vm::HeldFrame& fr : axe.frames) fr.positions = {-0.05f, 0, 0.5f, 0.05f, 0, 0.5f, 0, 0.1f, 0};
  axe.indices = {0, 1, 2};
  axe.clips.push_back({.name = "fire", .first = 1, .count = 4, .fps = 10});
  axe.clips.push_back({.name = "fire", .first = 5, .count = 4, .fps = 10});
  CHECK(vm::count_held_clips(axe, "fire") == 2);
  CHECK(vm::find_held_clip(axe, "fire", 1) == 1 && vm::find_held_clip(axe, "fire", 2) == -1);
  std::vector<vm::HeldModel> set(se::kWeapons, axe);
  const se::ViewSettings s;
  const double dt = 0.025;
  // One swing: the frames it shows from the shot to rest stay inside one take.
  auto swing = [&](se::Hand& hand, int fire_frame) {
    se::HandInput in;
    in.weapon = se::Weapon::melee;
    in.fired = true;
    in.fire_frame = fire_frame;
    std::vector<int> seen;
    for (int i = 0; i < 24; ++i) {  // 0.6 s: longer than a swing
      const se::HandFrame f = se::step_hand(hand, in, s, set, eye(), dt);
      in.fired = false;
      seen.push_back(f.frame_a);
      if (f.frame_t > 0) seen.push_back(f.frame_b);
    }
    int lo = 9, hi = 0;
    for (int fr : seen)
      if (fr != 0) lo = fr < lo ? fr : lo, hi = fr > hi ? fr : hi;
    CHECK(seen.back() == 0);  // back at rest
    CHECK((lo == 1 && hi == 4) || (lo == 5 && hi == 8));
    return lo == 1 ? 0 : 1;
  };
  se::Hand hand;
  hand.at_eye = true;
  {
    se::HandInput in;
    in.weapon = se::Weapon::melee;
    se::step_hand(hand, in, s, set, eye(), dt);
  }
  // The game's word wins: its weapon frame names the swing.
  CHECK(swing(hand, 5) == 1);
  CHECK(swing(hand, 1) == 0);
  CHECK(swing(hand, 7) == 1);
  // Unsaid, the hand picks: both swings come up, neither always.
  int second = 0;
  for (int i = 0; i < 40; ++i) second += swing(hand, 0);
  CHECK(second > 8 && second < 32);
}
