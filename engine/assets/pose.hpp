// POSING A MODEL: a clip sampled at a time into every joint's local transform, the locals
// chained into model space, the bone palette the skinning reads, and the CPU skinning that is
// the GPU's reference (render/skinning.hpp holds its compute pass to `skin_mesh`).
//
// The order a frame poses a figure in: `sample_clip` (or `rest_pose`, or the game's own
// locals) -> `pose_globals` -> `skin_palette` -> the GPU (or `skin_mesh` on the CPU). A mesh
// with morph targets first takes its weights (`sample_weights`) and adds its targets to its
// bind (`morph_mesh`), and that morphed bind is what is skinned. Every span is the caller's,
// sized to the model's joints (a mesh's targets, its vertices), so posing allocates nothing.
//
// Sampling blends the two baked frames either side of the time: the translations and scales
// linearly, the rotations by a normalised blend on the shorter arc. At kClipRateHz the frames
// are close enough that this is the arc's own rotation to a fraction of a milliradian, and it
// needs no transcendental.
//
// ge:summary Posing a model: clip sampling, joints to model space, the palette, CPU skinning
// ge:keywords assets pose animation sample_clip rest_pose pose_globals skin_palette skin_mesh
// ge:keywords skinning bone-palette linear-blend-skinning nlerp loop clamp reach reach_box
// ge:keywords bounding-box joint-angle morph-target sample_weights morph_mesh blend-shape
// ge:skill assets
// ge:test tests/assets_pose_test.cpp
#pragma once

#include <span>

#include "assets/model.hpp"

namespace ge::assets {

// Every joint at its rest transform.
void rest_pose(const Model& model, std::span<Trs> local);
// Every joint at time `t_s` of `clip`: wrapped into the clip with `loop`, else held at its
// ends.
void sample_clip(const Model& model, const Clip& clip, double t_s, bool loop, std::span<Trs> local);
// Mesh `mesh`'s target weights at time `t_s` of `clip` (wrapped or held as `sample_clip`
// does), blended between the two baked frames either side: the clip's track for the mesh, or
// the mesh's own weights when the clip has none or `clip` is null.
void sample_weights(const Model& model, const Clip* clip, size_t mesh, double t_s, bool loop,
                    std::span<float> weights);
// `mesh` at bind with each target added times its weight: positions, and the normals made unit
// again, three floats a vertex each.
void morph_mesh(const Mesh& mesh, std::span<const float> weights, std::span<float> positions,
                std::span<float> normals);
// The locals chained to model space, parents first.
void pose_globals(const Model& model, std::span<const Trs> local, std::span<Affine> global);
// The palette: each joint's model-space transform times its inverse bind, so a vertex at bind
// lands where the pose puts it.
void skin_palette(const Model& model, std::span<const Affine> global, std::span<Affine> palette);
// `mesh` skinned by `palette` (linear blend of its four influences): positions and unit normals
// into `positions` and `normals`, three floats a vertex each.
void skin_mesh(const Mesh& mesh, std::span<const Affine> palette, std::span<float> positions,
               std::span<float> normals);
// The same from a bind of the caller's (a `morph_mesh` result) in place of the mesh's own.
void skin_mesh(const Mesh& mesh, std::span<const Affine> palette, std::span<const float> bind,
               std::span<const float> bind_normals, std::span<float> positions,
               std::span<float> normals);

struct Box {
  float min[3] = {0, 0, 0};
  float max[3] = {0, 0, 0};
};
// THE REACH of a mesh: the box round it at bind and at every baked frame of every clip (its
// targets weighted as the clip weights them), grown by `margin` of its size on each side. A
// skinned mesh is quantised into this box on the GPU, so a pose outside it is clamped to its
// walls.
Box reach_box(const Model& model, const Mesh& mesh, float margin = 0.05f);

// The turn a joint's local rotation makes from its rest as a rotation vector (its axis times
// its angle in radians, the angle 0..pi) into `out`, and the angle returned: what the animation
// lab graphs a clip by. The vector passes through zero as a swing passes through rest, so its
// second difference is a joint's angular acceleration with no cusp at rest.
double turn_from_rest(const Joint& joint, const Trs& local, double out[3]);

}  // namespace ge::assets
