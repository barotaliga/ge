// THE SHEETS a model is looked at on (over `lab/model_view.hpp`), one picture each, the same for
// any model whatever poses it (a clip, a game's poser, the rest pose), so every lab that shows a
// model shows it the same way:
//
// - `turntable_sheet`: the model all the way round, `count` azimuths in perspective.
// - `axes_sheet`: orthographic from the front, the right, the back and above, with a ruler a
//   metre a tick and the model's size, to read proportions.
// - `topology_sheet`: big, shaded with every triangle's edges, and its skeleton with the joints'
//   names: where the mesh is dense or thin and where the joints sit inside it.
// - `detail_sheet`: a close-up a region (`detail_region`) from outside the model and from the
//   opposite side, wired: the hands, the feet, the head, whatever is small on screen.
// - `lod_sheet`: every level of detail side by side at one size with its triangles, and again
//   drawn as small as it is seen at the distances a level is meant for, scaled up so its pixels
//   show: whether a level's silhouette holds where it is used.
// - `strip_sheet`: a motion's frames in a row from one or more azimuths.
//
// ge:summary The model sheets: turntable, axes with a ruler, topology, details, LODs, strips
// ge:keywords lab model-sheets turntable axes ruler topology wireframe detail close-up lod-sheet
// ge:keywords filmstrip strip angles proportions silhouette distance
// ge:skill labs assets
// ge:test tests/lab_model_view_test.cpp
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "lab/model_view.hpp"

namespace ge::lab {

Picture turntable_sheet(const assets::Model& model, const Shape& shape, double floor_y, int panel,
                        int count = 8, double elevation_deg = 12, double fov_deg = 30);
Picture axes_sheet(const assets::Model& model, const Shape& shape, double floor_y, int panel);
Picture topology_sheet(const assets::Model& model, const Shape& shape, double floor_y, int panel);

// The joints to look at close: `names` (comma-separated joint names; unknown ones are named in
// `missing`), or by default the skeleton's ends, then the joints whose own skin is small (3 to
// 35 cm round), one side of a mirrored pair, never a mesh's own node, at most `most`.
std::vector<int> detail_joints(const assets::Model& model, const std::string& names, int most,
                               std::string* missing);
// What a close-up of `joint` frames: its own skin when that is small, else the 15 cm round the
// joint itself (a ski's tip, a pole's basket: skinned to a bigger part).
bool detail_region(const assets::Model& model, const Shape& shape, int joint, float centre[3],
                   float* radius);
Picture detail_sheet(const assets::Model& model, const Shape& shape, std::span<const int> joints,
                     int panel);

// `shape_at_lod(n)` the model at level n; `triangles` each level's count.
Picture lod_sheet(const assets::Model& model, const std::function<Shape(int)>& shape_at_lod,
                  int levels, double floor_y, int panel, std::vector<size_t>* triangles);

// `frames` in a row a azimuth, each under its label; every panel at one scale (the frames' box).
Picture strip_sheet(const assets::Model& model, std::span<const Shape> frames,
                    std::span<const std::string> labels, std::span<const double> azimuths,
                    double floor_y, int panel, double elevation_deg = 10);

}  // namespace ge::lab
