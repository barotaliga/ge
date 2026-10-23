// FALL LINE'S THINGS ON THE MOUNTAIN, as instance families (render/instances.hpp): the FOREST,
// every tree of the map placed once at load, and the FIGURES, every racer drawn as the skier
// model (plugins/skier/assets/skier, compiled into the game) placed every frame where the run puts
// him. The trees are the mountain's own (mountain/app/trees.hpp: five forms, four variants a
// form, three LODs, the snow on the boughs, every kind in its colours), unit shapes (1 m tall with
// a crown 1 m in radius) scaled by each tree's height and crown.
//
// A FIGURE is a skinned mesh, and a skinned mesh is one pose (render/skinning.hpp): the family
// holds one mesh a racer, each the model's mesh at bind packed into the poser's reach
// (skier/app/rig_pose.hpp's `pose_reach`), posed on the GPU every frame from the racer's pose.
// The instance stands the mesh in the world at the skier's figure frame (skier/app/figure.hpp:
// the snow under him, his heading); each racer wears the model's colours in his own jacket.
//
// THE CROWD (the free ride's amateurs, sim/crowd.hpp) is the same model as a palette family
// (render/skinning.hpp): one mesh, an instance an amateur at his id, each posed by a palette of
// its own from his place and pose (`crowd_skier`: the amateur as a skier the poser reads), sized
// by his body and wearing one of the crowd's jackets. One on a lift's carrier or up a lift
// unseen is not drawn (a scale of nought: the cull's sphere is too small for any pixel).
//
// ge:summary Fall Line's forest and skinned skiers as instance families
// ge:keywords fall-line scene forest trees figures racers instances materials skinned
// ge:keywords skier-model forest_meshes figure_meshes tree_instances figure_instance skier_model
// ge:keywords jacket placement crowd amateurs crowd_meshes crowd_instance crowd_skier
// ge:skill render
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "assets/gltf.hpp"
#include "assets/model.hpp"
#include "fall_line/sim/crowd_state.hpp"
#include "mountain/mapgen/types.hpp"
#include "render/instances.hpp"
#include "skier/skier.hpp"

namespace fall_line::app {

// The most racers the figures' family holds.
inline constexpr int kMaxFigures = 8;

std::vector<ge::render::MeshSource> forest_meshes();
// Linear rgb, three floats a material.
std::vector<float> forest_materials();
// Every tree of the map, in its order.
std::vector<ge::render::Instance> tree_instances(const ge::mountain::mapgen::Level& level);

// The skier model, read once from the bytes compiled into the game; `ok` false (and `error` why)
// only when the committed skier.glb is broken.
const ge::assets::LoadResult& skier_model();
// One mesh a racer: the model's first mesh at bind in the poser's reach (render/skinning.hpp's
// `skinned_source`), mesh k for racer k.
std::vector<ge::render::MeshSource> figure_meshes(const ge::assets::Model& model);
// Every racer's materials, the model's in order with the jacket his own: racer k's are
// k * model.materials.size() on (linear rgb, three floats a material).
std::vector<float> figure_materials(const ge::assets::Model& model);
// Racer `racer`'s instance: his mesh and materials, at `skier`'s figure frame on `level`'s snow.
ge::render::Instance figure_instance(const ge::skier::SkierState& skier,
                                     const ge::mountain::mapgen::Level& level, int racer,
                                     uint32_t materials_per_racer);

// The most amateurs the crowd's family holds (sim/crowd_rules.hpp's most).
inline constexpr int kMaxCrowd = sim::crowd::kMost;

// The crowd's one mesh: the model's first at bind in the poser's reach, posed by each amateur's
// palette (render/skinning.hpp's `palette_source`).
std::vector<ge::render::MeshSource> crowd_meshes(const ge::assets::Model& model);
// The crowd's jackets, each the model's materials with the jacket its own: jacket k's are
// k * model.materials.size() on.
std::vector<float> crowd_materials(const ge::assets::Model& model);
// The amateur as a skier the poser and the figure frame read: where he is, his heading and
// velocity, his lean (the skis' edge with it), his crouch as the tuck, down when he is.
ge::skier::SkierState crowd_skier(const sim::Amateur& a);
// Whether he is drawn: on the snow (skiing, stopped, down, in a queue, skating off a top).
bool crowd_drawn(const sim::Amateur& a);
// His instance: at his figure frame on `level`'s snow, sized by his body, in his jacket; a scale
// of nought when he is not drawn.
ge::render::Instance crowd_instance(const sim::Amateur& a, const ge::mountain::mapgen::Level& level,
                                    uint32_t materials_per_jacket);

}  // namespace fall_line::app
