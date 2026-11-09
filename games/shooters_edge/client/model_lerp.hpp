// THE MODELS DRAWN BETWEEN THEIR FRAMES, so a figure, a monster, an item or a missile moves
// smoothly at any picture rate instead of in the steps the game moves it in. Presentation only: it
// rewrites the window's models (`LerpModel`, each a world_models.hpp `Placed` as the GPU-free
// part needs it: world_fx.cpp copies them in and out) each picture and writes nothing back to the
// game.
//
// TWO KINDS OF STEP a picture would otherwise show:
//
// - THE ANIMATION (`r_lerpmodels`, every path: the listen server, a played demo, a joined server):
//   a classic program steps a model's frame ten times a second, so a figure's limbs jump from pose
//   to pose. When an entity's `frame` changes, it is drawn from the frame it left to the new one
//   over the time its frames last (`ModelTrack::span`: the gap between its last two changes when
//   it is `kLeastFrameSpan`..`kMostFrameSpan`, else the last such gap, at first `kFrameSpan`: a
//   program's tenth of a second falls on 7 or 8 of a 72 Hz server's frames), the GPU blending the
//   two frames' vertices (render/morph.hpp's `MorphPose::blend`). A looping group (a torch's flame)
//   flows between its own frames too (`LerpModel::lerp`). Measured on the picture's clock (`t`), so
//   it holds for any source.
// - THE PLACE (`r_lerpmove`, the listen server only: a demo and a joined server are already drawn
//   between their frames, demo_lerp.hpp and net/classic_remote.hpp): the server runs 72 or 77
//   frames a second and the picture 60 or 144, so a moving thing's place falls unevenly on the
//   pictures. Each is drawn ONE SERVER FRAME BEHIND, between the last two states the window saw
//   (`ModelLerpClock`: the state's time and the picture's on the server's clock), its angles the
//   shortest way round. A thing that is not a player and stands still between moves (a walking
//   monster, which the program moves a stride at a time, ten a second) is drawn over the time
//   since its last move instead (the same rule as a frame's time), from where it is drawn now.
//
// | Not drawn between | Why |
// | --- | --- |
// | an entity first seen, or seen again after a picture without it | nothing to come from |
// | another model on the entity, or its warp (a spawn, a teleport) | a new thing, not a move |
// | a move over `kSnapUnits` in one state | a teleport, a respawn |
// | two states over `kMostGap` apart | a stall: the newest as read |
// | the picture's clock running back (a seek), the state's clock running back (a new map) | a cut |
//
// ge:summary Models drawn between their frames: animation blended, live places one frame behind
// ge:keywords model-lerp lerp_models r_lerpmodels r_lerpmove interpolation frame-lerp jittery
// ge:keywords animation-blend vertex-lerp smooth-animation monster-steps step-lerp one-tic-behind
// ge:keywords model_lerp model-track lerp-frames lerp-moves frame_from frame_f
// ge:skill shooter render
// ge:test tests/shooters_edge_model_lerp_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <unordered_map>

#include "shooter/vec.hpp"

namespace shooters_edge::client {

inline constexpr double kFrameSpan = 0.1;        // a classic frame's time, s: a blend's at first
inline constexpr double kLeastFrameSpan = 0.02;  // the least a blend or a stride takes, s
inline constexpr double kMostFrameSpan = 0.15;   // the most; a longer gap was a pause, not a pace
inline constexpr float kSnapUnits = 128;         // a move farther in one state: not drawn between
inline constexpr double kMostGap = 0.25;         // states further apart: not drawn between, s

// The switches (r_lerpmodels, r_lerpmove): what is drawn between.
struct ModelLerpSwitches {
  bool frames = false, moves = false;
};

// The listen server's clock for the places (`moves` false: the places are already drawn between,
// or not to be). `state_s` is the server's time of the newest state, `drawn_s` the moment the
// picture shows on the same clock, one server frame behind the state's run time.
struct ModelLerpClock {
  bool moves = false;
  double state_s = 0, drawn_s = 0;
};

namespace sh = ge::shooter;

// One model as the lerp reads and writes it.
struct LerpModel {
  int entity = 0;  // a static: -1 - its index; 0: an effect's own, left as it is
  int modelindex = 0, frame = 0;
  bool warping = false;  // in its warp (a spawn, a teleport): a new thing
  sh::Vec3f origin, angles;
  // Written: the frame group it is leaving (-1: none), the share of the way to `frame` (1: there),
  // and whether a looping group flows between its own frames.
  int frame_from = -1;
  float frame_f = 1;
  bool lerp = false;
};

struct ModelTrack {
  int modelindex = 0;
  uint32_t seen = 0;  // the last call's count it was in
  // The animation (the picture's clock).
  int frame = 0, from = -1;  // the frame group now and the one it left (-1: none)
  double changed = -1;       // when `frame` came, s
  double span = kFrameSpan;  // what its blend takes, s
  // The place (the server's clock): the last two states and when they were.
  sh::Vec3f origin, angles, prev_origin, prev_angles;
  double now_s = -1, prev_s = -1;
  double moved_s = -1;  // the state it last moved in
  // A still thing's move drawn over the time since the last: from where it was drawn.
  bool stepping = false;
  sh::Vec3f step_origin, step_angles;
  double step_at = 0, step_span = kFrameSpan;
};

struct ModelLerp {
  std::unordered_map<int, ModelTrack> tracks;  // by entity (a static: -1 - its index)
  uint32_t calls = 0;
  double last_t = -1, last_state = -1;
};

// `placed` (this picture's, as the game holds them) rewritten to be drawn between frames at the
// picture's time `t` s: each one's `frame_from`, `frame_f` and `lerp` under `on.frames`, and, under
// `on.moves` with `clock.moves`, its origin and angles. Entity 0 is left as it is.
void lerp_models(ModelLerp& l, std::span<LerpModel> placed, double t, const ModelLerpClock& clock,
                 const ModelLerpSwitches& on);

}  // namespace shooters_edge::client
