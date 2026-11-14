// FALL LINE'S GPU PICTURES ON THE PLAY CONTRACT (engine/play/game.hpp § THE GPU'S PICTURES): the
// renderer `ge shot --gpu` and `ge record --gpu` draw a run with. It opens a headless device (the
// software adapter in the GPU-less container), makes the game's frame (frame.hpp) on the first
// picture, for that run's map, and draws each picture the play contract asks for from the run's
// own GameState: every racer a figure laying his tracks and throwing his spray, the sky at the
// map's hour, the clock at the run's.
//
// THE VIEWS: `chase` (behind and over the player, the camera a run is watched from), `high` (the
// same from further back and higher: the line through the gates ahead), and the map's named
// views (views.hpp: start, piste, lookback, vista, aerial), still cameras the field skis through.
//
// THE MODES, each the same frame read another way:
// - `lit`: the picture as the game shows it.
// - `wireframe`: the meshes' edges over black (frame.hpp's `draw_wire`): the ground's clipmap
//   tiles banded by distance (near magenta, through blue and cyan to green, grey from 2 km), the
//   trees and the figures orange, hidden lines hidden.
// - `depth`: how far each pixel is, from the frame's depth: near white, 4 km black on a log
//   scale, the sky black.
// - `overlay`: the lit picture with the race drawn over it on the CPU: the piste's line and every
//   gate across the piste with its number where the frame's depth shows them, and each racer's
//   velocity over the next half second with his number and speed.
//
// A cut (the first picture) draws `kSettle` frames of the still run so TAA's history fills; every
// picture after draws one, so a recording's frames carry the motion and the tracks from one to
// the next.
//
// ge:summary Fall Line's GPU renderer for ge shot --gpu and ge record --gpu: views and modes
// ge:keywords play renderer gpu-shot gpu-recording chase high lit wireframe depth overlay
// ge:keywords renderer-spec k-renderer headless capture readback
// ge:skill render agent-interface
#pragma once

#include <array>
#include <optional>
#include <string_view>

#include "play/game.hpp"
#include "render/view.hpp"

namespace fall_line::sim {
struct GameState;
}

namespace fall_line::app {

// The frames a cut draws before its picture is read.
inline constexpr int kSettle = 8;

// The renderer: `GE_RENDERER(fall_line::app::kRenderer)` in the play executable registers it.
extern const ge::play::RendererSpec kRenderer;

// The camera of view `name` (a rung of the ladder, `chase` or `high`, or one of the map's named
// views) on run state `s`, at the map's hour and the run's clock, into `view`; a rung also says the
// point it frames (`subject`), a view of the mountain none. False when no view has that name. The
// web viewer (web/viewer.cpp) watches a run through it as the renderer pictures one.
bool run_camera(const sim::GameState& s, std::string_view name, ge::render::FrameView& view,
                std::optional<std::array<double, 3>>& subject);

}  // namespace fall_line::app
