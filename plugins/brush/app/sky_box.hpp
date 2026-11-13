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
// ge:summary A map's own sky box: six pictures turned into the scene's sky box atlas
// ge:keywords sky-box skybox sky env rt bk lf ft up dn build_sky_box sky-pictures cube-map
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

}  // namespace ge::brush::app
