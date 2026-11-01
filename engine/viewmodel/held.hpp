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
//   into the world's lens. A gun of SEVERAL BARRELS lists each one's tip vertices (`barrels`) and
//   which barrel each frame of its firing animation fires from (`frame_barrel`): a pair that
//   alternates, a cluster that turns; `muzzle_at` takes the barrel.
// - CLIPS: a model's animations (a pump, a swing, barrels turning) baked as runs of frames after
//   the rest frame (`held_with_clips`), each run named in `clips` with its rate, so a game plays
//   one by time (`held_clip_frames`) and the frames blend as any morph frames do.
// - THE MODEL'S OWN FLAME: `flame` marks the triangles that are the file's muzzle flash (a classic
//   weapon draws its flash as glowing geometry on its firing frames), so a game whose engine
//   draws the flash hides them (`DrawLit::hidden`).
// - THE HANDS: `hand` marks the triangles of the hands and arms a model is built holding
//   (`mark_hands`: every mesh whose name carries a marker), so a player who wants the weapon alone
//   sees it without them (`hidden_triangles`).
//
// ge:summary The held model: a first-person weapon's frames, skin, colours and grip mount
// ge:keywords viewmodel view-model held-model first-person weapon gun hands morph-frames
// ge:keywords skin glow fullbright emissive uv grip muzzle mount fit_mount held_from_model
// ge:keywords pose_frames held-frame muzzle-tip find_muzzle muzzle_at muzzle-flash flame
// ge:keywords kept-texture baked-texture emissive-glow held-clip held_with_clips animation
// ge:keywords find_held_clip held_clip_frames fire-animation bounds_frame
// ge:keywords barrels multi-barrel alternating-barrels frame_barrel barrel_of_frame
// ge:keywords hide-hands mark_hands hidden_triangles hands-off arms
// ge:skill render assets
// ge:test tests/viewmodel_held_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
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

// A clip baked into a held model's frames: `count` frames from `first`, `fps` apart, the first at
// the clip's start and the last at its end.
struct HeldClip {
  std::string name;
  int first = 0;
  int count = 0;
  double fps = 30;
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
  std::vector<uint8_t> hand;              // one a triangle: 1 the hands and arms holding it
  // More than one barrel: each one's tip vertices (empty: the one muzzle, `muzzle_vertices`), and
  // the barrel each frame fires from, one a frame (-1: none; empty: barrel 0 on every frame).
  std::vector<std::vector<uint32_t>> barrels;
  std::vector<int16_t> frame_barrel;
  std::vector<HeldClip> clips;  // the model's animations as runs of `frames` (empty: none)
  uint32_t vertex_count() const {
    return frames.empty() ? 0 : static_cast<uint32_t>(frames[0].positions.size() / 3);
  }
};

// The box round every frame of a model (or frame `frame` alone when it is one), held frame.
void held_bounds(const HeldModel& model, float min[3], float max[3], int frame = -1);

// HOW A MODEL SITS IN THE HAND. `muzzle` is the model's own axis the barrel points along (0: +x,
// 1: +y, 2: +z; `muzzle_negative` for the minus side), `up` the axis that is up when held. The
// model is turned so they become +z and +y, scaled so its length along the muzzle is `length_m`
// (0: scaled by `scale` instead), and moved so the grip is at the origin: `grip` is that point as
// shares of the box after the turn (0..1 on x, y and z: 0.5, 0.2, 0.25 is the middle across, low, a
// quarter of the way from the back), or the model's own origin when `grip_at_origin` (a model built
// at its true size round the hand that holds it, scaled by `scale`).
struct Mount {
  int muzzle = 2;
  bool muzzle_negative = false;
  int up = 1;
  bool up_negative = false;
  double length_m = 0;
  double scale = 1;
  double grip[3] = {0.5, 0.2, 0.25};
  int bounds_frame = -1;  // the box is frame n's (a rest frame a swing leaves); -1 every frame's
  bool grip_at_origin = false;  // the model's own origin is the grip (a model built round its hand)
};

// Every frame of `model` put into the held frame by `mount`, in place, and its muzzle found
// (`find_muzzle`) when it has none. `measured_on`, when given, is the model the box (the length
// and the grip's shares) is read off instead: the same model without the parts that are not the
// weapon (the hands and arms holding it, which reach back toward the eye). Its frames are turned
// on a copy; it is not changed.
void fit_mount(HeldModel& model, const Mount& mount, const HeldModel* measured_on = nullptr);

// The muzzle as the vertices of frame 0 within `share` of its length from its front (+z), those
// nearest the long axis first: at most 16 of them, the model's own flash (`flame`) left out. The
// direction is +z.
void find_muzzle(HeldModel& model, double share = 0.04);

// The barrel's tip on frames `a` and `b` blended by `t`, held frame, m: barrel `barrel` of
// `barrels` (-1, or a model with none: the one muzzle).
void muzzle_at(const HeldModel& model, int a, int b, double t, float out[3], int barrel = -1);
// The barrel frame `frame` fires from (`frame_barrel`), or -1 (the one muzzle) when it names none.
int barrel_of_frame(const HeldModel& model, int frame);

// A glTF model (engine frame, metres) as a held model: every mesh at its rest pose, or posed by
// clip `clip` at `frames` evenly spaced times as morph frames, each vertex coloured by its part's
// material (a baked colour atlas is already a part a colour). A model loaded with its textures
// kept (assets::LoadOptions::keep_textures) draws its first textured material's picture as the
// skin at every vertex's coordinates, its glow picture (the file's emissive texture) marking the
// texels at least half bright as glowing. Not yet mounted.
HeldModel held_from_model(const assets::Model& model, int clip = -1, int frames = 1);

// A glTF model as a held model with its animations: frame 0 the rest pose, then every clip of the
// model baked at `fps` (its start and end included) as frames named "<clip>.<i>", each listed in
// `clips`. A model with no clip is held_from_model's one rest frame.
HeldModel held_with_clips(const assets::Model& model, double fps);

// Marks `model`'s triangles that come from a mesh of `source` (the glTF it was made from by
// held_from_model or held_with_clips: the meshes one after another) whose name contains `marker`
// as the hands (`HeldModel::hand`). False when none does (`hand` left empty).
bool mark_hands(HeldModel& model, const assets::Model& source, std::string_view marker);

// One flag a triangle of `model` for `DrawLit::hidden`: nonzero where it is the model's own flame
// and `flame` is set, or its hands and `hands` is set. Empty when nothing is hidden.
void hidden_triangles(const HeldModel& model, bool flame, bool hands, std::vector<uint8_t>& out);

// The clip named `name`, or -1.
int find_held_clip(const HeldModel& model, std::string_view name);

// The frames to draw `t_s` s into clip `clip`: `a` and `b` blended by `t`. Past its end it holds
// the last frame, or wraps when `loop`; with no such clip, the rest frame (0, 0, 0).
void held_clip_frames(const HeldModel& model, int clip, double t_s, bool loop, int& a, int& b,
                      double& t);

// Frames `a` and `b` blended by `t` (0..1) into `positions` and `normals` (xyz a vertex each,
// sized by the caller); the normals are renormalised.
void pose_frames(const HeldModel& model, int a, int b, double t, std::vector<float>& positions,
                 std::vector<float>& normals);

}  // namespace ge::viewmodel
