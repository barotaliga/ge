// THE HELD MODEL: a first-person weapon (or a tool, a pair of hands) in the one form the view-model
// layer draws, whatever file it came from. A glTF (engine/assets) or a game's own morph-frame
// format is converted into it once at load; nothing on a frame's path reads a file format.
//
// - THE FRAME is the view's, in metres: +x to the right, +y up, +z forward (out of the screen
//   into the world), the GRIP at the origin (where the hand holds it), the muzzle along +z. A
//   triangle's front is the side its (b - a) x (c - a) points to, as everywhere in the engine.
// - FRAMES: every vertex has one position and one normal a frame (a morph animation, or one frame
//   for a static model); the triangles, the texture coordinates and the colours are shared.
//   `pose_frames` blends two frames for a frame drawn between them.
// - THE SURFACE is a SKIN (an RGBA8 picture, sRGB, rows from the top, read at `uv`) or, when
//   there is no skin, a linear colour a vertex. A skin's `glow` mask marks texels that light
//   themselves (a sight's dot, an indicator): drawn at their own colour whatever the light.
// - THE MOUNT (`fit_mount`) puts a model authored in any frame into the held frame: the axis its
//   muzzle points along, its scale and the point that becomes the grip.
// - THE MUZZLE: the barrel's tip, where a game stages its muzzle flash, its smoke and its shot's
//   light, given by the vertices it is the centre of (`muzzle_vertices`), so it rides every morph
//   frame (a recoil, a pump) as the barrel does; `find_muzzle` picks the front-most ones.
//   `muzzle_dir` is the way the shot leaves, held frame. `held_to_world` (place.hpp) takes the tip
//   into the world's lens.
// - THE MODEL'S OWN FLAME: `flame` marks the triangles that are the file's muzzle flash (a classic
//   weapon draws its flash as glowing geometry on its firing frames), so a game whose engine
//   draws the flash hides them (`DrawLit::hidden`).
//
// ge:summary The held model: a first-person weapon's frames, skin, colours and grip mount
// ge:keywords viewmodel view-model held-model first-person weapon gun hands morph-frames
// ge:keywords skin glow fullbright emissive uv grip muzzle mount fit_mount held_from_model
// ge:keywords pose_frames held-frame muzzle-tip find_muzzle muzzle_at muzzle-flash flame
// ge:skill render assets
// ge:test tests/viewmodel_held_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "assets/image.hpp"
#include "assets/model.hpp"

namespace ge::viewmodel {

struct HeldFrame {
  std::string name;
  std::vector<float> positions;  // xyz a vertex, m, the held frame
  std::vector<float> normals;    // xyz a vertex, unit
};

struct Skin {
  assets::Image image;        // sRGB, rows from the top (empty: no skin)
  std::vector<uint8_t> glow;  // 1 a texel that lights itself (empty: none do)
};

struct HeldModel {
  std::vector<HeldFrame> frames;  // at least one; every frame the same vertex count
  std::vector<uint32_t> indices;  // three a triangle
  std::vector<float> uv;          // two a vertex (u right, v down, 0..1) when `skin` is set
  std::vector<float> colour;      // linear rgb a vertex when there is no skin
  Skin skin;
  std::vector<uint32_t> muzzle_vertices;  // the barrel's tip is their centre (empty: the origin)
  float muzzle_dir[3] = {0, 0, 1};        // the way the shot leaves, held frame, unit
  std::vector<uint8_t> flame;             // one a triangle: 1 the model's own muzzle flash
  uint32_t vertex_count() const {
    return frames.empty() ? 0 : static_cast<uint32_t>(frames[0].positions.size() / 3);
  }
};

// The box round every frame of a model, held frame.
void held_bounds(const HeldModel& model, float min[3], float max[3]);

// HOW A MODEL SITS IN THE HAND. `muzzle` is the model's own axis the barrel points along (0: +x,
// 1: +y, 2: +z; `muzzle_negative` for the minus side), `up` the axis that is up when held. The
// model is turned so they become +z and +y, scaled so its length along the muzzle is `length_m`
// (0: scaled by `scale` instead), and moved so the grip is at the origin: `grip` is that point as
// shares of the box after the turn (0..1 on x, y and z: 0.5, 0.2, 0.25 is the middle across, low, a
// quarter of the way from the back).
struct Mount {
  int muzzle = 2;
  bool muzzle_negative = false;
  int up = 1;
  bool up_negative = false;
  double length_m = 0;
  double scale = 1;
  double grip[3] = {0.5, 0.2, 0.25};
};

// Every frame of `model` put into the held frame by `mount`, in place, and its muzzle found
// (`find_muzzle`) when it has none.
void fit_mount(HeldModel& model, const Mount& mount);

// The muzzle as the vertices of frame 0 within `share` of its length from its front (+z), those
// nearest the long axis first: at most 16 of them, the model's own flash (`flame`) left out. The
// direction is +z.
void find_muzzle(HeldModel& model, double share = 0.04);

// The barrel's tip on frames `a` and `b` blended by `t`, held frame, m.
void muzzle_at(const HeldModel& model, int a, int b, double t, float out[3]);

// A glTF model (engine frame, metres) as a held model: every mesh at its rest pose, or posed by
// clip `clip` at `frames` evenly spaced times as morph frames, each vertex coloured by its part's
// material (a baked colour atlas is already a part a colour). Not yet mounted.
HeldModel held_from_model(const assets::Model& model, int clip = -1, int frames = 1);

// Frames `a` and `b` blended by `t` (0..1) into `positions` and `normals` (xyz a vertex each,
// sized by the caller); the normals are renormalised.
void pose_frames(const HeldModel& model, int a, int b, double t, std::vector<float>& positions,
                 std::vector<float>& normals);

}  // namespace ge::viewmodel
