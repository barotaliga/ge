// THE SKY OVER A RUN AS THE SERVER SET IT (cfg/cvars.hpp's sv_weather, sv_snowfall, sv_fog,
// sv_wind, sv_winddir, sv_storm, sv_hour and sv_skyrate): the map's own weather replaced by the
// picked one before the run is made (mountain/mapgen/weather.hpp's `with_sky`, so nothing the map
// built moves, while the wind the skiers ride against and the fall do), and the hour the sky has
// reached a time into the run. sv_cover is the picture's alone (app/frame.hpp's
// FrameOptions::cover).
//
// ge:summary The sky over a run from the server's cvars: the weather picked, the sky's hour
// ge:keywords sky weather sv_weather sv_hour sv_skyrate sky-clock server_sky sky_hour with_sky
// ge:skill sim console
// ge:test tests/fall_line_sky_test.cpp
#pragma once

#include <string>

#include "fall_line/cfg/cvars.hpp"
#include "mountain/mapgen/types.hpp"

namespace fall_line::sim {

// `s`'s sky over `level` (its weather replaced as picked). Empty, or what was wrong with the
// pick (an unknown weather word: the map's own kind is kept, the rest still applies).
std::string server_sky(const cfg::Settings& s, ge::mountain::mapgen::Level& level);

// The sky's hour `t` seconds into the run: sv_hour (the map's start hour when negative) moved on
// by sv_skyrate, wrapped to 0..24.
double sky_hour(const cfg::Settings& s, const ge::mountain::mapgen::Level& level, double t);

}  // namespace fall_line::sim
