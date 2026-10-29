// LAB `viewmodel`: the first-person held weapon on the CPU, in a stone corridor (viewmodel/
// stage.hpp). Every weapon held at rest side by side; one weapon under every named light (day,
// dim, dark, a torch, its own muzzle flash, a red and a blue lamp), so the light of the place
// visibly reaches the gun; and a scripted second and a half of walking, turning, firing and
// putting it away as a strip, with the placement's numbers as a table. `--models` names a
// directory of .glb weapons (each turned muzzle forward and gripped by `fit_mount`); without it,
// a weapon built of boxes. CPU pictures from the engine's own arithmetic, so in the digest.
//
// ge:summary Lab: the held weapon in first person: every weapon, every light, walk, turn, fire
// ge:keywords lab viewmodel view-model first-person weapon gun held lineup lights torch flash
// ge:keywords dark coloured motion bob sway kick lower corridor stage
// ge:skill render labs
// ge:test tests/viewmodel_raster_test.cpp
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "assets/gltf.hpp"
#include "camera/first_person.hpp"
#include "camera/pose.hpp"
#include "core/math.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "viewmodel/held.hpp"
#include "viewmodel/place.hpp"
#include "viewmodel/stage.hpp"

namespace {

namespace vm = ge::viewmodel;
namespace fs = std::filesystem;
using ge::lab::Picture;

struct Weapon {
  std::string name;
  vm::HeldModel model;
};

// A weapon of boxes: a receiver, a barrel, a grip and a sight, in the held frame already.
vm::HeldModel box_gun() {
  vm::HeldModel m;
  vm::HeldFrame f;
  auto box = [&](float x0, float y0, float z0, float x1, float y1, float z1, float r, float g,
                 float b) {
    const float v[8][3] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                           {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
    const int faces[6][4] = {{1, 2, 6, 5}, {0, 4, 7, 3}, {3, 7, 6, 2},
                             {0, 1, 5, 4}, {4, 5, 6, 7}, {0, 3, 2, 1}};
    const float normals[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                 {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
    for (int fi = 0; fi < 6; ++fi) {
      const uint32_t base = static_cast<uint32_t>(f.positions.size() / 3);
      for (int c = 0; c < 4; ++c) {
        f.positions.insert(f.positions.end(), v[faces[fi][c]], v[faces[fi][c]] + 3);
        f.normals.insert(f.normals.end(), normals[fi], normals[fi] + 3);
        m.colour.insert(m.colour.end(), {r, g, b});
      }
      const float* a = v[faces[fi][0]];
      const float* bb = v[faces[fi][1]];
      const float* c = v[faces[fi][2]];
      const float e1[3] = {bb[0] - a[0], bb[1] - a[1], bb[2] - a[2]};
      const float e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
      const float cr[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                           e1[0] * e2[1] - e1[1] * e2[0]};
      if (cr[0] * normals[fi][0] + cr[1] * normals[fi][1] + cr[2] * normals[fi][2] >= 0)
        m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
      else
        m.indices.insert(m.indices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
    }
  };
  box(-0.03f, -0.02f, -0.12f, 0.03f, 0.06f, 0.18f, 0.25f, 0.26f, 0.28f);  // receiver
  box(-0.015f, 0.01f, 0.18f, 0.015f, 0.04f, 0.46f, 0.18f, 0.18f, 0.2f);   // barrel
  box(-0.02f, -0.14f, -0.06f, 0.02f, -0.02f, 0.0f, 0.35f, 0.22f, 0.12f);  // grip
  box(-0.008f, 0.06f, 0.10f, 0.008f, 0.08f, 0.14f, 0.7f, 0.7f, 0.72f);    // sight
  m.frames.push_back(std::move(f));
  return m;
}

// The mount a glb weapon gets: its barrel turned forward (the kit's point along -z in the engine's
// frame), up +y, gripped low and a third of the way from the back, at 0.6 of the kit's size so the
// biggest still leaves the picture to the world; an upright melee weapon (taller than long) is held
// upright by its handle.
vm::Mount mount_for(const vm::HeldModel& m) {
  float lo[3], hi[3];
  vm::held_bounds(m, lo, hi);
  vm::Mount mount;
  mount.muzzle_negative = true;  // the kit's barrels point along its -z once in the engine's frame
  mount.grip[0] = 0.5;
  mount.grip[1] = 0.3;
  mount.grip[2] = 0.3;
  mount.scale = 0.6;
  if (hi[1] - lo[1] > hi[2] - lo[2]) {  // held upright by the bottom of its handle
    mount.muzzle_negative = false;
    mount.scale = 1.0;
    mount.grip[1] = 0.0;
    mount.grip[2] = 0.5;
  }
  return mount;
}

bool load_weapons(const std::string& dir, std::vector<Weapon>& out, std::string* error) {
  std::error_code ec;
  std::vector<fs::path> files;
  for (const fs::directory_entry& e : fs::directory_iterator(dir, ec))
    if (e.path().extension() == ".glb") files.push_back(e.path());
  if (ec || files.empty()) {
    *error = "no .glb files in '" + dir + "'";
    return false;
  }
  std::sort(files.begin(), files.end());
  for (const fs::path& p : files) {
    ge::assets::LoadResult r = ge::assets::load_gltf_file(p.string());
    if (!r.ok) {
      *error = p.string() + ": " + r.error;
      return false;
    }
    Weapon w{p.stem().string(), vm::held_from_model(r.model)};
    vm::fit_mount(w.model, mount_for(w.model));
    out.push_back(std::move(w));
  }
  return true;
}

ge::camera::Pose stage_view() {
  ge::camera::Pose v;
  v.eye = {0, 1.6, 0};
  v.yaw = 0;
  v.pitch = -0.04;
  v.fov_y = ge::camera::fov_y_of(90.0 * ge::core::kPi / 180.0, 16.0 / 9.0);
  return v;
}

// The muzzle's place in the view: the middle of the model's front face, placed.
void muzzle_of(const vm::HeldModel& m, const vm::Placement& p, float out[3]) {
  float lo[3], hi[3];
  vm::held_bounds(m, lo, hi);
  const float tip[3] = {0.5f * (lo[0] + hi[0]), lo[1] + 0.7f * (hi[1] - lo[1]), hi[2] + 0.02f};
  vm::place_point(p, tip, out);
}

Picture to_picture(const std::vector<uint8_t>& rgba, int w, int h) {
  Picture p;
  p.width = w;
  p.height = h;
  p.rgba = rgba;
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

void run(ge::lab::Lab& lab) {
  const int width = static_cast<int>(lab.arg_num("width"));
  if (width < 96 || width > 1920) {
    lab.fail("--width must be 96..1920", "next: rerun with --width=480.");
    return;
  }
  const int height = width * 9 / 16;
  const std::string dir = lab.arg("models");
  const std::string row_name = lab.arg("row");
  const vm::PlaceRow row = row_name == "modern" ? vm::modern_row() : vm::classic_row();
  if (row_name != "classic" && row_name != "modern") {
    lab.fail("--row must be classic or modern", "next: rerun with --row=classic.");
    return;
  }
  std::vector<Weapon> weapons;
  if (dir.empty()) {
    weapons.push_back({"box-gun", box_gun()});
  } else {
    std::string error;
    if (!load_weapons(dir, weapons, &error)) {
      lab.fail(error,
               "next: --models names a directory of .glb weapons, e.g. "
               "--models=games/shooters_edge/content/models/view (run from the root).");
      return;
    }
  }
  const std::vector<std::pair<std::string, std::string>> inputs = {
      {"models", dir.empty() ? "box gun" : dir},
      {"row", row_name},
      {"width", std::to_string(width)}};

  const vm::Stage stage = vm::corridor_stage();
  const ge::camera::Pose view = stage_view();
  vm::StageShot shot;
  shot.width = width;
  shot.height = height;
  vm::PlaceState rest_state;
  const vm::Placement rest = vm::frame_viewmodel(row, vm::PlaceInput{}, rest_state, 1.0 / 60);

  // Every weapon at rest, by day.
  {
    std::vector<Picture> panels;
    std::vector<std::string> labels;
    for (const Weapon& w : weapons) {
      float muzzle[3];
      muzzle_of(w.model, rest, muzzle);
      const vm::StageLights l = vm::stage_lights(vm::StageLight::day, view, muzzle);
      panels.push_back(
          to_picture(vm::draw_stage(stage, l, view, w.model, rest, shot), width, height));
      labels.push_back(w.name);
      float lo[3], hi[3];
      vm::held_bounds(w.model, lo, hi);
      lab.metric("length-" + w.name, hi[2] - lo[2], "m");
    }
    lab.shot({"lineup", "EVERY WEAPON HELD",
              "Each weapon held at rest in first person in the stone corridor by day, the grip at "
              "the row's offset (right, down, forward of the eye), drawn through its own lens over "
              "the room.",
              "the muzzle points down the corridor; the gun sits low right and is not cut by the "
              "picture's edge or the near plane; its size reads as a weapon held, not a toy",
              inputs},
             sheet(panels, labels, weapons.size() > 2 ? 3 : static_cast<int>(weapons.size())));
  }

  // One weapon under every light.
  const std::string pick = lab.arg("weapon");
  const Weapon* chosen = &weapons[0];
  for (const Weapon& w : weapons)
    if (w.name == pick) chosen = &w;
  {
    std::vector<Picture> panels;
    std::vector<std::string> labels;
    float muzzle[3];
    muzzle_of(chosen->model, rest, muzzle);
    for (vm::StageLight kind : vm::kStageLights) {
      const vm::StageLights l = vm::stage_lights(kind, view, muzzle);
      panels.push_back(
          to_picture(vm::draw_stage(stage, l, view, chosen->model, rest, shot), width, height));
      labels.emplace_back(vm::stage_light_name(kind));
    }
    std::vector<std::pair<std::string, std::string>> in = inputs;
    in.push_back({"weapon", chosen->name});
    lab.shot({"lights", "ONE WEAPON UNDER EVERY LIGHT",
              "The same weapon at rest under the stage's six lights: day (a sky and a sun), dim, "
              "dark (the floor alone), a torch on the left wall, its own muzzle flash, and a red "
              "lamp left and a blue one right.",
              "the gun takes the room's light: dark in the dark but still a silhouette, warm on "
              "its left by the torch, lit from the muzzle by the flash, red on its left and blue "
              "on its right",
              in},
             sheet(panels, labels, 3));
  }

  // A scripted moment: walk, turn right, fire, put it away.
  {
    const ge::camera::FirstPersonRow eye_row;
    vm::PlaceState st;
    vm::PlaceInput in;
    const double dt = 1.0 / 60;
    const double marks[] = {0.15, 0.30, 0.45, 0.85, 1.05, 1.32, 1.40, 1.75};
    const char* what[] = {"walk", "walk", "walk", "turn", "turn", "fire", "kick", "away"};
    std::vector<Picture> panels;
    std::vector<std::string> labels;
    ge::lab::LabTable table{"the placement through the script",
                            {"t s", "shift right m", "up m", "forward m", "yaw rad", "pitch rad"},
                            {}};
    double most_lag = 0, most_back = 0, most_rise = 0;
    size_t next = 0;
    for (int i = 1; i <= 120; ++i) {
      const double t = i * dt;
      const bool walking = t < 0.6;
      in.ground_speed = walking ? 6.0 : 0.0;
      in.stride_s = t;
      in.bob_m = ge::camera::first_person_bob(eye_row, t, in.ground_speed);
      if (t > 0.6 && t < 1.0) in.look_yaw += 2.5 * dt;
      in.shot = i == 78 ? 1.0 : 0.0;
      in.lowered = t > 1.5 ? 1.0 : 0.0;
      const vm::Placement p = vm::frame_viewmodel(row, in, st, dt);
      most_lag = std::min(most_lag, p.turn[0]);
      most_back = std::min(most_back, p.shift_m[2]);
      most_rise = std::max(most_rise, p.turn[1]);
      if (i % 6 == 0) {
        char c[6][16];
        std::snprintf(c[0], 16, "%.2f", t);
        for (int k = 0; k < 3; ++k) std::snprintf(c[1 + k], 16, "%.4f", p.shift_m[k]);
        std::snprintf(c[4], 16, "%.4f", p.turn[0]);
        std::snprintf(c[5], 16, "%.4f", p.turn[1]);
        table.rows.push_back({c[0], c[1], c[2], c[3], c[4], c[5]});
      }
      if (next < std::size(marks) && t + 1e-9 >= marks[next]) {
        ge::camera::Pose v = view;
        v.yaw = in.look_yaw;
        v.eye.y += in.bob_m;
        float muzzle[3];
        muzzle_of(chosen->model, p, muzzle);
        const vm::StageLights l = vm::stage_lights(
            i >= 78 && i <= 81 ? vm::StageLight::flash : vm::StageLight::torch, v, muzzle);
        panels.push_back(
            to_picture(vm::draw_stage(stage, l, v, chosen->model, p, shot), width, height));
        char b[32];
        std::snprintf(b, sizeof b, "%s %.2f S", what[next], t);
        labels.push_back(b);
        ++next;
      }
    }
    lab.metric("sway-most-lag", most_lag, "rad");
    lab.metric("kick-most-back", most_back, "m");
    lab.metric("kick-most-rise", most_rise, "rad");
    lab.table(table);
    std::vector<std::pair<std::string, std::string>> inp = inputs;
    inp.push_back({"weapon", chosen->name});
    lab.shot({"motion", "WALK, TURN, FIRE, PUT AWAY",
              "Two seconds scripted at 60 Hz by the torch: a walk at 6 m/s (the eye's bob pushes "
              "the gun forward), a turn to the right at 2.5 rad/s (the gun lags left and catches "
              "up), one shot (a muzzle flash; the gun kicks back and up) and the weapon put away.",
              "the bob is visible but small; the lag reads as weight, not as slop; the kick snaps "
              "and settles within a few frames; put away, the gun leaves the picture downward",
              inp},
             sheet(panels, labels, 4));
  }
}

const ge::lab::LabSpec kSpec{
    "viewmodel",
    ge::lab::LabKind::picture,
    "engine",
    "engine/viewmodel (held.hpp, place.hpp, light.hpp, raster.hpp, stage.hpp)",
    "render",
    "Does a first-person weapon sit right in the view, move with weight, and take the light of "
    "the place it is in?",
    {
        {"models", "", "a directory of .glb weapons (default: a weapon of boxes)"},
        {"weapon", "", "the weapon the lights and the motion are shown with (default: the first)"},
        {"row", "classic", "the placement row: classic or modern"},
        {"width", "480", "a panel's width, px (its height is 9/16 of it)"},
    },
    "--width=128",
    run,
};

GE_LAB(kSpec);

}  // namespace
