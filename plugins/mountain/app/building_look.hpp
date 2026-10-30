// THE VILLAGE'S BUILDINGS AS DRAWN (mountain/village/buildings.hpp), for a game to draw as one
// instance family beside the forest and the parked cars.
//
// A MESH A KIND (`building_mesh`), in the building's own frame: the middle of its footprint on
// its floor at the origin, its front toward +z, its width along x, y up, its kind's measures
// (building_rules.hpp). Low-poly and faceted as the forest and the cars are, each read by its
// shape at a skier's distance:
//   - A HOUSE: a stone ground floor and a timber storey under a gable to the street, its roof out
//     over deep eaves; THE FLATS the same over four storeys, its ridge along the street.
//   - THE LODGE: a boarded ground floor and the restaurant's band of glass over it under a broad
//     low gable; THE HOTELS a balcony on each floor across the front under a big gable.
//   - THE RENTAL, A SHOP: display windows under an awning, a rendered flat over.
//   - THE TICKET OFFICE, THE SKI SCHOOL, FIRST AID: a hut each, the kiosk a row of windows.
//   - THE GARAGE: ribbed steel on a concrete plinth, three doors a groomer drives through; THE
//     PUMP HOUSE: formwork concrete under a flat roof.
//   - THE CHURCH: a rendered nave under a steep roof, its bell tower at the front under a spire.
// Every roof is under the snow on it. Each has a far LOD (its walls and its roof, a spire) past
// `kBuildingLodPixels`.
//
// THE PLINTH: a building stands at the highest snow under its footprint (`Building::y`); the
// plinth (`kPlinthMesh`, a unit box under the origin) is scaled to its footprint and down to the
// lowest (`low`) and a little into the snow, so no wall hangs in the air on a slope.
//
// THE COLOURS: a material is `kBuildingParts` colours, one set a look (`kBuildingLooks`: the
// renders and the timbers a mountain village is built in, the garage's steel, the church's
// white); a building's set is dealt off its id among its kind's (`building_material`).
//
// THE LIGHTS: every look comes twice, its windows dark and then lit (`kLitSets` on: the glass a
// warm lamp-lit room, `kWindowNits`). Once the sun is `kLightsOnElevation` under the horizon a
// building's windows are lit while the share of its kind at home or at work at that hour
// (`lights_share`: the houses in the evening, the lodge's restaurant to closing, the shops to
// shutting, the garage while the groomers are out) is over its own draw (`lights_on`), so the
// village goes dark one window set at a time, never flickering.
//
// ge:summary The village's buildings as drawn: a mesh a kind, the plinth, the looks
// ge:keywords village buildings building-mesh building-meshes plinth gable roof snow-roof eaves
// ge:keywords chalet lodge hotel church spire garage building-surfaces building-instances
// ge:keywords lit-windows lights-on lights-share night window-glow
// ge:skill mountain render
// ge:test tests/mountain_building_look_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "mountain/village/building_rules.hpp"
#include "mountain/village/buildings.hpp"
#include "render/instances.hpp"
#include "render/mesh.hpp"
#include "render/surface.hpp"

namespace ge::mountain::app {

// A face's part: an instance's material plus this picks its colour.
enum BuildingPart : uint8_t {
  kRender,  // a rendered wall
  kBoards,  // timber: boarding, balconies, the kiosk
  kStone,   // a stone ground floor, the plinth, concrete
  kSnow,    // the snow on a roof
  kPane,    // the windows' glass
  kFrame,   // the frames, the fascias, the doors
  kSteel,   // the garage's cladding, an awning
};
inline constexpr int kBuildingParts = 7;
// The looks a village's buildings come in (one material set each, then each again lit).
inline constexpr int kBuildingLooks = 7;
// Added to a set to light its windows.
inline constexpr uint32_t kLitSets = kBuildingLooks * kBuildingParts;
// A lit window's own light, cd/m^2: a lamp-lit room seen through its glass.
inline constexpr float kWindowNits = 5.0f;
// The windows are lit once the sun is this far under the horizon, rad (-2 degrees).
inline constexpr double kLightsOnElevation = -0.035;
// The family's meshes: a kind each in `BuildingKind` order, then the plinth.
inline constexpr uint32_t kPlinthMesh = village::kBuildingKinds;
// A building's far LOD below this many px of its bounding sphere's radius.
inline constexpr float kBuildingLodPixels = 14.0f;
// How far the roof runs out past the walls, m; how far the plinth sinks into the snow.
inline constexpr float kEaves = 0.9f;
inline constexpr double kPlinthSink = 0.4;

// A kind's mesh at full detail (`lod` 0) or far (1), in its own frame.
render::MeshData building_mesh(village::BuildingKind kind, int lod);
// Every kind's mesh in `BuildingKind` order with its two LODs, then the plinth's.
std::vector<render::MeshSource> building_meshes();
// Every look's set, dark, then every one lit: kBuildingParts surfaces a set.
std::vector<render::Surface> building_surfaces();
// The set building `b` wears by day.
uint32_t building_material(const village::Building& b);
// The share of `kind`'s buildings lit at solar `hour` (0..24) once it is dark.
double lights_share(village::BuildingKind kind, double hour);
// Whether building `b`'s windows are lit at solar `hour` with the sun `sun_elevation` (rad) up.
bool lights_on(const village::Building& b, double hour, double sun_elevation);
// Building `b` as two instances: itself (its windows lit or dark), then its plinth.
void building_instances(const village::Building& b, std::vector<render::Instance>& out,
                        bool lit = false);

}  // namespace ge::mountain::app
