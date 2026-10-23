// LAB `model-views`: an authored model LOOKED AT the way a modeller looks at it, on the sheets
// of lab/model_sheets.hpp: all the way round in perspective, from the front, the right, the back
// and above against a ruler, big with every edge and its skeleton, close up on its small parts
// (the joints' skin: `--detail`, or the parents of the skeleton's leaves), every level of detail
// side by side and as small as each is seen, and every clip as a strip from two sides. With
// `--refs=<file>` it also lays out a reference sheet (lab/model_compare.hpp): photographs of the
// real thing, cropped, each beside the model posed and turned to match (poses here: `rest`, or
// `<clip>@<seconds>`). `--model` names a .glb, `--asset` a built asset by name; with neither it is
// the test model through a .glb. CPU pictures, so in the digest (the reference sheet is not: its
// photographs are the machine's own and never committed).
//
// ge:summary Lab: a model turned round, ruled, wired, close up, by LOD, its clips and references
// ge:keywords lab model-views model turntable axes ruler topology detail close-up lod reference
// ge:keywords photograph compare refs clip strip asset glb
// ge:skill assets labs
// ge:test tests/lab_model_view_test.cpp
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "assets/gltf.hpp"
#include "assets/gltf_write.hpp"
#include "assets/test_model.hpp"
#include "lab/lab.hpp"
#include "lab/model_compare.hpp"
#include "lab/model_sheets.hpp"

namespace {

namespace a = ge::assets;
namespace fs = std::filesystem;
using ge::lab::Det;
using ge::lab::Shape;

// A built asset's model: <root>/<name>/<name>.glb under any plugin's or game's assets.
std::string asset_path(const std::string& name) {
  for (const char* top : {"plugins", "games"}) {
    std::error_code ec;
    for (const fs::directory_entry& e : fs::directory_iterator(top, ec)) {
      const fs::path p = e.path() / "assets" / name / (name + ".glb");
      if (fs::exists(p)) return p.string();
    }
  }
  return {};
}

// The pose a name says: `rest`, or `<clip>@<seconds>`.
bool pose_named(const a::Model& m, const std::string& pose, int& clip, double& t) {
  clip = -1;
  t = 0;
  if (pose == "rest") return true;
  const size_t at = pose.find('@');
  if (at == std::string::npos) return false;
  clip = a::find_clip(m, pose.substr(0, at));
  t = std::atof(pose.c_str() + at + 1);
  return clip >= 0;
}

void run(ge::lab::Lab& lab) {
  const int panel = static_cast<int>(lab.arg_num("size"));
  const int frames = static_cast<int>(lab.arg_num("frames"));
  if (panel < 64 || panel > 1024 || frames < 2 || frames > 16) {
    lab.fail("--size must be 64..1024 and --frames 2..16",
             "next: rerun with --size=256 --frames=6.");
    return;
  }
  std::string path = lab.arg("model");
  const std::string asset = lab.arg("asset");
  if (path.empty() && !asset.empty()) {
    path = asset_path(asset);
    if (path.empty()) {
      lab.fail("no built asset named '" + asset + "' under plugins/*/assets or games/*/assets",
               "next: `ge asset list` names them; run from the repository's root (skill: assets).");
      return;
    }
  }
  a::LoadResult loaded = path.empty()
                             ? a::load_gltf(a::write_glb(a::make_test_model()), "test_model.glb")
                             : a::load_gltf_file(path);
  if (!loaded.ok) {
    lab.fail(loaded.error, "next: fix the file the line names (skill: assets).");
    return;
  }
  const a::Model& m = loaded.model;
  const std::vector<std::pair<std::string, std::string>> inputs = {
      {"model", path.empty() ? "test model" : path}};

  const ge::lab::LodGroups lods = ge::lab::lod_groups(m);
  const std::vector<int> lod0 = ge::lab::meshes_at_lod(lods, 0);
  const Shape rest = ge::lab::shape_at(m, -1, 0, lod0);
  const double floor_y = ge::lab::shape_box(rest).min[1];
  lab.metric("joints", static_cast<double>(m.joints.size()), "count");
  lab.metric("lods", lods.most, "count");
  lab.metric("clips", static_cast<double>(m.clips.size()), "count");

  lab.shot(
      {"turntable", "ALL THE WAY ROUND",
       "The model at rest from eight azimuths a 45 degrees apart, 12 degrees above it, through "
       "a 30 degree lens, lit by a key over its right shoulder, on a grid a line a metre.",
       "the silhouette reads from every side; no hole, no face missing (a face wound inward "
       "is not drawn), nothing floating off the body",
       inputs},
      ge::lab::turntable_sheet(m, rest, floor_y, panel));
  lab.shot({"axes", "FRONT, RIGHT, BACK, ABOVE",
            "The model at rest orthographic from the front, its right, the back and above, a "
            "ruler up the left of each (a tick every 10 cm, the metres numbered), its size top "
            "right.",
            "the proportions against the ruler (a figure's height, a ski's length); left and "
            "right alike where they should be",
            inputs},
           ge::lab::axes_sheet(m, rest, floor_y, panel));
  lab.shot({"topology", "EDGES AND SKELETON",
            "Large: the model shaded with every triangle's edges from front left; then its "
            "skeleton over it from behind left, each joint named.",
            "triangles spent where the silhouette bends, few on flat parts; every joint inside "
            "the mesh where it bends it",
            inputs},
           ge::lab::topology_sheet(m, rest, floor_y, panel * 2));

  std::string missing;
  const std::vector<int> joints = ge::lab::detail_joints(m, lab.arg("detail"), 8, &missing);
  if (!missing.empty()) lab.note("--detail: no joint named " + missing);
  lab.metric("details", static_cast<double>(joints.size()), "count");
  if (!joints.empty()) {
    lab.shot({"details", "CLOSE UP",
              "Each detail (the skin a joint moves most, or the 15 cm round a joint whose skin is "
              "larger: a ski's tip, a pole's basket) close up from the side of the model it lies "
              "on and from the opposite side, with its edges.",
              "the small parts hold up close: no slivers, no gaps where parts meet, the shading "
              "smooth where the part is round",
              inputs},
             ge::lab::detail_sheet(m, rest, joints, panel));
  }

  std::vector<size_t> tris;
  const ge::lab::Picture lod_pic = ge::lab::lod_sheet(
      m, [&](int n) { return ge::lab::shape_at(m, -1, 0, ge::lab::meshes_at_lod(lods, n)); },
      lods.most, floor_y, panel, &tris);
  for (size_t n = 0; n < tris.size(); ++n) {
    lab.metric("lod" + std::to_string(n) + ".triangles", static_cast<double>(tris[n]), "count");
  }
  lab.shot({"lods", "LEVELS OF DETAIL",
            "Each level of detail (meshes named <name>.lod<N>) at one size with its triangles; "
            "below, each drawn 96, 48, 24 and 12 px tall and scaled up so its pixels show.",
            "each level keeps the silhouette at the size it is drawn at; a level that only "
            "differs when large can take over sooner",
            inputs},
           lod_pic);

  for (size_t c = 0; c < m.clips.size(); ++c) {
    const a::Clip& clip = m.clips[c];
    std::vector<Shape> shots;
    std::vector<std::string> labels;
    for (int k = 0; k < frames; ++k) {
      const double t = clip.duration_s() * k / (frames - 1);
      shots.push_back(ge::lab::shape_at(m, static_cast<int>(c), t, lod0));
      char b[32];
      std::snprintf(b, sizeof b, "%.2f S", t);
      labels.push_back(b);
    }
    const double azs[2] = {35, 90};
    std::vector<std::pair<std::string, std::string>> in = inputs;
    in.push_back({"clip", clip.name});
    lab.shot({"clip-" + clip.name, "CLIP " + clip.name,
              "Clip '" + clip.name +
                  "' at evenly spaced times, from front right and from the "
                  "right, every panel at one scale.",
              "the motion reads; the skin follows its bones without folding or stretching", in},
             ge::lab::strip_sheet(m, shots, labels, azs, floor_y, panel));
  }

  const std::string refs = lab.arg("refs");
  if (!refs.empty()) {
    std::vector<ge::lab::RefRow> rows;
    std::string error;
    if (!ge::lab::parse_refs(refs, rows, &error)) {
      lab.fail(error, "next: fix the reference file (lab/model_compare.hpp says its keys).");
      return;
    }
    const ge::lab::PoseFn pose_of = [&](const std::string& pose, int lod, Shape& shape, double* fy,
                                        std::string* err) {
      int clip = -1;
      double t = 0;
      if (!pose_named(m, pose, clip, t)) {
        *err = "no pose '" + pose + "': this lab takes rest or <clip>@<seconds>";
        return false;
      }
      shape = ge::lab::shape_at(m, clip, t, ge::lab::meshes_at_lod(lods, lod));
      *fy = floor_y;
      return true;
    };
    ge::lab::Picture sheet;
    if (!ge::lab::reference_sheet(rows, m, pose_of, panel, ge::lab::ViewStyle{}, sheet, &error)) {
      lab.fail(error, "next: fix the row it names (skill: assets, § Reference sheets).");
      return;
    }
    lab.shot({"refs", "BESIDE THE REFERENCES",
              "Each reference photograph, cropped, on the left; the model on the right posed and "
              "turned as the row says.",
              "the model's proportions, silhouette and pose against the photograph's; what to "
              "change in the model, not in the camera",
              inputs},
             sheet, Det::measured);
  }
}

const ge::lab::LabSpec kSpec{
    "model-views",
    ge::lab::LabKind::picture,
    "engine",
    "engine/lab/model_sheets.hpp and model_compare.hpp over engine/assets",
    "assets",
    "What does a model look like from every side, close up, at each level of detail and beside "
    "photographs of the real thing?",
    {
        {"model", "", "a .glb or .gltf (default: the test model through a .glb)"},
        {"asset", "", "a built asset by name (plugins/*/assets/<name>/<name>.glb)"},
        {"size", "256", "a panel's side, px"},
        {"frames", "6", "a clip strip's frames"},
        {"detail", "", "comma-separated joints to look at close (default: the leaves' parents)"},
        {"refs", "", "a reference file (lab/model_compare.hpp): photographs beside the model"},
    },
    "--size=96 --frames=3",
    run,
};

GE_LAB(kSpec);

}  // namespace
