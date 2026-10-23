// LAB `skier-views`: the skier model as the game poses it (app/rig_pose.hpp from a `SkierState`),
// on the model sheets (engine/lab/model_sheets.hpp): every stance (standing, carving each way,
// the tuck, the air, down) from four sides, the carve all the way round and close up, and with
// `--refs=<file>` a reference sheet of photographs of skiers beside the model posed the same
// (engine/lab/model_compare.hpp). A row's `pose` is a stance's name, then any of the state's
// numbers to change, `name,field:value,...` (the fields: lean, edge, tuck, speed, air (s in the
// air), down (0 or 1)), e.g. `carve-left,lean:-0.8,tuck:0.4`. A run of the game's own motion is
// `make lab LAB=anim` (which bakes it into a clip, `skier-run.glb`), then `make lab
// LAB=model-views ARGS=--model=previews/anim/skier-run.glb`. CPU pictures, so in the digest (the
// reference sheet is not).
//
// ge:summary Lab: the skier model in the game's stances from every side, close up, beside photos
// ge:keywords lab skier-views skier model stance carve tuck air down pose reference photograph
// ge:keywords compare turntable detail rig-pose
// ge:skill skier labs
// ge:test tests/skier_rig_pose_test.cpp
#include <cstdlib>
#include <string>
#include <vector>

#include "assets/gltf.hpp"
#include "lab/lab.hpp"
#include "lab/model_compare.hpp"
#include "lab/model_sheets.hpp"
#include "skier/app/rig_pose.hpp"

namespace {

namespace sk = ge::skier;
namespace a = ge::assets;
using ge::lab::Det;
using ge::lab::Shape;

constexpr const char* kModelPath = "plugins/skier/assets/skier/skier.glb";

struct Stance {
  const char* name;
  double lean, edge, tuck, speed, air;
  bool down;
};
// The stances the sheets show, in order (rad, rad, 0..1, m/s, s in the air).
constexpr Stance kStances[] = {
    {"stand", 0, 0, 0, 0, 0, false},
    {"carve-left", -0.6, -0.7, 0.2, 14, 0, false},
    {"carve-right", 0.6, 0.7, 0.2, 14, 0, false},
    {"tuck", 0, 0, 1, 25, 0, false},
    {"air", 0, 0, 0.3, 15, 0.5, false},
    {"down", 0.3, 0, 0, 3, 0, true},
};

// The state a pose string says: a stance by name, then field:value changes.
bool state_of(const std::string& pose, sk::SkierState& s, std::string* error) {
  const size_t comma = pose.find(',');
  const std::string name = pose.substr(0, comma);
  const Stance* st = nullptr;
  for (const Stance& k : kStances) {
    if (name == k.name) st = &k;
  }
  if (!st) {
    *error =
        "no stance '" + name + "'; the stances: stand, carve-left, carve-right, tuck, air, down";
    return false;
  }
  s = {};
  s.lean = st->lean;
  s.edge = st->edge;
  s.tuck = st->tuck;
  s.speed = st->speed;
  s.vz = st->speed;
  s.air_time = st->air;
  s.on_snow = st->air <= 0;
  s.down = st->down;
  size_t at = comma;
  while (at != std::string::npos) {
    const size_t next = pose.find(',', at + 1);
    const std::string kv =
        pose.substr(at + 1, next == std::string::npos ? std::string::npos : next - at - 1);
    const size_t colon = kv.find(':');
    if (colon == std::string::npos) {
      *error = "'" + kv;
      *error += "' in pose '" + pose + "' is not field:value";
      return false;
    }
    const std::string f = kv.substr(0, colon);
    const double v = std::atof(kv.c_str() + colon + 1);
    if (f == "lean") {
      s.lean = v;
    } else if (f == "edge") {
      s.edge = v;
    } else if (f == "tuck") {
      s.tuck = v;
    } else if (f == "speed") {
      s.speed = v;
      s.vz = v;
    } else if (f == "air") {
      s.air_time = v;
      s.on_snow = v <= 0;
    } else if (f == "down") {
      s.down = v != 0;
    } else {
      *error = "no field '" + f + "'; the fields: lean, edge, tuck, speed, air, down";
      return false;
    }
    at = next;
  }
  return true;
}

// The model posed as the game poses it for `s`, settled (no lag left over).
Shape shape_for(const a::Model& m, const sk::RigBinding& binding, const sk::SkierState& s) {
  sk::RigPoser poser;
  sk::RigPose pose{};
  poser.pose(s, 0, pose);
  std::vector<a::Trs> local(m.joints.size());
  std::vector<a::Affine> global(m.joints.size());
  sk::rig_locals(m, binding, pose, local);
  a::pose_globals(m, local, global);
  return ge::lab::shape_of(m, global);
}

void run(ge::lab::Lab& lab) {
  const int panel = static_cast<int>(lab.arg_num("size"));
  if (panel < 64 || panel > 1024) {
    lab.fail("--size must be 64..1024", "next: rerun with --size=256.");
    return;
  }
  a::LoadResult loaded = a::load_gltf_file(kModelPath);
  if (!loaded.ok) {
    lab.fail("the skier model did not load: " + loaded.error,
             "next: run from the repository's root, or rebuild it with `make ge ARGS=\"asset "
             "build skier\"` (skill: assets).");
    return;
  }
  const a::Model& m = loaded.model;
  const sk::RigBinding binding = sk::bind_rig(m);
  if (!binding.complete) {
    lab.fail("the skier model lacks some of the rig's joints (app/rig.hpp)",
             "next: rebuild it with `make ge ARGS=\"asset build skier\"` (skill: assets).");
    return;
  }
  const std::vector<std::pair<std::string, std::string>> inputs = {{"model", kModelPath}};

  std::vector<Shape> stances;
  std::vector<std::string> names;
  for (const Stance& st : kStances) {
    sk::SkierState s;
    std::string error;
    state_of(st.name, s, &error);
    stances.push_back(shape_for(m, binding, s));
    std::string n = st.name;
    for (char& c : n) c = c >= 'a' && c <= 'z' ? static_cast<char>(c - 32) : c;
    names.push_back(n);
  }
  const double sides[4] = {0, 90, 180, 315};
  lab.shot({"stances", "THE STANCES FROM FOUR SIDES",
            "The model as the game poses it for each stance (standing, carving left and right, "
            "the tuck, in the air, down), settled, from the front, the right, the back and front "
            "left, every panel at one scale, the snow at the grid.",
            "the skis flat on the snow or tipped on their edges, never into it; the body inclined "
            "into the carve, the hips over the feet; the tuck folded; down lying on the snow",
            inputs},
           ge::lab::strip_sheet(m, stances, names, sides, 0, panel));
  lab.shot({"carve-turntable", "THE CARVE ALL THE WAY ROUND",
            "The model carving left (lean -0.6 rad, edge -0.7 rad), from eight azimuths.",
            "the angulation reads from every side: legs inclined more than the torso, the head "
            "level",
            inputs},
           ge::lab::turntable_sheet(m, stances[1], 0, panel));
  std::string missing;
  const std::vector<int> joints = ge::lab::detail_joints(m, lab.arg("detail"), 8, &missing);
  if (!missing.empty()) lab.note("--detail: no joint named " + missing);
  lab.shot({"carve-details", "THE CARVE CLOSE UP",
            "Each detail of the model carving left, close up from outside and from the opposite "
            "side, with its edges.",
            "the boots in their bindings, the gloves on the grips, the edges on the snow", inputs},
           ge::lab::detail_sheet(m, stances[1], joints, panel));

  const std::string refs = lab.arg("refs");
  if (refs.empty()) return;
  std::vector<ge::lab::RefRow> rows;
  std::string error;
  if (!ge::lab::parse_refs(refs, rows, &error)) {
    lab.fail(error, "next: fix the reference file (lab/model_compare.hpp says its keys).");
    return;
  }
  const ge::lab::PoseFn pose_of = [&](const std::string& pose, int, Shape& shape, double* fy,
                                      std::string* err) {
    sk::SkierState s;
    if (!state_of(pose, s, err)) return false;
    shape = shape_for(m, binding, s);
    *fy = 0;
    return true;
  };
  ge::lab::Picture sheet;
  if (!ge::lab::reference_sheet(rows, m, pose_of, panel, ge::lab::ViewStyle{}, sheet, &error)) {
    lab.fail(error, "next: fix the row it names (skill: assets, § Reference sheets).");
    return;
  }
  lab.shot({"refs", "BESIDE THE REFERENCES",
            "Each reference photograph, cropped, on the left; the skier model on the right in the "
            "row's stance, turned as the row says.",
            "the proportions (head, torso, legs, skis), the stance's angles and the gear against "
            "the photograph's; what to change in the model or the poser, not in the camera",
            inputs},
           sheet, Det::measured);
}

const ge::lab::LabSpec kSpec{
    "skier-views",
    ge::lab::LabKind::picture,
    "plugin:skier",
    "plugins/skier/app/rig_pose.hpp over the skier model, on engine/lab/model_sheets.hpp",
    "skier",
    "What does the skier model look like in each of the game's stances, from every side, close "
    "up and beside photographs of skiers?",
    {
        {"size", "256", "a panel's side, px"},
        {"detail", "", "comma-separated joints to look at close (default: the model's ends)"},
        {"refs", "", "a reference file (lab/model_compare.hpp): photographs beside the model"},
    },
    "--size=96",
    run,
};

GE_LAB(kSpec);

}  // namespace
