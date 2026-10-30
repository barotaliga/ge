// A MODEL: an authored mesh with its skeleton, its materials and its baked clips, in the
// engine's own frame and form (docs/rendering.md § Authored models). A loader
// (assets/gltf.hpp) fills one from a file at load time; nothing reads a file format after
// that, and nothing on a frame's path allocates.
//
// - THE FRAME is the world's (engine/core/math.hpp): y up, +z forward, +x to the right, so a
//   model faces heading 0. Metres. A triangle's front is the side its (b - a) x (c - a)
//   points to, as render/mesh.hpp's.
// - EVERY MESH IS SKINNED. A rigid part (a rotor, a door) is a mesh whose vertices are all
//   bound with weight 1 to the joint of the node that carries it, so one path poses rigid and
//   deforming parts alike and a clip may move either.
// - THE SKELETON is every node of the scene, parents before children (`Joint::parent` is less
//   than the joint's own index), each with its rest transform (local to its parent) and its
//   inverse bind matrix (model space to the joint's space at bind). At most kMaxJoints, so a
//   vertex names its joints in a byte each.
// - A CLIP is baked at kClipRateHz into one local transform a joint a frame, so sampling a
//   clip is two reads and a blend, and its first and last frames are its ends.
// - A MORPH TARGET is an offset a vertex from the mesh at bind (a classic model's frame, a face's
//   expression): a pose adds each target times its weight to the bind before skinning, and a
//   clip carries a weight a target a baked frame for each mesh it moves.
// - AN AFFINE is a 3x4 matrix as three rows (p' = M (x, y, z, 1)): the bone palette's form,
//   three vec4s a joint on the GPU (render/skinning.hpp).
//
// ge:summary An authored model in the engine's form: skinned meshes, skeleton, materials, clips
// ge:keywords assets model mesh skinned-mesh skeleton joint bone inverse-bind material clip
// ge:keywords animation keyframes trs affine palette rigid-part kmaxjoints kcliprate
// ge:keywords to_affine compose invert transform_point transform_vector
// ge:keywords morph-target morph-track blend-shape target-weights
// ge:skill assets
// ge:test tests/assets_pose_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ge::assets {

inline constexpr int kMaxJoints = 256;       // a vertex's joint index is a byte
inline constexpr int kJointsPerVertex = 4;   // the strongest four influences
inline constexpr int kMaxParts = 256;        // a vertex's material is a byte
inline constexpr float kClipRateHz = 60.0f;  // a clip's baked frames a second

// A transform as translation, rotation (a unit quaternion x, y, z, w) and scale, applied
// scale first: p' = t + rotate(r, s * p).
struct Trs {
  float t[3] = {0, 0, 0};
  float r[4] = {0, 0, 0, 1};
  float s[3] = {1, 1, 1};
};

// Three rows of a 3x4 matrix: p' = (row0 . p1, row1 . p1, row2 . p1) with p1 = (x, y, z, 1).
struct Affine {
  float m[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
};

Affine to_affine(const Trs& trs);
// a * b: the transform that applies b, then a.
Affine compose(const Affine& a, const Affine& b);
// The inverse of an invertible affine (a singular one comes back as the identity).
Affine invert(const Affine& a);
void transform_point(const Affine& a, const float p[3], float out[3]);
// The 3x3 half alone (a direction; not normalised).
void transform_vector(const Affine& a, const float v[3], float out[3]);

struct Joint {
  std::string name;
  int32_t parent = -1;  // the parent joint, or -1 for a root
  Trs rest;             // local to the parent
  Affine inverse_bind;  // model space -> this joint's space, at bind
};

struct Material {
  std::string name;
  float rgb[3] = {1, 1, 1};  // the base colour, linear
};

// A mesh's offset from its bind: every vertex's position (m) and normal (a difference of unit
// normals) a pose adds times the target's weight.
struct MorphTarget {
  std::string name;
  std::vector<float> positions;  // xyz a vertex, m
  std::vector<float> normals;    // xyz a vertex
};

// A mesh at bind, in model space. Four influences a vertex, the weights summing to 1.
struct Mesh {
  std::string name;
  std::vector<float> positions;  // xyz a vertex, m
  std::vector<float> normals;    // xyz a vertex, unit
  std::vector<uint8_t> joints;   // kJointsPerVertex a vertex
  std::vector<float> weights;    // kJointsPerVertex a vertex
  std::vector<uint8_t> parts;    // a vertex's material
  std::vector<uint32_t> indices;
  std::vector<MorphTarget> targets;
  std::vector<float> target_weights;  // a target each: the weights when no clip moves them
  uint32_t vertex_count() const { return static_cast<uint32_t>(parts.size()); }
};

// One mesh's target weights at every baked frame of a clip: weights[frame * targets + target].
struct MorphTrack {
  int32_t mesh = -1;
  std::vector<float> weights;
};

// Every joint's local transform at every baked frame: samples[frame * joints + joint]; and the
// target weights of each mesh the clip moves.
struct Clip {
  std::string name;
  uint32_t frames = 0;  // at kClipRateHz; the last is the clip's end
  std::vector<Trs> samples;
  std::vector<MorphTrack> morphs;
  double duration_s() const { return frames > 1 ? (frames - 1) / double{kClipRateHz} : 0.0; }
};

struct Model {
  std::vector<Joint> joints;
  std::vector<Mesh> meshes;
  std::vector<Material> materials;
  std::vector<Clip> clips;
};

// The index of the joint, mesh or clip of that name, or -1.
int find_joint(const Model& model, std::string_view name);
int find_mesh(const Model& model, std::string_view name);
int find_clip(const Model& model, std::string_view name);

}  // namespace ge::assets
