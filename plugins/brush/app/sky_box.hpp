// plugins/brush/app/sky_box.hpp: A MAP'S OWN SKY BOX, the six pictures a map names drawn round it
// at infinity on its sky faces in place of the sky texture's two scrolling layers.
//
// THE PICTURES come in the pack's order rt, bk, lf, ft, up, dn (kSkyBoxSuffixes), each the view
// out of the box along one axis, its top and right edges known (kSkyBoxPictures, in engine axes:
// the map's +x is the engine's +x, its +y the engine's +z, its +z up the engine's +y). None is
// mirrored. All six are needed; the box is square, its faces the smaller side of the `rt` picture
// (at most kSkyBoxMaxFace), and every picture is rescaled to it.
//
// `build_sky_box` turns them into the scene's atlas (draw_data.hpp's Scene::sky_box): the six
// faces in the shader's order (kSkyBoxLook and kSkyBoxUp), each resampled from its picture by the
// direction its texel looks along, bilinear in the picture's own texels.
//
// THE TURN (`sky_box_turn`): a map may turn its box about an axis (the map's own axes) at a rate in
// degrees a second, by the right hand (looking down the axis from its tip, the box turns
// counter-clockwise as the angle grows). The axis is made unit first; one shorter than 0.001, or
// a rate of 0, is no turn. The shader finds a direction's texel by turning it back.
//
// ge:summary A map's own sky box: six pictures turned into the scene's sky box atlas, and its turn
// ge:keywords sky-box skybox sky env rt bk lf ft up dn build_sky_box sky-pictures cube-map
// ge:keywords skyrotate skyaxis sky_box_turn sky-turn
// ge:keywords worldspawn-sky sky-orientation
// ge:skill brush render
// ge:test tests/brush_sky_box_test.cpp
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "assets/image.hpp"
#include "brush/app/draw_data.hpp"

namespace ge::brush::app {

// The six pictures' file suffixes, in the order `build_sky_box` takes them.
inline constexpr const char* kSkyBoxSuffixes[6] = {"rt", "bk", "lf", "ft", "up", "dn"};

// How each picture (kSkyBoxSuffixes's order) is seen from inside the box, engine axes, y up: the
// way it is looked along, the way its top edge points, the way its right edge points.
struct SkyBoxPicture {
  int look[3];
  int top[3];
  int right[3];
};
inline constexpr SkyBoxPicture kSkyBoxPictures[6] = {
    {{1, 0, 0}, {0, 1, 0}, {0, 0, -1}},   // rt: the map's +x
    {{0, 0, 1}, {0, 1, 0}, {1, 0, 0}},    // bk: the map's +y
    {{-1, 0, 0}, {0, 1, 0}, {0, 0, 1}},   // lf: the map's -x
    {{0, 0, -1}, {0, 1, 0}, {-1, 0, 0}},  // ft: the map's -y
    {{0, 1, 0}, {-1, 0, 0}, {0, 0, -1}},  // up: straight up, its top toward the map's -x
    {{0, -1, 0}, {1, 0, 0}, {0, 0, -1}},  // dn: straight down, its top toward the map's +x
};

// The largest face the box keeps (its atlas three faces wide); a larger set is rescaled to it.
inline constexpr int kSkyBoxMaxFace = 1024;

// The scene's sky box from the six pictures in kSkyBoxSuffixes's order (sRGB, any sizes): fills
// `scene->sky_box` and `scene->sky_box_face`. False, with the scene's box left empty and `why`
// naming the picture, when one is empty or its pixels do not fill its size.
bool build_sky_box(const std::array<assets::Image, 6>& pictures, Scene* scene,
                   std::string* why = nullptr);

// The rows of the matrix that turns a direction from the eye (engine axes) back into the box's own
// at `seconds` into the level: the box turned by `degrees_per_second` times `seconds` about
// `axis` (the map's axes, any length). The identity when there is no turn (above).
void sky_box_turn(const double axis[3], double degrees_per_second, double seconds,
                  double rows[3][3]);

}  // namespace ge::brush::app
