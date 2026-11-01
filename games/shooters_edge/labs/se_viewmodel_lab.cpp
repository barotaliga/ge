// LAB `se-viewmodel`: Shooter's Edge's own held weapons (games/shooters_edge/view), the built
// models with no classic pack, drawn in first person on the CPU in the engine's stone corridor
// (viewmodel/stage.hpp), every frame stepped through the game's hand (view/hand.hpp):
//
//   - HELD: all eight at rest by day, each on its own mount and offset, centred (the default) and
//     to the right (HELD-RIGHT); per weapon, the share of the picture its box covers and where its
//     muzzle lands on the screen.
//   - FIRED: each in the dark on the frame of a shot, lit by its own muzzle flash (the hand's
//     light, also put in the room) and showing the stage's flame at the muzzle; per weapon, how
//     much the shot lifts the picture.
//   - SWITCH: the shotgun put away and the rocket launcher raised on the game's weapon change, a
//     frame each 1/30 s, and the time from the change to the new weapon up.
//   - CVARS: the shotgun as the player's cvars move it, set through a real cvar registry: the
//     game's place (centred), r_viewmodel_center 0 (to the right), r_viewmodel_hands 0 (the
//     weapon alone), r_viewmodel_fov 50 (a narrower lens: a bigger gun), cl_gunsway 0 on a turn,
//     cl_guny -0.03, r_drawviewmodel 0.
//
// CPU pictures from the engine's own arithmetic, so in the digest.
//
// ge:summary Lab: Shooter's Edge's held weapons: every one held, fired with its light, switched
// ge:keywords lab se-viewmodel shooters-edge viewmodel held-weapon first-person mount switch
// ge:keywords muzzle-flash flash-light cvars r_drawviewmodel r_viewmodel_fov cl_gunx cl_gunsway
// ge:keywords r_viewmodel_center r_viewmodel_hands centred hide-hands
// ge:skill render labs
// ge:test tests/shooters_edge_view_test.cpp
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "camera/pose.hpp"
#include "console/cvar.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooters_edge/view/hand.hpp"
#include "shooters_edge/view/weapons.hpp"
#include "viewmodel/raster.hpp"
#include "viewmodel/stage.hpp"

namespace {

namespace se = shooters_edge::view;
namespace vm = ge::viewmodel;
using ge::lab::Picture;
using Inputs = std::vector<std::pair<std::string, std::string>>;

constexpr double kDt = 1.0 / 60;

ge::camera::Pose stage_view() {
  ge::camera::Pose v;
  v.eye = {0, 1.6, 0};
  v.pitch = -0.04;
  v.fov_y = ge::camera::fov_y_of(90.0 * ge::core::kPi / 180.0, 16.0 / 9.0);
  return v;
}

Picture to_picture(std::vector<uint8_t> rgba, int w, int h) {
  Picture p;
  p.width = w;
  p.height = h;
  p.rgba = std::move(rgba);
  return p;
}

// Panels laid `cols` across with a label each.
Picture sheet(const std::vector<Picture>& panels, const std::vector<std::string>& labels,
              int cols) {
  const int w = panels[0].width, h = panels[0].height, gap = 4;
  const int rows = (static_cast<int>(panels.size()) + cols - 1) / cols;
  Picture out =
      ge::lab::create_picture(cols * (w + gap) + gap, rows * (h + gap) + gap, {16, 17, 20});
  for (size_t i = 0; i < panels.size(); ++i) {
    const int x = gap + static_cast<int>(i % cols) * (w + gap);
    const int y = gap + static_cast<int>(i / cols) * (h + gap);
    ge::lab::blit(out, panels[i], x, y);
    ge::lab::fill_rect(out, x, y, ge::lab::text_width(labels[i], 2) + 8, 22, {0, 0, 0, 150});
    ge::lab::draw_text(out, x + 4, y + 4, labels[i], {240, 240, 240}, 2);
  }
  return out;
}

// The mean of a picture's channels, 0..255.
double mean_level(const Picture& p) {
  double sum = 0;
  for (size_t i = 0; i < p.rgba.size(); i += 4) sum += p.rgba[i] + p.rgba[i + 1] + p.rgba[i + 2];
  return sum / (0.75 * static_cast<double>(p.rgba.size()));
}

// A view point on the screen (0..1 across, 0..1 down) through the lens `fov_y` (0: the view's).
void to_screen(const float v[3], double fov_y, double aspect, double out[2]) {
  const double ty = ge::core::libm::tan(0.5 * fov_y);
  const double z = std::max(1e-4, static_cast<double>(v[2]));
  out[0] = 0.5 + 0.5 * v[0] / (z * ty * aspect);
  out[1] = 0.5 - 0.5 * v[1] / (z * ty);
}

void muzzle_view(const vm::HeldModel& m, const vm::Placement& p, float out[3]) {
  float tip[3];
  vm::muzzle_at(m, 0, 0, 0, tip);
  vm::place_point(p, tip, out);
}

// The hand settled at rest on `w` for half a second, then one more frame (fired or not).
se::HandFrame settle(se::Hand& hand, se::Weapon w, const se::ViewSettings& s,
                     const std::vector<vm::HeldModel>& models, const ge::camera::Pose& view,
                     bool fire_last, double turn = 0) {
  se::HandInput in;
  in.weapon = w;
  se::HandFrame f;
  for (int i = 0; i < 30; ++i) {
    in.place.look_yaw = turn * i * kDt;
    in.fired = fire_last && i == 29;
    f = se::step_hand(hand, in, s, models, view, kDt);
  }
  return f;
}

void run(ge::lab::Lab& lab) {
  const int width = static_cast<int>(lab.arg_num("width"));
  if (width < 96 || width > 1920) {
    lab.fail("--width must be 96..1920", "next: rerun with --width=480.");
    return;
  }
  const int height = width * 9 / 16;
  const double aspect = static_cast<double>(width) / height;
  const std::string dir = lab.arg("models");
  std::vector<vm::HeldModel> models;
  std::string error;
  if (!se::load_weapon_models(dir, models, &error)) {
    lab.fail(error, "next: rerun from the repository's root, or pass --models=<folder>.");
    return;
  }
  // Each weapon without the hands holding it, mounted the same: what its size is measured on.
  std::vector<vm::HeldModel> bare(models.size());
  for (const se::WeaponLook& look : se::weapon_looks()) {
    vm::HeldModel held;
    se::load_weapon_model(se::weapon_model_path(dir, look), look, held, &error,
                          &bare[static_cast<size_t>(look.weapon)]);
  }
  const Inputs inputs = {{"models", dir}, {"width", std::to_string(width)}};
  const vm::Stage stage = vm::corridor_stage();
  const ge::camera::Pose view = stage_view();
  vm::StageShot shot;
  shot.width = width;
  shot.height = height;
  const se::ViewSettings game_default;
  const se::ViewSettings& game = game_default;
  int problems = 0;

  std::vector<uint8_t> hidden;
  auto draw = [&](const se::HandFrame& f, vm::StageLights lights, const ge::camera::Pose& v) {
    const vm::HeldModel& m = models[static_cast<size_t>(f.weapon)];
    const se::WeaponLook& look = se::weapon_look(f.weapon);
    if (f.flashing) {
      lights.lights.push_back(f.flash);
      lights.flare = !look.muzzle.empty();
      muzzle_view(m, f.placement, lights.flare_at);
      vm::place_vector(f.placement, m.muzzle_dir, lights.flare_dir);
      // The flame at the gun's drawn scale: a shotgun's short star, a launcher's longer jet.
      lights.flare_length_m = static_cast<float>(se::kBuiltScale * 0.1 * (0.6 + look.kick));
      lights.flare_width_m = static_cast<float>(se::kBuiltScale * 0.09);
      lights.flare_heat = static_cast<float>(f.flash.intensity / look.flash_intensity);
      lights.flare_prongs = look.weapon == se::Weapon::rocket_launcher ? 0 : 5;
    }
    vm::hidden_triangles(m, true, !f.hands, hidden);
    shot.hidden = hidden;
    if (!f.drawn) {
      // A weapon not drawn: the room alone (a model of one frame and no triangles).
      vm::HeldModel none;
      none.frames.emplace_back();
      return to_picture(vm::draw_stage(stage, lights, v, none, f.placement, shot), width, height);
    }
    return to_picture(vm::draw_stage(stage, lights, v, m, f.placement, shot), width, height);
  };

  // Every weapon held at rest, by day: centred (the default) and to the right.
  for (const bool centre : {true, false}) {
    se::ViewSettings held_as = game_default;
    held_as.centre = centre;
    std::vector<Picture> panels;
    std::vector<std::string> labels;
    ge::lab::LabTable table{
        centre ? "every weapon held, centred" : "every weapon held, to the right",
        {"weapon", "length m", "screen share", "muzzle x", "muzzle y"},
        {}};
    for (const se::WeaponLook& look : se::weapon_looks()) {
      se::Hand hand;
      const se::HandFrame f = settle(hand, look.weapon, held_as, models, view, false);
      const vm::HeldModel& m = models[static_cast<size_t>(look.weapon)];
      float mv[3];
      muzzle_view(m, f.placement, mv);
      const vm::StageLights l = vm::stage_lights(vm::StageLight::day, view, mv);
      panels.push_back(draw(f, l, view));
      labels.emplace_back(look.name);
      const vm::HeldModel& b = bare[static_cast<size_t>(look.weapon)];
      const double share = vm::held_coverage(b, f.placement, view.fov_y, width, height);
      double at[2];
      to_screen(mv, view.fov_y, aspect, at);
      float lo[3], hi[3];
      vm::held_bounds(b, lo, hi, 0);
      const std::string key(look.name);
      std::string tail = centre ? "" : "right-";
      tail += key;
      lab.metric(std::string("share-").append(tail), share, "share");
      lab.metric(std::string("muzzle-x-").append(tail), at[0], "share");
      lab.metric(std::string("muzzle-y-").append(tail), at[1], "share");
      char c[4][16];
      std::snprintf(c[0], 16, "%.3f", look.upright ? hi[1] - lo[1] : hi[2] - lo[2]);
      std::snprintf(c[1], 16, "%.3f", share);
      std::snprintf(c[2], 16, "%.3f", at[0]);
      std::snprintf(c[3], 16, "%.3f", at[1]);
      table.rows.push_back({key, c[0], c[1], c[2], c[3]});
      // Held, not a toy and not the whole picture (a built weapon at its true size in hands covers
      // more than a toy-scale one did); the muzzle (the tip) on the screen.
      // Centred, a gun is seen down its length and covers less.
      const double least = centre ? 0.01 : 0.03;
      if (share < least || share > 0.4) {
        ++problems;
        lab.note(key + ": it covers " + c[1] + " of the picture (stated " +
                 (centre ? "0.01" : "0.03") + "..0.4)");
      }
      if (!look.upright && (at[0] < 0.02 || at[0] > 0.98 || at[1] < 0.02 || at[1] > 0.98)) {
        ++problems;
        lab.note(key + ": its muzzle is off the picture");
      }
    }
    lab.table(table);
    lab.shot({centre ? "held" : "held-right",
              centre ? "EVERY WEAPON HELD, CENTRED" : "EVERY WEAPON HELD TO THE RIGHT",
              std::string("Shooter's Edge's eight built weapons, held in the hands, at rest in "
                          "first person by day in the stone corridor, each on its own mount and "
                          "offset from the eye, through the hand: ") +
                  (centre ? "centred under the crosshair, the default (r_viewmodel_center 1)."
                          : "to the right (r_viewmodel_center 0)."),
              centre ? "every muzzle points down the corridor from the middle of the picture's "
                       "lower half, below the crosshair; the weapon's top stays under the middle "
                       "line, its back leaves the bottom edge; the melee weapon stands upright"
                     : "every muzzle points down the corridor; each weapon sits low right with its "
                       "top well under the middle line and its back leaving the bottom right "
                       "corner, at about the same size; the melee weapon stands upright",
              inputs},
             sheet(panels, labels, 4));
  }

  // Each weapon fired in the dark: its own flash lights it and the room.
  {
    std::vector<Picture> panels;
    std::vector<std::string> labels;
    for (const se::WeaponLook& look : se::weapon_looks()) {
      se::Hand calm, shot_hand;
      const se::HandFrame still = settle(calm, look.weapon, game, models, view, false);
      const se::HandFrame fired = settle(shot_hand, look.weapon, game, models, view, true);
      float mv[3];
      muzzle_view(models[static_cast<size_t>(look.weapon)], still.placement, mv);
      const vm::StageLights dark = vm::stage_lights(vm::StageLight::dark, view, mv);
      const Picture a = draw(still, dark, view);
      const Picture b = draw(fired, dark, view);
      const double lift = mean_level(b) - mean_level(a);
      const std::string key(look.name);
      lab.metric("flash-lift-" + key, lift, "level");
      if ((look.flash_s > 0) != (lift > 0.5)) {
        ++problems;
        lab.note(key + ": the shot lifted the picture by " + std::to_string(lift) +
                 (look.flash_s > 0 ? " (it has a flash)" : " (it has none)"));
      }
      panels.push_back(b);
      labels.push_back(key + " FIRED");
    }
    lab.shot({"fired", "EVERY WEAPON FIRED IN THE DARK",
              "Each weapon on the frame of a shot in the dark corridor: the hand's muzzle light "
              "at the barrel's tip lights the gun and the room round it, and a flare marks the "
              "muzzle. The melee weapon has no flash.",
              "the light comes from the muzzle: the barrel's top and the near walls lit warm (the "
              "lightning gun's cold), the gun's back darker; the kick has pushed it back and up",
              inputs},
             sheet(panels, labels, 4));
  }

  // The switch: the shotgun down, the rocket launcher up.
  {
    se::Hand hand;
    settle(hand, se::Weapon::shotgun, game, models, view, false);
    se::HandInput in;
    in.weapon = se::Weapon::rocket_launcher;
    std::vector<Picture> panels;
    std::vector<std::string> labels;
    double up_at = -1;
    for (int i = 1; i <= 36; ++i) {
      const se::HandFrame f = se::step_hand(hand, in, game, models, view, kDt);
      if (up_at < 0 && !f.switching) up_at = i * kDt;
      if (i % 3 == 0 && panels.size() < 12) {
        float mv[3];
        muzzle_view(models[static_cast<size_t>(f.weapon)], f.placement, mv);
        panels.push_back(draw(f, vm::stage_lights(vm::StageLight::torch, view, mv), view));
        char b[40];
        std::snprintf(b, sizeof b, "%.3f S %s", i * kDt, f.drawn ? "" : "(DOWN)");
        labels.emplace_back(b);
      }
    }
    lab.metric("switch-s", up_at, "s");
    const double most = 2 * se::weapon_row().switch_s + 0.1;
    if (up_at < 0 || up_at > most) {
      ++problems;
      lab.note("the switch took " + std::to_string(up_at) + " s (stated at most " +
               std::to_string(most) + ")");
    }
    lab.shot({"switch", "SHOTGUN TO ROCKET LAUNCHER",
              "The game changes the weapon at 0 s: the shotgun goes down and out of the picture, "
              "the rocket launcher comes up in its place; a frame each 1/20 s by the torch.",
              "the old weapon leaves downward and pitched down, the new one rises the same way; "
              "no frame shows both or a weapon popping in at rest",
              inputs},
             sheet(panels, labels, 4));
  }

  // The player's cvars.
  {
    ge::console::Cvars cvars;
    se::ViewSettings s;
    se::add_view_cvars(cvars, s);
    struct Case {
      const char* label;
      const char* name;
      const char* value;
      double turn;
    };
    const Case cases[] = {{"GAME'S PLACE (CENTRED)", nullptr, nullptr, 0},
                          {"R_VIEWMODEL_CENTER 0", "r_viewmodel_center", "0", 0},
                          {"R_VIEWMODEL_HANDS 0", "r_viewmodel_hands", "0", 0},
                          {"R_VIEWMODEL_FOV 50", "r_viewmodel_fov", "50", 0},
                          {"TURNING, SWAY 1", nullptr, nullptr, 3.0},
                          {"TURNING, CL_GUNSWAY 0", "cl_gunsway", "0", 3.0},
                          {"CL_GUNY -0.03", "cl_guny", "-0.03", 0},
                          {"R_DRAWVIEWMODEL 0", "r_drawviewmodel", "0", 0}};
    std::vector<Picture> panels;
    std::vector<std::string> labels;
    for (const Case& c : cases) {
      for (const char* n : {"cl_guny", "r_viewmodel_fov", "cl_gunsway", "r_drawviewmodel",
                            "r_viewmodel_center", "r_viewmodel_hands"})
        cvars.reset(*cvars.find(n), ge::console::Source::local);
      if (c.name && cvars.set(c.name, c.value, ge::console::Source::local) !=
                        ge::console::SetResult::changed) {
        ++problems;
        lab.note(std::string("setting ") + c.name + " " + c.value + " was refused");
      }
      se::Hand hand;
      const se::HandFrame f = settle(hand, se::Weapon::shotgun, s, models, view, false, c.turn);
      ge::camera::Pose v = view;
      v.yaw = c.turn * 29 * kDt;
      float mv[3];
      muzzle_view(models[static_cast<size_t>(f.weapon)], f.placement, mv);
      panels.push_back(draw(f, vm::stage_lights(vm::StageLight::day, v, mv), v));
      labels.emplace_back(c.label);
    }
    lab.shot({"cvars", "THE PLAYER'S CVARS",
              "The shotgun at the game's place, then moved by the player's cvars, each set "
              "through a cvar registry: to the right (r_viewmodel_center 0), without the hands "
              "and arms (r_viewmodel_hands 0), a narrower lens of its own (r_viewmodel_fov, so "
              "it is drawn bigger), half a second into a turn with the sway and without it "
              "(cl_gunsway), lowered 3 cm (cl_guny), and not drawn (r_drawviewmodel).",
              "to the right, the muzzle sits right of the middle; without hands the weapon alone "
              "floats in the same place; at 50 degrees it is bigger and its muzzle still points "
              "down the corridor; with no sway it does not lag the turn; the last panel is the "
              "room alone",
              inputs},
             sheet(panels, labels, 4));
  }

  lab.metric("problems", problems, "count");
  if (problems > 0)
    lab.fail(std::to_string(problems) + " stated limits missed (the notes name them)",
             "next: change the weapon's row in games/shooters_edge/view/weapons.cpp (its length "
             "or offset) or the hand, and rerun with --compare=before.");
}

const ge::lab::LabSpec kSpec{
    "se-viewmodel",
    ge::lab::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/view (the built weapons' mounts, the hand, the cvars) over "
    "engine/viewmodel",
    "render",
    "Does each of Shooter's Edge's own weapons sit right in the view, light the room when it "
    "fires, switch cleanly and follow the player's cvars?",
    {{"models", "games/shooters_edge/content/models/view", "the folder of the view models"},
     {"width", "480", "a panel's width, px (its height is 9/16 of it)"}},
    "--width=128",
    run,
};
GE_LAB(kSpec);

}  // namespace
