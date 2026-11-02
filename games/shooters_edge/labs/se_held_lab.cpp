// LAB `se-held`: A CLASSIC GAME'S OWN FIRST-PERSON WEAPONS, through its adapter
// (client/game_adapter.hpp's `held_weapons`, client/held.hpp's `load_held_set`), drawn on the CPU
// in the engine's stone corridor (viewmodel/stage.hpp) and stepped through the game's hand
// (view/hand.hpp) at the eye, where the game's files author them:
//
//   - HELD: the eight at rest, in the number keys' order.
//   - FIRED: the shotgun's shot frame by frame (its firing frames, the `fire` clip, at the
//     classic ten a second), then back at rest.
//   - ALL OR NONE: the same files with one weapon taken out: no weapon is drawn and the line says
//     which is missing; Shooter's Edge's own set never stands in for a classic game's.
//
// The weapons are the game's own files under `--basedir` (the folder that holds the game folder;
// `--game` a mod), else a fixture pack with the morph reader's sample model under every weapon's
// name. CPU pictures from the engine's own arithmetic, so in the digest.
//
// ge:summary Lab: a classic game's own held weapons via its adapter, at the eye, all or none
// ge:keywords lab se-held held-weapon first-person classic-weapons held_weapons load_held_set
// ge:keywords at_eye fire-clip firing-frames game-adapter
// ge:skill shooter render labs
#include <string>
#include <utility>
#include <vector>

#include "core/math.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "morph/write.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/held.hpp"
#include "shooters_edge/view/hand.hpp"
#include "vfs/pak.hpp"
#include "viewmodel/raster.hpp"
#include "viewmodel/stage.hpp"

namespace {

namespace se = shooters_edge::view;
namespace cl = shooters_edge::client;
namespace vm = ge::viewmodel;
using ge::lab::Picture;

constexpr double kDt = 1.0 / 60;

ge::camera::Pose stage_view() {
  ge::camera::Pose v;
  v.eye = {0, 1.6, 0};
  v.pitch = -0.04;
  v.fov_y = ge::camera::fov_y_of(90.0 * ge::core::kPi / 180.0, 16.0 / 9.0);
  return v;
}

Picture sheet(const std::vector<Picture>& panels, const std::vector<std::string>& labels,
              int cols) {
  const int w = panels[0].width, h = panels[0].height, gap = 4;
  const int rows = (static_cast<int>(panels.size()) + cols - 1) / cols;
  Picture out =
      ge::lab::create_picture(cols * (w + gap) + gap, rows * (h + gap) + gap, {16, 17, 20});
  for (size_t i = 0; i < panels.size(); ++i) {
    const int x = gap + static_cast<int>(i % size_t(cols)) * (w + gap);
    const int y = gap + static_cast<int>(i / size_t(cols)) * (h + gap);
    ge::lab::blit(out, panels[i], x, y);
    ge::lab::fill_rect(out, x, y, ge::lab::text_width(labels[i], 2) + 8, 22, {0, 0, 0, 150});
    ge::lab::draw_text(out, x + 4, y + 4, labels[i], {240, 240, 240}, 2);
  }
  return out;
}

// The fixture: the sample model under each held weapon's name and a grey ramp palette, leaving
// out `skip` (-1: none).
bool fixture(int skip, cl::SearchPath& files, std::string& error) {
  std::vector<uint8_t> palette(768);
  for (size_t i = 0; i < palette.size(); ++i) palette[i] = uint8_t((i / 3) ^ (i % 3 == 0 ? 0 : 7));
  std::vector<std::pair<std::string, std::vector<uint8_t>>> listed = {{"gfx/palette.lmp", palette}};
  // Made small and moved out in front of the eye and below it, as a held weapon's file sits (x forward, z up).
  ge::morph::Model sample = ge::morph::sample_model();
  for (ge::morph::Frame& f : sample.frames)
    for (size_t v = 0; v + 2 < f.positions.size(); v += 3) {
      for (size_t k = 0; k < 3; ++k) f.positions[v + k] *= 0.3f;
      f.positions[v] += 20;
      f.positions[v + 2] -= 8;
    }
  const std::vector<uint8_t> model = ge::morph::write_model(sample);
  int k = 0;
  for (const std::string_view name : shooters_edge::profiles::classic_game().held_weapons)
    if (k++ != skip) listed.push_back({std::string(name), model});
  std::vector<uint8_t> pak;
  files = {};
  return ge::vfs::write_pak(listed, &pak, &error) &&
         cl::add_archive(files, "pak0.pak", std::move(pak), error);
}

void run(ge::lab::Lab& lab) {
  const int width = static_cast<int>(lab.arg_num("width"));
  if (width < 96 || width > 1920) {
    lab.fail("--width must be 96..1920", "next: rerun with --width=320.");
    return;
  }
  const int height = width * 9 / 16;
  std::string error;
  cl::SearchPath files;
  const bool real = !lab.arg("basedir").empty();
  if (real ? !cl::open_game_dir(lab.arg("basedir"), lab.arg("game"), files, error)
           : !fixture(-1, files, error)) {
    lab.fail(error,
             "next: pass --basedir=<the folder that holds the game folder>. Skill: shooter.");
    return;
  }
  if (!real) files.folder = "fixture";  // a classic game's folder is open
  const cl::GameAdapter& game = cl::classic_adapter();
  std::vector<vm::HeldModel> models;
  const bool at_eye = cl::load_held_set(game, files, {}, models);
  lab.metric("weapons", double(models.size()), "count");
  lab.metric("at-eye", at_eye ? 1 : 0, "bool");
  if (!at_eye || models.size() != size_t(se::kWeapons)) {
    lab.fail("the game's held weapons did not load (the status line above says why)",
             "next: check the game folder holds every progs/v_*.mdl. Skill: shooter.");
    return;
  }
  // All or none: one weapon taken out of the fixture, and nothing is drawn.
  {
    cl::SearchPath part;
    std::vector<vm::HeldModel> none;
    if (fixture(se::kWeapons - 1, part, error)) {
      part.folder = "fixture";
      const bool got = cl::load_held_set(game, part, {}, none);
      lab.metric("short-set.weapons", double(none.size()), "count");
      lab.metric("short-set.at-eye", got ? 1 : 0, "bool");
    }
  }
  const vm::Stage stage = vm::corridor_stage();
  const ge::camera::Pose view = stage_view();
  vm::StageShot shot;
  shot.width = width;
  shot.height = height;
  const se::ViewSettings settings;
  std::vector<uint8_t> hidden;
  auto draw = [&](const se::HandFrame& f, const vm::StageLights& lights) {
    const vm::HeldModel& m = models[static_cast<size_t>(f.weapon)];
    vm::hidden_triangles(m, !f.flashing, false, hidden);
    shot.hidden = hidden;
    shot.frame_a = f.frame_a;
    shot.frame_b = f.frame_b;
    shot.frame_t = f.frame_t;
    Picture p;
    p.width = width;
    p.height = height;
    p.rgba = vm::draw_stage(stage, lights, view, m, f.placement, shot);
    return p;
  };
  const float mid[3] = {0, -0.1f, 0.4f};
  // The eight at rest.
  {
    std::vector<Picture> panels;
    std::vector<std::string> labels;
    for (const se::WeaponLook& look : se::weapon_looks()) {
      se::Hand hand;
      hand.at_eye = true;
      se::HandInput in;
      in.weapon = look.weapon;
      se::HandFrame f;
      for (int i = 0; i < 30; ++i) f = se::step_hand(hand, in, settings, models, view, kDt);
      panels.push_back(draw(f, vm::stage_lights(vm::StageLight::day, view, mid)));
      const std::string_view file =
          shooters_edge::profiles::classic_game().held_weapons[size_t(look.weapon)];
      labels.emplace_back(file.substr(file.rfind('/') + 1));
    }
    lab.shot({"held",
              "THE GAME'S OWN WEAPONS, HELD",
              "Every held weapon the game's files carry, read through its adapter and placed at "
              "the eye where the files author them, at rest in the corridor by day.",
              "eight weapons, each in the lower middle as the game holds it",
              {{"source", real ? "game" : "fixture"}}},
             sheet(panels, labels, 4));
  }
  // The shotgun's shot, frame by frame.
  {
    std::vector<Picture> panels;
    std::vector<std::string> labels;
    se::Hand hand;
    hand.at_eye = true;
    se::HandInput in;
    in.weapon = se::Weapon::shotgun;
    for (int i = 0; i < 30; ++i) se::step_hand(hand, in, settings, models, view, kDt);
    for (int i = 0; i < 48; ++i) {
      in.fired = i == 0;
      const se::HandFrame f = se::step_hand(hand, in, settings, models, view, kDt);
      if (i % 6 != 0) continue;
      panels.push_back(draw(f, vm::stage_lights(vm::StageLight::dim, view, mid)));
      labels.push_back("frame " + std::to_string(f.frame_a));
      lab.metric("fire.frame-" + std::to_string(i), f.frame_a, "index");
    }
    lab.shot({"fired",
              "THE SHOTGUN'S SHOT, FRAME BY FRAME",
              "A shot plays the model's own firing frames (its fire clip, ten a second), a "
              "panel every 0.1 s, then the rest frame.",
              "the frame number climbing through the shot, then back at 0",
              {{"source", real ? "game" : "fixture"}}},
             sheet(panels, labels, 4));
  }
}

const ge::lab::LabSpec kSpec{
    "se-held",
    ge::lab::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/client/game_adapter (held_weapons) and view/hand (at the eye)",
    "shooter",
    "Does a classic game draw only its own first-person weapons, read through its adapter and "
    "held at the eye, with their own firing frames?",
    {{"basedir", "", "the folder that holds the game folder (empty: the fixture pack)"},
     {"game", "", "a mod folder over the base"},
     {"width", "320", "a panel's width, px (its height is 9/16 of it)"}},
    "--width=128",
    run,
};
GE_LAB(kSpec);

}  // namespace
