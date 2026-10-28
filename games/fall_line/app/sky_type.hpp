// THE KIND OF SKY a run is drawn under, off its cvars: sv_skytype names a type of
// render/sky_style.hpp's catalogue and its knobs (sv_skyair, sv_planet, ...) turn it, and
// sv_clouds names the kinds of cloud (render/cloud_kinds.hpp) drawn in place of the map's. Only
// the picture reads them: no step, digest or map moves with them.
//
// ge:summary Fall Line's sky type and knobs from its cvars, as the frame's sky style
// ge:keywords fall-line sky-type sky-style sv_skytype sky-knobs sky_style_of sv_clouds
// ge:keywords cloud-stack cloud_stack_of
// ge:skill render console
// ge:test tests/fall_line_sky_test.cpp
#pragma once

#include <string>

#include "fall_line/app/frame.hpp"
#include "fall_line/cfg/cvars.hpp"
#include "render/sky_style.hpp"

namespace fall_line::app {

// `out` set to the type sv_skytype names, its knobs turned; an unknown name keeps the Earth's
// and returns what went wrong (empty when nothing did).
std::string sky_style_of(const cfg::Settings& s, ge::render::SkyStyle& out);

// The kinds of cloud sv_clouds names ("map": the map's own, a count of -1), as `o`'s stack; a
// name no kind has keeps the kinds before it and returns what went wrong (empty when nothing
// did).
std::string cloud_stack_of(const cfg::Settings& s, FrameOptions& o);

}  // namespace fall_line::app
