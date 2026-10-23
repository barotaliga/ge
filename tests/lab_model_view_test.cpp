// engine/lab's model view, sheets and reference sheets over the test model: a shape posed from
// a clip, the levels of detail read off the meshes' names, a joint's region, the camera framing
// the whole box, faces turned away left undrawn, and a reference file parsed and laid out.
//
// ge:summary Tests the labs' model view, the model sheets and the reference sheets
// ge:keywords test lab model-view shape lod region orbit framing reference parse_refs sheet
// ge:skill labs testing
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "assets/pose.hpp"
#include "assets/test_model.hpp"
#include "check.hpp"
#include "lab/model_compare.hpp"
#include "lab/model_sheets.hpp"
#include "lab/model_view.hpp"
#include "lab/png.hpp"

using namespace ge::lab;
namespace a = ge::assets;
namespace fs = std::filesystem;

namespace {

constexpr Rgba kBack{24, 26, 32};

// Pixels in [x0, x0 + w) x [y0, y0 + h) that are not the background.
int drawn(const Picture& p, int x0, int y0, int w, int h) {
  int n = 0;
  for (int y = y0; y < y0 + h; ++y) {
    for (int x = x0; x < x0 + w; ++x) {
      const uint8_t* c = &p.rgba[(size_t(y) * size_t(p.width) + size_t(x)) * 4];
      n += (c[0] != kBack.r || c[1] != kBack.g || c[2] != kBack.b) ? 1 : 0;
    }
  }
  return n;
}

ViewStyle plain() {
  ViewStyle s;
  s.floor = false;
  s.background = kBack;
  return s;
}

}  // namespace

TEST(a_shape_is_every_vertex_posed) {
  const a::Model m = a::make_test_model();
  size_t vertices = 0, indices = 0;
  for (const a::Mesh& mesh : m.meshes) {
    vertices += mesh.vertex_count();
    indices += mesh.indices.size();
  }
  const Shape rest = shape_at(m, -1, 0);
  CHECK(rest.part.size() == vertices && rest.top.size() == vertices);
  CHECK(rest.pos.size() == vertices * 3 && rest.idx.size() == indices);
  CHECK(rest.bones.size() == m.joints.size());
  // The limb stands 1 m tall from the origin at rest; the wave bends it, so its box moves.
  const a::Box box = shape_box(rest);
  CHECK(std::fabs(box.min[1]) < 1e-4 && box.max[1] > 0.99f);
  const Shape bent = shape_at(m, a::find_clip(m, "wave"), 0.5);
  CHECK(std::fabs(shape_box(bent).max[0] - box.max[0]) > 0.05f);
}

TEST(levels_of_detail_come_from_the_names) {
  a::Model m = a::make_test_model();
  const size_t first = m.meshes.size();
  a::Mesh coarse = m.meshes[0];
  coarse.name = m.meshes[0].name + ".lod1";
  m.meshes.push_back(coarse);
  a::Mesh far = m.meshes[0];
  far.name = m.meshes[0].name + "_lod2";
  m.meshes.push_back(far);
  const LodGroups lods = lod_groups(m);
  CHECK(lods.most == 3);
  CHECK(lods.names.size() == first);
  CHECK(lods.levels[0][1] == static_cast<int>(first));
  CHECK(lods.levels[0][2] == static_cast<int>(first) + 1);
  // A group with fewer levels gives its last at a deeper one.
  const std::vector<int> at2 = meshes_at_lod(lods, 2);
  CHECK(at2.size() == first && at2[0] == static_cast<int>(first) + 1);
  for (size_t g = 1; g < first; ++g) CHECK(at2[g] == static_cast<int>(g));
}

TEST(a_region_is_the_skin_a_joint_moves_most) {
  const a::Model m = a::make_test_model();
  const Shape rest = shape_at(m, -1, 0);
  float centre[3], radius = 0;
  CHECK(region_of(m, rest, a::find_joint(m, "fin"), 0, centre, &radius));
  // The fin sits on the wrist, high on the limb; the root's own skin is low.
  CHECK(centre[1] > 0.7f && radius > 0 && radius < 0.4f);
  float low[3], r2 = 0;
  CHECK(region_of(m, rest, a::find_joint(m, "root"), 0, low, &r2));
  CHECK(low[1] < centre[1]);
  CHECK(!region_of(m, rest, 99, 0, low, &r2));
}

TEST(the_orbit_frames_the_whole_model) {
  const a::Model m = a::make_test_model();
  const Shape rest = shape_at(m, -1, 0);
  const a::Box box = shape_box(rest);
  for (double az : {0.0, 45.0, 90.0, 200.0}) {
    Picture p = create_picture(96, 96, kBack);
    draw_shape(p, 0, 0, 96, 96, orbit_cam(box, az, 15), m, rest, plain());
    const int all = drawn(p, 0, 0, 96, 96);
    const int inner = drawn(p, 2, 2, 92, 92);
    CHECK_MSG(all > 200, "az " + std::to_string(az) + ": " + std::to_string(all) + " px drawn");
    CHECK_MSG(all == inner, "az " + std::to_string(az) + ": the model touches the panel's edge");
  }
}

TEST(the_front_and_the_back_are_mirrors) {
  // Orthographic from the front and the back, the limb at rest is the same silhouette mirrored:
  // a face turned away is never drawn, so each side shows only its own faces.
  const a::Model m = a::make_test_model();
  const Shape rest = shape_at(m, -1, 0);
  const a::Box box = shape_box(rest);
  Picture front = create_picture(64, 64, kBack), back = create_picture(64, 64, kBack);
  draw_shape(front, 0, 0, 64, 64, axis_cam(box, "front"), m, rest, plain());
  draw_shape(back, 0, 0, 64, 64, axis_cam(box, "back"), m, rest, plain());
  const int f = drawn(front, 0, 0, 64, 64), b = drawn(back, 0, 0, 64, 64);
  CHECK(f > 100);
  CHECK_MSG(std::abs(f - b) <= f / 20, std::to_string(f) + " vs " + std::to_string(b));
}

TEST(the_sheets_are_their_sizes) {
  const a::Model m = a::make_test_model();
  const Shape rest = shape_at(m, -1, 0);
  const Picture turn = turntable_sheet(m, rest, 0, 64);
  CHECK(turn.width == 4 * 64 && turn.height == 2 * (64 + 14));
  const Picture axes = axes_sheet(m, rest, 0, 64);
  CHECK(axes.width == 4 * 64);
  std::string missing;
  const std::vector<int> joints = detail_joints(m, "fin,nose", 8, &missing);
  CHECK(joints.size() == 1 && missing == "nose");
  CHECK(!detail_joints(m, "", 8, &missing).empty());
  std::vector<size_t> tris;
  lod_sheet(m, [&](int) { return shape_at(m, -1, 0); }, 1, 0, 64, &tris);
  CHECK(tris.size() == 1 && tris[0] == rest.idx.size() / 3);
}

TEST(a_reference_file_reads_and_lays_out) {
  const fs::path dir = fs::temp_directory_path() / "ge_lab_model_view_test";
  fs::create_directories(dir);
  Picture photo = create_picture(80, 120, {200, 200, 210});
  CHECK(write_png((dir / "ref.png").string(), photo));
  {
    std::ofstream f(dir / "refs.txt");
    f << "# a comment\n\n";
    f << "ref=ref.png crop=10,20,40,60 az=0..90:30 el=5 pose=rest label=\"THE LIMB\"\n";
    f << "ref=ref.png az=45\n";
  }
  std::vector<RefRow> rows;
  std::string error;
  CHECK_MSG(parse_refs((dir / "refs.txt").string(), rows, &error), error);
  CHECK(rows.size() == 2);
  CHECK(rows[0].azimuths.size() == 4 && rows[0].azimuths[3] == 90);
  CHECK(rows[0].crop[2] == 40 && rows[0].label == "THE LIMB" && rows[0].elevation == 5);
  CHECK(rows[1].pose == "rest" && rows[1].azimuths.size() == 1);

  const a::Model m = a::make_test_model();
  const PoseFn pose_of = [&](const std::string& pose, int, Shape& shape, double* fy,
                             std::string* err) {
    if (pose != "rest") {
      *err = "no pose " + pose;
      return false;
    }
    shape = shape_at(m, -1, 0);
    *fy = 0;
    return true;
  };
  Picture sheet;
  CHECK_MSG(reference_sheet(rows, m, pose_of, 60, ViewStyle{}, sheet, &error), error);
  // The first row: a 40 x 60 crop at 60 px tall, then four panels as wide.
  CHECK(sheet.width >= 6 + 40 * 5 + 6 * 5);
  CHECK(sheet.height > 2 * 60);

  {
    std::ofstream f(dir / "bad.txt");
    f << "ref=ref.png zoom=2\n";
  }
  rows.clear();
  CHECK(!parse_refs((dir / "bad.txt").string(), rows, &error));
  CHECK(error.find("zoom") != std::string::npos);
  fs::remove_all(dir);
}
