// plugins/skier/app/rig.hpp and the skier model built on it, held by the engine's own checks:
//
//   * THE RIG: parents before children, every left joint mirrored by its right, every limb's
//     bone along -y at bind (rig.hpp's promise to a builder), the skis' tips and tails a ski's
//     length apart and the boot `kMountBack` behind the middle; the outline is the waist under
//     the boot and widens to the tips.
//   * THE MODEL: plugins/skier/assets/skier/skier.glb loads, has every rig joint by name at its
//     bind position to a tenth of a millimetre, every weight sums to one, and its skis' base
//     lies on the snow: the model and the code agree on the skeleton.
//
// ge:summary Tests for the skier's rig and the skier model built on it
// ge:keywords test skier rig skeleton bind mirror model glb joints weights skis
// ge:skill skier assets testing
#include <cmath>
#include <string>

#include "assets/gltf.hpp"
#include "assets/pose.hpp"
#include "check.hpp"
#include "skier/app/rig.hpp"

namespace sk = ge::skier;
namespace a = ge::assets;

TEST(the_rig_is_a_tree_mirrored_left_to_right_standing_on_its_skis) {
  const auto& joints = sk::rig_joints();
  CHECK(joints[0].parent == -1);
  for (size_t i = 1; i < joints.size(); i++) {
    const sk::RigJoint& j = joints[i];
    CHECK_MSG(j.parent >= 0 && j.parent < static_cast<int>(i),
              std::string(j.name) + ": its parent must come before it");
    const std::string name = j.name;
    if (name.ends_with("_l")) {
      const int r = sk::rig_joint(name.substr(0, name.size() - 2) + "_r");
      CHECK_MSG(r >= 0, name + " has no right twin");
      const sk::RigJoint& twin = joints[static_cast<size_t>(r)];
      CHECK(twin.at.x == -j.at.x && twin.at.y == j.at.y && twin.at.z == j.at.z);
    }
  }
  for (const char* limb : {"hip_l", "knee_l", "shoulder_l", "elbow_l", "pole_l"}) {
    const sk::RigJoint& j = joints[static_cast<size_t>(sk::rig_joint(limb))];
    const sk::RigJoint* child = nullptr;
    for (const sk::RigJoint& c : joints) {
      if (c.parent == sk::rig_joint(limb) && !child) child = &c;
    }
    CHECK_MSG(child && child->at.y < j.at.y && std::abs(child->at.z - j.at.z) < 1e-12,
              std::string(limb) + "'s bone must run down -y at bind");
  }
  const sk::RigJoint& tip = joints[static_cast<size_t>(sk::rig_joint("ski_tip_l"))];
  const sk::RigJoint& tail = joints[static_cast<size_t>(sk::rig_joint("ski_tail_l"))];
  CHECK(std::abs(tip.at.z - tail.at.z - sk::kReferenceSkis.length) < 1e-12);
  CHECK(std::abs((tip.at.z + tail.at.z) / 2 - sk::kMountBack) < 1e-12);
  CHECK(tip.at.y == 0 && tail.at.y == 0);
  const double waist = sk::ski_half_width(sk::kReferenceSkis, sk::kMountBack);
  CHECK(std::abs(waist - sk::kReferenceSkis.waist / 2) < 1e-12);
  CHECK(sk::ski_half_width(sk::kReferenceSkis, tip.at.z) > waist + 0.015);
}

TEST(the_skier_model_is_built_on_the_rig) {
  const a::LoadResult r =
      a::load_gltf_file(std::string(GE_SOURCE_DIR) + "/plugins/skier/assets/skier/skier.glb");
  CHECK_MSG(r.ok, r.error);
  if (!r.ok) return;
  const a::Model& m = r.model;
  std::vector<a::Trs> local(m.joints.size());
  std::vector<a::Affine> global(m.joints.size());
  a::rest_pose(m, local);
  a::pose_globals(m, local, global);
  for (const sk::RigJoint& j : sk::rig_joints()) {
    const int at = a::find_joint(m, j.name);
    CHECK_MSG(at >= 0,
              std::string("skier.glb has no joint ") + j.name + "; next: ge asset build skier");
    if (at < 0) continue;
    const float* g = global[static_cast<size_t>(at)].m;
    const double err = std::abs(g[3] - j.at.x) + std::abs(g[7] - j.at.y) + std::abs(g[11] - j.at.z);
    CHECK_MSG(err < 1e-4, std::string(j.name) + " sits " + std::to_string(err) +
                              " m off the rig's bind; next: ge asset build skier");
  }
  CHECK(m.meshes.size() == 1);
  const a::Mesh& mesh = m.meshes[0];
  float lowest = 1e9f;  // under the skis' front halves (nothing else reaches past the boots)
  for (size_t v = 0; v < mesh.vertex_count(); v++) {
    float sum = 0;
    for (size_t k = 0; k < a::kJointsPerVertex; k++)
      sum += mesh.weights[v * a::kJointsPerVertex + k];
    CHECK_MSG(std::abs(sum - 1) < 1e-3f, "a vertex's weights sum to " + std::to_string(sum));
    if (mesh.positions[v * 3 + 2] > 0.3f) lowest = std::min(lowest, mesh.positions[v * 3 + 1]);
  }
  CHECK_MSG(std::abs(lowest) < 1e-4f,
            "the skis' base lies " + std::to_string(lowest) + " m off the snow at bind");
}
