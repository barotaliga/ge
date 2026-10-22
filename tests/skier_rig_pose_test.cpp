// plugins/skier/app/rig_pose.hpp held to the three faults the anim lab found in the placeholder
// figure, over the `turns` ride scenario at 120 Hz:
//
//   * NO STRETCH: every bone of the rig keeps its bind length in every frame.
//   * ON THE SNOW, NEVER IN IT: the skis' tips and tails, the boots' pivot and the posed model's
//     ski vertices stay at or over y = 0, and the pole tips trail at their fixed height.
//   * NO POP: the lean is lagged twice, so a joint's speed changes smoothly even when the lean
//     the physics hands over flips in one step; the largest change of a joint's speed in one
//     step stays small.
//   * THE MODEL: `rig_locals` gives the skier model locals that pose every rig joint back where
//     the poser put it.
//   * THE REACH: the posed model stays inside `pose_reach` through the turns, a jump, a hard
//     landing and a wipeout on a trunk, so the GPU's skinning never clamps it.
//
// ge:summary Tests for the skier poser: bone lengths, the snow, a lean flip, the model's locals
// ge:keywords test skier rig pose poser ik stretch clip pop snow pole model locals
// ge:skill skier assets testing
#include <cmath>
#include <string>
#include <vector>

#include "assets/gltf.hpp"
#include "assets/pose.hpp"
#include "check.hpp"
#include "skier/app/rig.hpp"
#include "skier/app/rig_pose.hpp"
#include "skier/scenarios.hpp"

namespace sk = ge::skier;
namespace a = ge::assets;
using ge::core::Vec3;

namespace {

constexpr double kHz = 120;

double dist(Vec3 p, Vec3 q) {
  return std::sqrt((p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y) +
                   (p.z - q.z) * (p.z - q.z));
}

// The poses of ride scenario `name`, one a step.
std::vector<sk::RigPose> ride(const char* name) {
  const sk::Scenario* sc = sk::find_scenario(name);
  CHECK(sc != nullptr);
  std::vector<sk::RigPose> poses;
  if (!sc) return poses;
  const ge::mountain::mapgen::Level level = sk::scenario_level(*sc);
  const sk::TreeIndex trees = sk::build_tree_index(level);
  const sk::World world{&level, &trees};
  sk::SkierState s = sk::stand_skier(world, sc->x, sc->z, sc->heading, sc->speed, sc->lift);
  sk::SkierEvents ev;
  sk::RigPoser poser;
  const auto steps = static_cast<int>(sc->seconds * kHz);
  poses.resize(static_cast<size_t>(steps));
  for (int i = 0; i < steps; i++) {
    poser.pose(s, 1 / kHz, poses[static_cast<size_t>(i)]);
    sk::step_skier(s, sc->input(i / kHz, s), world, sk::kReferenceSkis, 1 / kHz, ev);
  }
  return poses;
}

const std::vector<sk::RigPose>& turns() {
  static const std::vector<sk::RigPose> k = ride("turns");
  return k;
}

int J(const char* name) { return sk::rig_joint(name); }

}  // namespace

TEST(every_bone_keeps_its_length_through_the_turns) {
  const auto& rig = sk::rig_joints();
  double worst = 0;
  std::string where;
  for (const sk::RigPose& p : turns()) {
    for (size_t j = 0; j < rig.size(); j++) {
      const int parent = rig[j].parent;
      // The pelvis is placed, not hung: root to pelvis is no bone.
      if (parent < 0 || static_cast<int>(j) == J("pelvis")) continue;
      const double bind = dist(rig[j].at, rig[static_cast<size_t>(parent)].at);
      const double posed = dist(p.at[j], p.at[static_cast<size_t>(parent)]);
      if (std::abs(posed - bind) > worst) {
        worst = std::abs(posed - bind);
        where = rig[j].name;
      }
    }
  }
  CHECK_MSG(worst < 1e-6, "the bone to " + where + " stretches by " + std::to_string(worst) +
                              " m; next: make lab LAB=anim ARGS=--scenario=turns");
}

TEST(the_skis_and_the_poles_stay_on_the_snow) {
  double lowest = 1e9, pole_err = 0;
  for (const sk::RigPose& p : turns()) {
    for (const char* j : {"ski_tip_l", "ski_tip_r", "ski_tail_l", "ski_tail_r"}) {
      lowest = std::fmin(lowest, p.at[static_cast<size_t>(J(j))].y);
    }
    for (const char* j : {"pole_tip_l", "pole_tip_r"}) {
      pole_err = std::fmax(pole_err, std::abs(p.at[static_cast<size_t>(J(j))].y - 0.06));
    }
  }
  CHECK_MSG(lowest > -1e-9, "a ski's tip or tail goes " + std::to_string(-lowest) +
                                " m into the snow; next: make lab LAB=anim ARGS=--scenario=turns");
  CHECK_MSG(pole_err < 1e-9,
            "a pole tip strays " + std::to_string(pole_err) + " m off its height over the snow");
}

TEST(a_lean_flipped_in_one_step_moves_the_body_smoothly) {
  sk::SkierState s;
  s.speed = 12;
  s.lean = 0.6;
  s.edge = 0.6;
  sk::RigPoser poser;
  std::vector<sk::RigPose> p(240);
  for (size_t i = 0; i < p.size(); i++) {
    if (i == 120) s.lean = -0.6, s.edge = -0.6;
    poser.pose(s, 1 / kHz, p[i]);
  }
  // The largest change of a joint's step (a second difference, m a step a step) at the flip
  // against the largest anywhere else: one lag alone gives the head a kink there.
  const int head = J("head");
  double at_flip = 0, step = 0;
  for (size_t i = 2; i < p.size(); i++) {
    const Vec3 a0 = p[i - 2].at[static_cast<size_t>(head)],
               a1 = p[i - 1].at[static_cast<size_t>(head)], a2 = p[i].at[static_cast<size_t>(head)];
    const double acc = std::sqrt((a2.x - 2 * a1.x + a0.x) * (a2.x - 2 * a1.x + a0.x) +
                                 (a2.y - 2 * a1.y + a0.y) * (a2.y - 2 * a1.y + a0.y));
    if (i >= 120 && i < 124) at_flip = std::fmax(at_flip, acc);
    step = std::fmax(step, dist(a2, a1));
  }
  CHECK_MSG(step > 1e-3, "the head never moved: the flip is not posed");
  // Through one lag the head's speed jumps from nothing to its most in the step after the flip
  // (a change of speed as large as the largest step: a pop). Through two its speed starts from
  // zero and builds over several steps, so the change in any one is about a quarter of it.
  CHECK_MSG(at_flip < 0.5 * step, "the head's step changes by " + std::to_string(at_flip) +
                                      " m at the flip against a largest step of " +
                                      std::to_string(step) + " m: a pop");
}

const a::LoadResult& skier_model() {
  static const a::LoadResult k =
      a::load_gltf_file(std::string(GE_SOURCE_DIR) + "/plugins/skier/assets/skier/skier.glb");
  return k;
}

TEST(the_models_locals_pose_every_rig_joint_where_the_poser_put_it) {
  const a::LoadResult& r = skier_model();
  CHECK_MSG(r.ok, r.error);
  if (!r.ok) return;
  const a::Model& m = r.model;
  const sk::RigBinding binding = sk::bind_rig(m);
  CHECK(binding.complete);
  std::vector<a::Trs> local(m.joints.size());
  std::vector<a::Affine> global(m.joints.size()), palette(m.joints.size());
  const a::Mesh& mesh = m.meshes[0];
  const size_t n = size_t{3} * mesh.vertex_count();
  std::vector<float> positions(n), normals(n);
  double worst = 0, lowest_ski = 1e9;
  const auto& poses = turns();
  for (size_t f = 0; f < poses.size(); f += 24) {
    sk::rig_locals(m, binding, poses[f], local);
    a::pose_globals(m, local, global);
    for (size_t j = 0; j < m.joints.size(); j++) {
      const int rj = binding.rig_of[j];
      if (rj < 0) continue;
      const float* g = global[j].m;
      worst = std::fmax(worst, dist(Vec3{g[3], g[7], g[11]}, poses[f].at[static_cast<size_t>(rj)]));
    }
    a::skin_palette(m, global, palette);
    a::skin_mesh(mesh, palette, positions, normals);
    for (size_t v = 0; v < mesh.vertex_count(); v++) {
      // The skis ahead of and behind the boots: nothing else of him reaches there.
      if (std::abs(mesh.positions[v * 3 + 2] - sk::kMountBack) > 0.3f) {
        lowest_ski = std::fmin(lowest_ski, positions[v * 3 + 1]);
      }
    }
  }
  CHECK_MSG(worst < 1e-4, "a model joint sits " + std::to_string(worst) +
                              " m off the poser's place; next: check bind_rig's names");
  CHECK_MSG(lowest_ski > -2e-3,
            "the posed skis' base goes " + std::to_string(-lowest_ski) +
                " m into the snow; next: make lab LAB=anim ARGS=--scenario=turns");
}

TEST(the_posed_model_stays_inside_its_reach) {
  const a::LoadResult& r = skier_model();
  CHECK_MSG(r.ok, r.error);
  if (!r.ok) return;
  const a::Model& m = r.model;
  const sk::RigBinding binding = sk::bind_rig(m);
  const a::Box reach = sk::pose_reach();
  std::vector<a::Trs> local(m.joints.size());
  std::vector<a::Affine> global(m.joints.size()), palette(m.joints.size());
  const a::Mesh& mesh = m.meshes[0];
  const size_t n = size_t{3} * mesh.vertex_count();
  std::vector<float> positions(n), normals(n);
  for (const char* name : {"turns", "kicker", "drop-harsh", "tree"}) {
    // The room left to the nearest wall, m: negative is outside.
    double room = 1e9;
    for (const sk::RigPose& p : ride(name)) {
      sk::rig_locals(m, binding, p, local);
      a::pose_globals(m, local, global);
      a::skin_palette(m, global, palette);
      a::skin_mesh(mesh, palette, positions, normals);
      for (size_t v = 0; v < n; v += 3) {
        for (size_t k = 0; k < 3; k++) {
          room = std::fmin(room, positions[v + k] - reach.min[k]);
          room = std::fmin(room, reach.max[k] - positions[v + k]);
        }
      }
    }
    CHECK_MSG(room > 0.02, std::string("the posed model comes within ") + std::to_string(room) +
                               " m of pose_reach's wall in `" + name +
                               "`; next: widen pose_reach in skier/app/rig_pose.cpp");
  }
}
