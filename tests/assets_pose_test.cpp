// engine/assets/model.hpp, pose.hpp and test_model.hpp, held by the engine's own checks:
//
//   * THE ALGEBRA: an affine times its inverse is the identity; a TRS's affine moves a point as
//     its parts do.
//   * THE TEST MODEL: every vertex's weights sum to 1; at rest the palette is the identity and
//     the skinned mesh is the bind mesh.
//   * THE CLIPS: `wave` bends the limb's top to the right (+x) and leaves its foot; the fin
//     stays rigid in every frame; `twist` turns the front (+z) to the right (+x), clockwise
//     from above; a looped time wraps, a held one stops at the ends; the reach box holds every
//     frame.
//   * THE GOLDEN: the posed mesh's bits at a sweep of times, one FNV-1a 64 a row, as
//     tests/golden/assets_pose.txt keeps. A deliberate move rewrites the file:
//     `GE_GOLDEN_WRITE=1 build/<preset>/tests/assets_pose_test`, and the PR says why.
//
// ge:summary Tests for posing: the affine algebra, the test model, the clips, a pose golden
// ge:keywords test assets pose skinning palette clip wave twist rigid reach golden
// ge:skill assets testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "assets/model.hpp"
#include "assets/pose.hpp"
#include "assets/test_model.hpp"
#include "check.hpp"
#include "golden.hpp"

namespace {

namespace a = ge::assets;

constexpr const char* kGolden = "tests/golden/assets_pose.txt";

// The model posed by `clip` (or at rest, -1) at t: positions and normals.
struct Posed {
  std::vector<float> pos, nrm;
};
Posed pose(const a::Model& m, int clip, double t, bool loop = false) {
  std::vector<a::Trs> local(m.joints.size());
  std::vector<a::Affine> global(m.joints.size()), palette(m.joints.size());
  if (clip < 0) {
    a::rest_pose(m, local);
  } else {
    a::sample_clip(m, m.clips[static_cast<size_t>(clip)], t, loop, local);
  }
  a::pose_globals(m, local, global);
  a::skin_palette(m, global, palette);
  Posed p;
  p.pos.resize(size_t{m.meshes[0].vertex_count()} * 3);
  p.nrm.resize(p.pos.size());
  a::skin_mesh(m.meshes[0], palette, p.pos, p.nrm);
  return p;
}

// The mean of the tube's vertices in ring r (six a ring, the rings first in the mesh).
void ring_centre(const Posed& p, int r, float out[3]) {
  for (int k = 0; k < 3; ++k) out[k] = 0;
  for (int v = 0; v < 6; ++v) {
    for (int k = 0; k < 3; ++k) out[k] += p.pos[size_t(r * 6 + v) * 3 + k] / 6.0f;
  }
}

float dist(const float* x, const float* y) {
  return std::sqrt((x[0] - y[0]) * (x[0] - y[0]) + (x[1] - y[1]) * (x[1] - y[1]) +
                   (x[2] - y[2]) * (x[2] - y[2]));
}

uint64_t fnv(const std::vector<float>& v, uint64_t h = 0xcbf29ce484222325ull) {
  for (float f : v) {
    uint32_t bits;
    std::memcpy(&bits, &f, 4);
    for (int k = 0; k < 4; ++k) {
      h ^= (bits >> (8 * k)) & 0xff;
      h *= 0x100000001b3ull;
    }
  }
  return h;
}

}  // namespace

TEST(affine_inverse_and_trs_hold) {
  a::Trs t;
  t.t[0] = 1, t.t[1] = -2, t.t[2] = 0.5f;
  const float h = std::sqrt(0.5f);
  t.r[1] = h, t.r[3] = h;  // a quarter turn about +y
  t.s[0] = 2, t.s[1] = 2, t.s[2] = 2;
  const a::Affine m = a::to_affine(t);
  const float p[3] = {0, 0, 1};
  float q[3];
  a::transform_point(m, p, q);
  // +z scaled by 2 and turned a quarter about +y lands on +x, then moves by t.
  CHECK(std::fabs(q[0] - 3) < 1e-5f && std::fabs(q[1] + 2) < 1e-5f &&
        std::fabs(q[2] - 0.5f) < 1e-5f);
  const a::Affine id = a::compose(m, a::invert(m));
  const float want[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
  for (int e = 0; e < 12; ++e) CHECK(std::fabs(id.m[e] - want[e]) < 1e-5f);
}

TEST(test_model_weights_sum_to_one_and_rest_is_bind) {
  const a::Model m = a::make_test_model();
  CHECK(m.joints.size() == 4 && m.meshes.size() == 1 && m.clips.size() == 2);
  const a::Mesh& mesh = m.meshes[0];
  for (uint32_t v = 0; v < mesh.vertex_count(); ++v) {
    float s = 0;
    for (int k = 0; k < 4; ++k) s += mesh.weights[v * 4 + k];
    CHECK(std::fabs(s - 1) < 1e-6f);
  }
  const Posed p = pose(m, -1, 0);
  for (size_t i = 0; i < p.pos.size(); ++i) CHECK(std::fabs(p.pos[i] - mesh.positions[i]) < 1e-6f);
}

TEST(wave_bends_the_top_right_and_keeps_the_fin_rigid) {
  const a::Model m = a::make_test_model();
  const int wave = a::find_clip(m, "wave");
  CHECK(wave == 0);
  const Posed rest = pose(m, -1, 0), mid = pose(m, wave, 0.5);
  float foot_r[3], foot_m[3], top_r[3], top_m[3];
  ring_centre(rest, 0, foot_r);
  ring_centre(mid, 0, foot_m);
  ring_centre(rest, 10, top_r);
  ring_centre(mid, 10, top_m);
  CHECK(dist(foot_r, foot_m) < 1e-6f);
  CHECK_MSG(top_m[0] > 0.3f, "the limb's top should swing right (+x) at the wave's peak");
  CHECK(top_m[1] < top_r[1]);
  // The fin's 24 vertices (after the tube's 66) keep their distances in every frame.
  const size_t fin0 = 66;
  for (int f = 0; f <= 60; f += 7) {
    const Posed p = pose(m, wave, f / 60.0);
    for (size_t i = fin0; i < fin0 + 24; i += 5) {
      const float d0 = dist(&rest.pos[fin0 * 3], &rest.pos[i * 3]);
      const float d1 = dist(&p.pos[fin0 * 3], &p.pos[i * 3]);
      CHECK(std::fabs(d0 - d1) < 1e-5f);
    }
  }
}

TEST(twist_turns_front_to_right_and_time_wraps_or_holds) {
  const a::Model m = a::make_test_model();
  const int twist = a::find_clip(m, "twist");
  // A quarter turn clockwise from above carries +z to +x and +x to -z: follow vertex 0's
  // normal, which points +x at rest.
  const Posed end = pose(m, twist, 0.5);
  CHECK(std::fabs(end.nrm[0]) < 1e-5f && std::fabs(end.nrm[2] + 1) < 1e-5f);
  // Looped, 0.6 s is 0.1 s into the half-second clip; held, it is the end.
  const Posed looped = pose(m, twist, 0.6, true), at = pose(m, twist, 0.1);
  for (size_t i = 0; i < at.pos.size(); ++i) CHECK(std::fabs(looped.pos[i] - at.pos[i]) < 1e-5f);
  const Posed held = pose(m, twist, 9.0);
  for (size_t i = 0; i < end.pos.size(); ++i) CHECK(held.pos[i] == end.pos[i]);
}

TEST(reach_box_holds_every_frame) {
  const a::Model m = a::make_test_model();
  const a::Box box = a::reach_box(m, m.meshes[0], 0.0f);
  for (size_t c = 0; c < m.clips.size(); ++c) {
    for (uint32_t f = 0; f < m.clips[c].frames; f += 3) {
      const Posed p = pose(m, static_cast<int>(c), f / 60.0);
      for (size_t i = 0; i < p.pos.size(); ++i) {
        CHECK(p.pos[i] >= box.min[i % 3] && p.pos[i] <= box.max[i % 3]);
      }
    }
  }
}

TEST(posed_mesh_keeps_its_golden_bits) {
  const a::Model m = a::make_test_model();
  std::vector<std::string> got;
  for (int clip = -1; clip < 2; ++clip) {
    for (double t : {0.0, 0.1, 0.25, 1.0 / 3.0, 0.5, 0.75, 1.0}) {
      const Posed p = pose(m, clip, t);
      char line[96];
      std::snprintf(line, sizeof line, "%d %016llx %016llx", clip,
                    static_cast<unsigned long long>(ge::test::bits(t)),
                    static_cast<unsigned long long>(fnv(p.nrm, fnv(p.pos))));
      got.push_back(line);
    }
  }
  if (std::getenv("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# clip (-1 rest) time, then FNV-1a 64 of the posed positions' and normals' bits\n";
    for (const auto& l : got) out << l << "\n";
    return;
  }
  std::vector<std::string> want;
  for (const auto& row : ge::test::golden_words(kGolden)) {
    std::string line;
    for (const auto& w : row) line += (line.empty() ? "" : " ") + w;
    want.push_back(line);
  }
  CHECK_MSG(want.size() == got.size(),
            std::string(kGolden) + " has the wrong rows; see this file's header");
  for (size_t i = 0; i < want.size() && i < got.size(); ++i) {
    CHECK_MSG(want[i] == got[i], "pose row " + std::to_string(i) + ": got " + got[i] + ", want " +
                                     want[i] + " (a deliberate move: see the header)");
  }
}
