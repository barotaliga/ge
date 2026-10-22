// FALL LINE'S NAMED VIEWS: where a picture of a map is taken from, by name, so the same view is
// the same picture on every run (docs/labs.md § Pictures before and after the renderer). A named
// view is a camera rig over the generated map; a moment of a run is seen from the CHASE camera
// (`chase_view`), behind and over the skier along his way.
//
// THE DAY: `day_view` puts the map's own sun (its hour, day of the year and latitude, R15) and
// its altitude into a FrameView; every view starts from it.
//
// ge:summary Fall Line's named camera views over a map and the map's day for the renderer
// ge:keywords fall-line views named-views camera-rig start piste vista lookback aerial sun day
// ge:keywords wood sunlit-wood tree-shadows
// ge:keywords view_names make_view day_view chase_view chase-camera run-moment
// ge:skill render
#pragma once

#include <span>
#include <string_view>

#include "mountain/mapgen/types.hpp"
#include "render/view.hpp"
#include "skier/skier.hpp"

namespace fall_line::app {

// The generator version the app builds its maps with: the single piste (v1) until the ski area
// is built (docs/goals.md M4b).
inline constexpr int kMapVersion = 1;

// Every named view, in the order a lab shoots them.
std::span<const std::string_view> view_names();

// The view's camera over `level`, under the map's own day (`out`'s sun is read: `wood` finds the
// wood that sun lights best). False for an unknown name.
bool make_view(const ge::mountain::mapgen::Level& level, std::string_view name,
               ge::render::FrameView& out);

// The map's sun and altitude, at `hour` (the map's own start hour when negative).
void day_view(const ge::mountain::mapgen::Level& level, double hour, ge::render::FrameView& out);

// The chase camera on `skier`: `back` m behind him along his way (his heading when he is nearly
// still) and `up` m over him, looking at a point a metre over his skis; never under the snow.
void chase_view(const ge::mountain::mapgen::Level& level, const ge::skier::SkierState& skier,
                ge::render::FrameView& out, double back = 5.5, double up = 2.0);

// What each view shows, for a lab's caption.
std::string_view view_caption(std::string_view name);

}  // namespace fall_line::app
