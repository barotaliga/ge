// THE KIND OF SKY a run is drawn under, off its cvars: sv_skytype names a type of
// render/sky_style.hpp's catalogue and its knobs (sv_skyair, sv_planet, ...) turn it, sv_moons
// names the moons over it (render/moons.hpp), and sv_clouds the kinds of cloud
// (render/cloud_kinds.hpp) drawn in place of the map's. Only
// the picture reads them: no step, digest or map moves with them.
//
// ge:summary Fall Line's sky type and knobs from its cvars, as the frame's sky style
// ge:keywords fall-line sky-type sky-style sv_skytype sky-knobs sky_style_of sv_clouds
// ge:keywords cloud-stack cloud_stack_of sv_moons moon_set_of other-moons
// ge:skill render console
// ge:test tests/fall_line_sky_test.cpp
#pragma once

#include <string>

#include "fall_line/app/frame.hpp"
#include "fall_line/cfg/cvars.hpp"
#include "render/moons.hpp"
#include "render/sky_style.hpp"

namespace fall_line::app {

// `out` set to the type sv_skytype names, its knobs turned; an unknown name keeps the Earth's
// and returns what went wrong (empty when nothing did).
std::string sky_style_of(const cfg::Settings& s, ge::render::SkyStyle& out);

// The moons sv_moons names as `out` ("type": the sky type's own, render/sky_style.hpp's); words
// that do not read keep the Earth's moon alone and return what went wrong (empty when nothing
// did).
std::string moon_set_of(const cfg::Settings& s, ge::render::MoonSet& out);

// The kinds of cloud sv_clouds names ("map": the map's own, a count of -1), as `o`'s stack; a
// name no kind has keeps the kinds before it and returns what went wrong (empty when nothing
// did).
std::string cloud_stack_of(const cfg::Settings& s, FrameOptions& o);

}  // namespace fall_line::app
