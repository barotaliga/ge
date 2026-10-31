// FALL LINE'S FOG (docs/rendering.md § Fall Line's fog): the map's mists in the frame, dealt by its
// air at the sky's hour (mountain/app/mist_look.hpp): the sea of fog in the valleys under a clear
// night's inversion, the ground mist round the eye, the cloud on the summit; burning off as the
// morning goes on and drifting on the map's wind.
//
// A caller that hands the frame fog of its own (a lab's `FrameScene::fog`, or a `FrameView::valley`
// set) draws only its own, so the scene it measures is the scene it placed. `r_fog 0` draws none;
// `r_fog_quality` sizes the froxels (`fog_settings`).
//
// ge:summary Fall Line's fog: the map's mists dealt into the frame at the sky's hour
// ge:keywords fall-line fog mist valley-fog dress_mists fog_settings r_fog r_fog_quality froxels
// ge:skill render mountain
// ge:test tests/fall_line_mists_test.cpp
#pragma once

#include <vector>

#include "fall_line/app/frame.hpp"
#include "mountain/mapgen/types.hpp"
#include "render/fog.hpp"
#include "render/view.hpp"

namespace fall_line::app {

// The froxels at `r_fog_quality` 0 (low) .. 3 (top); 2 is the default's 160 x 90 x 64.
ge::render::FogSettings fog_settings(int quality);

// This frame's fog into `out` (cleared first) and `view.valley`: the scene's own when it brings
// any, else, with `on`, the map's at the scene's hour (the map's own when negative).
void dress_mists(const ge::mountain::mapgen::Level& level, bool on, const FrameScene& scene,
                 ge::render::FrameView& view, std::vector<ge::render::FogVolume>& out);

}  // namespace fall_line::app
