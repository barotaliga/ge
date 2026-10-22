// FALL LINE'S THINGS ON THE MOUNTAIN, as instance families (render/instances.hpp): the FOREST,
// every tree of the map placed once at load, and the FIGURES, every racer's pieces
// (skier/app/figure.hpp) placed every frame from the run. Both are placeholders until the world
// is drawn (docs/goals.md M8: the forest's own shapes and snow, the skinned figure): a faceted
// spruce cone, a bare broadleaf crown and a snag's trunk; a figure of boxes, cylinders and a
// ball, each racer in his own jacket.
//
// The meshes are unit shapes: a tree is 1 m tall with a crown 1 m in radius, scaled by its own
// height and crown; a figure's box and ball are 1 m across and its cylinder 1 m across and tall,
// scaled by each piece's size.
//
// ge:summary Fall Line's forest and figures as instance families: meshes, materials, placement
// ge:keywords fall-line scene forest trees figures racers placeholder instances materials
// ge:keywords forest_meshes figure_meshes tree_instances figure_instances jacket
// ge:skill render
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "mountain/mapgen/types.hpp"
#include "render/instances.hpp"
#include "skier/app/figure.hpp"
#include "skier/skier.hpp"

namespace fall_line::app {

// The most racers the figures' family holds.
inline constexpr int kMaxFigures = 8;

std::vector<ge::render::MeshSource> forest_meshes();
// Linear rgb, three floats a material.
std::vector<float> forest_materials();
// Every tree of the map, in its order.
std::vector<ge::render::Instance> tree_instances(const ge::mountain::mapgen::Level& level);

std::vector<ge::render::MeshSource> figure_meshes();
std::vector<float> figure_materials();
// Racer `racer`'s pieces, posed for `skier` on `level`'s snow.
void figure_instances(const ge::skier::SkierState& skier, const ge::mountain::mapgen::Level& level,
                      int racer, std::span<ge::render::Instance, ge::skier::kFigurePieces> out);

}  // namespace fall_line::app
