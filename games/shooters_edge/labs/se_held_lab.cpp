// LAB `se-held`: A CLASSIC GAME'S OWN FIRST-PERSON WEAPONS, through its adapter
// (client/game_adapter.hpp's `held_weapons`, client/held.hpp's `load_held_set`), drawn on the CPU
// in the engine's stone corridor (viewmodel/stage.hpp) and stepped through the game's hand
// (view/hand.hpp) at the eye, where the game's files author them:
//
//   - HELD: the eight at rest, in the number keys' order.
//   - FIRED: the shotgun's shot frame by frame (its firing frames, the `fire` clip, at the
//     classic ten a second), then back at rest.
//   - BOB: the shotgun at four points of a stride, pushed forward by 0.4 of the bob, the eye
//     carrying the bob once (cl_bobhead 0) and twice (cl_bobhead 1).
//   - HELD FIRING: the nailgun and the super nailgun held firing at 144 pictures a second, each
//     shot's frame shown at once and held (r_lerpmodels 0) and turned to from the last over the
//     frame's time (r_lerpmodels 1): a filmstrip of one shot's pictures, and a chart of how far the
//     model moves from picture to picture. Held, it moves once a shot and stands still between;
//     blended, it moves a little every picture.
//   - LEFT: four weapons held right and left (Quake II's `hand 1`, `cl_gun 3`): the left drawn
//     mirrored, lit from the side it is seen on.
//   - ALL OR NONE: the same files with one weapon taken out: no weapon is drawn and the line says
//     which is missing; Shooter's Edge's own set never stands in for a classic game's.
//
// The weapons are the game's own files under `--basedir` (the folder that holds the game folder;
// `--game` a mod), else a fixture pack with the morph reader's sample model under every weapon's
// name. CPU pictures from the engine's own arithmetic, so in the digest.
//
// ge:summary Lab: a classic game's own held weapons via its adapter, at the eye, all or none
// ge:keywords lab se-held held-weapon first-person classic-weapons held_weapons load_held_set
// ge:keywords at_eye fire-clip firing-frames game-adapter gun-bob cl_bobhead left-handed mirror
// ge:skill shooter render labs
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "brush/map.hpp"
#include "camera/first_person.hpp"
#include "core/math.hpp"
#include "lab/chart.hpp"
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
  // Made small and moved out in front of the eye and below it, as a held weapon's file sits (x
  // forward, z up).
  ge::morph::Model sample = ge::morph::sample_model();
  for (ge::morph::Frame& f : sample.frames)
    for (size_t v = 0; v + 2 < f.positions.size(); v += 3) {
      for (size_t k = 0; k < 3; ++k) f.positions[v + k] *= 0.3f;
      f.positions[v] += 20;
      f.positions[v + 2] -= 8;
    }
  const std::vector<uint8_t> model = ge::morph::write_model(sample);
  int k = 0;
  for (const std::string_view name : shooters_edge::profiles::q1_game().held_weapons)
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
  const cl::GameAdapter& game = cl::qw_adapter();
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
  auto draw = [&](const se::HandFrame& f, const vm::StageLights& lights,
                  const ge::camera::Pose& at) {
    const vm::HeldModel& m = models[static_cast<size_t>(f.weapon)];
    vm::hidden_triangles(m, !f.flashing, false, hidden);
    shot.hidden = hidden;
    shot.frame_a = f.frame_a;
    shot.frame_b = f.frame_b;
    shot.frame_t = f.frame_t;
    Picture p;
    p.width = width;
    p.height = height;
    p.rgba = vm::draw_stage(stage, lights, at, m, f.placement, shot);
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
      panels.push_back(draw(f, vm::stage_lights(vm::StageLight::day, view, mid), view));
      const std::string_view file =
          shooters_edge::profiles::q1_game().held_weapons[size_t(look.weapon)];
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
      panels.push_back(draw(f, vm::stage_lights(vm::StageLight::dim, view, mid), view));
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
  // The stride's bob: the weapon pushed forward by 0.4 of it, the eye carrying it once or twice.
  {
    const ge::camera::FirstPersonRow row = ge::camera::classic_first_person(ge::brush::kUnitM);
    const double speed = 320 * ge::brush::kUnitM;
    std::vector<Picture> panels;
    std::vector<std::string> labels;
    double lo = 1e9, hi = -1e9;
    for (const int head : {0, 1}) {
      for (const double phase : {0.0, 0.25, 0.5, 0.75}) {
        const double bob = ge::camera::first_person_bob(row, phase * row.bob_cycle_s, speed);
        ge::camera::Pose at = view;
        at.eye.y += bob * (1 + head);
        se::Hand hand;
        hand.at_eye = true;
        se::HandInput in;
        in.weapon = se::Weapon::shotgun;
        in.place.bob_m = bob;
        se::HandFrame f;
        for (int i = 0; i < 30; ++i) f = se::step_hand(hand, in, settings, models, at, kDt);
        if (head == 0) {
          lo = std::min(lo, f.placement.shift_m[2]);
          hi = std::max(hi, f.placement.shift_m[2]);
        }
        panels.push_back(draw(f, vm::stage_lights(vm::StageLight::day, at, mid), at));
        char label[48];
        std::snprintf(label, sizeof label, "BOBHEAD %d  BOB %+.1f", head, bob / ge::brush::kUnitM);
        labels.emplace_back(label);
      }
    }
    lab.metric("bob.forward-travel", (hi - lo) / ge::brush::kUnitM, "units");
    lab.shot(
        {"bob",
         "THE WEAPON'S BOB",
         "The shotgun at four points of a stride at 320 units a second (the classic rig's bob, "
         "7 units down to 4 up): the weapon pushed forward by 0.4 of the bob, the eye risen "
         "and fallen by it once (cl_bobhead 0, the top row) and twice (cl_bobhead 1, the "
         "bottom row: the weapon's bob carried by the head too).",
         "the weapon a little further in at the top of the stride; the bottom row's room "
         "moving twice as far as the top's, the weapon riding with the eye",
         {{"source", real ? "game" : "fixture"}}},
        sheet(panels, labels, 4));
  }
  // Held firing: the nailguns' frames at 144 pictures a second, held and blended.
  {
    constexpr double kPictureDt = 1.0 / 144;
    constexpr double kShotS = 0.1;  // the nailguns' refire
    constexpr int kPictures = 72;   // half a second: five shots
    ge::lab::LineChart chart;
    chart.frame = {"HOW FAR THE NAILGUN MOVES FROM PICTURE TO PICTURE, HELD FIRING",
                   "144 pictures a second, a shot every 0.1 s, from the second shot on", "t (s)",
                   "the model's mean move a picture (units)"};
    chart.spikes = false;
    std::vector<Picture> panels;
    std::vector<std::string> labels;
    for (const se::Weapon w : {se::Weapon::nailgun, se::Weapon::heavy_nailgun}) {
      const vm::HeldModel& m = models[static_cast<size_t>(w)];
      const std::string name = w == se::Weapon::nailgun ? "nailgun" : "super-nailgun";
      for (const bool blend : {false, true}) {
        se::ViewSettings s = settings;
        s.lerp = blend;
        se::Hand hand;
        hand.at_eye = true;
        se::HandInput in;
        in.weapon = w;
        for (int i = 0; i < 30; ++i) se::step_hand(hand, in, s, models, view, kPictureDt);
        std::vector<float> was, now, normals;
        ge::lab::Trace line;
        line.label = name + (blend ? ", blended (r_lerpmodels 1)" : ", held (r_lerpmodels 0)");
        double next_shot = 0, sum = 0, most = 0, t = 0;
        int still = 0, moved = 0;
        for (int i = 0; i < kPictures; ++i, t += kPictureDt) {
          in.fired = t + 1e-9 >= next_shot;
          if (in.fired) next_shot += kShotS;
          const se::HandFrame f = se::step_hand(hand, in, s, models, view, kPictureDt);
          vm::pose_frames(m, f.frame_a, f.frame_b, f.frame_t, now, normals);
          if (!was.empty() && t > kShotS) {  // from the second shot: the gun turning round
            double step = 0;
            for (size_t v = 0; v + 2 < now.size(); v += 3) {
              const double dx = now[v] - was[v], dy = now[v + 1] - was[v + 1],
                           dz = now[v + 2] - was[v + 2];
              step += std::sqrt(dx * dx + dy * dy + dz * dz);
            }
            step = step * 3 / double(now.size()) / ge::brush::kUnitM;
            line.x.push_back(t);
            line.y.push_back(step);
            sum += step;
            most = std::max(most, step);
            (step < 1e-6 ? still : moved)++;
          }
          was = now;
          // One shot's pictures, from the third shot on, for the filmstrip.
          if (w == se::Weapon::heavy_nailgun && i >= 29 && i < 29 + 8 && i % 2 == 1) {
            panels.push_back(draw(f, vm::stage_lights(vm::StageLight::dim, view, mid), view));
            char label[48];
            std::snprintf(label, sizeof label, "%s %.0f MS: %d>%d %.2f", blend ? "BLEND" : "HELD",
                          t * 1e3, f.frame_a, f.frame_b, f.frame_t);
            labels.emplace_back(label);
          }
        }
        const std::string key = name + (blend ? ".blended" : ".held");
        const double mean = sum / std::max(1, still + moved);
        lab.metric(key + ".still-share", double(still) / std::max(1, still + moved), "share");
        // The largest move a picture over the mean: 1 for a steady turn, about the pictures a shot
        // (14) when the whole turn lands on one picture.
        lab.metric(key + ".jolt", mean > 0 ? most / mean : 0, "ratio");
        chart.traces.push_back(std::move(line));
      }
    }
    const ge::lab::Chart c = ge::lab::line_chart(chart);
    lab.shot({"held-firing-chart",
              c.title,
              "The nailgun and the super nailgun held firing (a shot every 0.1 s) drawn 144 times "
              "a second: how far the model's vertices move, on average, from one picture to the "
              "next. Held (r_lerpmodels 0), each shot's frame is shown at once: one big move a "
              "shot and still pictures between. Blended (r_lerpmodels 1), each shot's frame is "
              "turned to from the last over the frame's 0.1 s: a small move every picture.",
              "the held lines are spikes on a flat zero; the blended lines are low and steady",
              {{"source", real ? "game" : "fixture"}}},
             ge::lab::to_picture(c.plot));
    lab.shot({"held-firing",
              "THE SUPER NAILGUN HELD FIRING, PICTURE BY PICTURE",
              "Four pictures of one shot, 1/72 s apart (every other picture at 144 a second), "
              "the top row held (r_lerpmodels 0), the bottom row blended (r_lerpmodels 1). Each "
              "label is the time and the frames drawn: from > to, and the share of the way.",
              "the top row's barrels the same in all four; the bottom row's turning a little in "
              "each, the share climbing",
              {{"source", real ? "game" : "fixture"}}},
             sheet(panels, labels, 4));
  }
  // The gun on the left: Quake II's `hand 1` (or `cl_gun 3`) draws the right-handed picture
  // flipped, lit from the side it is seen on.
  {
    std::vector<Picture> panels;
    std::vector<std::string> labels;
    se::ViewSettings left = settings;
    const std::span<const se::WeaponLook> looks = se::weapon_looks();
    for (const int hand_side : {0, 1}) {
      left.hand = hand_side;
      for (size_t k = 1; k < looks.size(); k += 2) {
        se::Hand hand;
        hand.at_eye = true;
        se::HandInput in;
        in.weapon = looks[k].weapon;
        se::HandFrame f;
        for (int i = 0; i < 30; ++i) f = se::step_hand(hand, in, left, models, view, kDt);
        panels.push_back(draw(f, vm::stage_lights(vm::StageLight::torch, view, mid), view));
        const std::string_view file =
            shooters_edge::profiles::q1_game().held_weapons[size_t(looks[k].weapon)];
        labels.push_back(std::string(hand_side == 1 ? "LEFT " : "RIGHT ") +
                         std::string(file.substr(file.rfind('/') + 1)));
      }
    }
    lab.shot({"left",
              "THE GUN ON THE LEFT",
              "Four weapons held right (hand 0, the top row) and left (hand 1, or cl_gun 3, the "
              "bottom row) by the torch: the left is the right-handed picture flipped across the "
              "screen's middle, its light found on the side it is seen on.",
              "each bottom panel the top one's mirror image in its shape; the light on the "
              "weapons from the same side of the room in both rows",
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
