// THE VILLAGE AS DRAWN: its streets laid on the snow and its traffic (mountain/village), for a
// game to draw as one instance family (render/instances.hpp) beside its forest.
//
// THE STREETS are one mesh a map (`street_mesh`), cut off the village's ground mask (ground.hpp):
// every run of cells along a row that is a carriageway (or a junction, the car park, the square)
// or a sidewalk is a strip laid a hand's breadth over the snow at the strip's ends, no longer
// than `kStreetRun` m so the strip keeps to the ground between them. A carriageway is the
// ploughed, packed grey-brown of a village street in winter; a sidewalk the paler packed snow a
// blower leaves; the banks and the gardens are the snow itself and are not drawn.
//
// THE VEHICLES are a mesh a kind (`vehicle_meshes`), in the vehicle's own frame: its middle on
// the snow at the origin, +z forward, y up, the measures its kind's (traffic_rules.hpp). Chunky
// and faceted as the forest is: a car is a body, a glasshouse over it, bumpers and four tyres;
// the van a tall box with its cab's glass; the bus a long box with its band of windows; a bicycle
// a frame on two wheels and its rider. Each has a far LOD (a box or two) past `kVehicleLodPixels`.
//
// THE COLOURS: a material is `kVillageParts` colours (`village_materials`): the paint, the glass,
// the tyres, the trim, the lamps, the carriageway and the sidewalk. The first set is the streets',
// then one a paint (`kPaints`: the colours a village's cars come in, the bus's livery last), and
// a vehicle's is dealt off its id (`vehicle_material`). The cold's marks (`village_surfaces`):
// after a clear night the parked cars stand white with hoar, their windscreens rimed over, until
// the morning's sun takes it; freezing rain glazes the carriageway black.
//
// ge:summary The village as drawn: the streets' mesh off the ground mask, the vehicles' meshes
// ge:keywords village streets street-mesh vehicles vehicle-meshes cars bus bike paint glass
// ge:keywords tyres village-materials vehicle-instance parked-instance carriageway sidewalk
// ge:keywords village-cold village_surfaces frost black-ice glaze frosted-glass cold_steps
// ge:skill mountain render
// ge:test tests/mountain_village_look_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "mountain/mapgen/types.hpp"
#include "mountain/village/ground.hpp"
#include "mountain/village/traffic.hpp"
#include "mountain/village/traffic_rules.hpp"
#include "render/instances.hpp"
#include "render/mesh.hpp"

namespace ge::mountain::app {

// A face's part: an instance's material plus this picks its colour.
enum VillagePart : uint8_t {
  kPaint,     // a vehicle's body (a cyclist's jacket)
  kGlass,     // the windows
  kTyre,      // the tyres, the bike's frame
  kTrim,      // the bumpers, the sills, the mirrors
  kLamp,      // the head lamps
  kCarriage,  // a ploughed carriageway, a junction, the car park, the square
  kWalk,      // a sidewalk
};
inline constexpr int kVillageParts = 7;
// The paints a village's vehicles come in; the last is the ski bus's livery.
inline constexpr int kPaints = 9;
// How long a strip of street runs before it is laid on the snow again, m; how far over the snow.
inline constexpr double kStreetRun = 2;
inline constexpr double kStreetLift = 0.06;
// A vehicle's far LOD below this many px of its bounding sphere's radius.
inline constexpr float kVehicleLodPixels = 10.0f;
inline constexpr int kVehicleKinds = static_cast<int>(village::VehicleKind::bike) + 1;

// A kind's mesh at full detail (`lod` 0) or far (1), in its own frame.
render::MeshData vehicle_mesh(village::VehicleKind kind, int lod);
// Every kind's mesh in `VehicleKind` order, each with its two LODs: the village family's first
// `kVehicleKinds` meshes.
std::vector<render::MeshSource> vehicle_meshes();
// The streets' mesh off `mask` on `level`'s snow (world metres; the instance stands at the
// origin). Empty for an empty mask.
render::MeshData street_mesh(const mapgen::Level& level, const village::GroundMask& mask);
// The streets' set first, then each paint's: kVillageParts colours a set, linear rgb.
std::vector<float> village_materials();

// THE COLD ON THE VILLAGE (mountain/air/marks.hpp): what a clear night's frost and a freezing
// rain have laid on its surfaces at an hour.
struct VillageCold {
  double frost = 0;  // `frost_cover`, 0..1: the hoar on what stood out all night
  double glaze = 0;  // `glaze_at`, 0..1: black ice across the carriageway
};
// Frost and glaze shown in steps this fine, so a family's materials are written again only when
// a step is crossed.
inline constexpr double kColdStep = 1.0 / 32;
// The same sets as `village_materials`, each a whole surface under `cold`: the frost whitens the
// paint, the trim and the lamps and turns the glass to a white, opaque rime; it greys the streets
// a little. The glaze darkens the carriageway to a glossy near mirror of the sky. A cold of
// nought is `village_materials` made matte, bit for bit.
std::vector<render::Surface> village_surfaces(const VillageCold& cold);
// `cold` rounded to `kColdStep`.
VillageCold cold_steps(const VillageCold& cold);

// The paint set a vehicle `id` of `kind` wears (the bus its livery).
uint32_t vehicle_material(int32_t id, village::VehicleKind kind);

// A vehicle at a moment as an instance of mesh `kind` (its middle `pose`'s, turned to its
// heading); a scale of nought when it is off the map.
render::Instance vehicle_instance(const village::VehiclePose& pose, int32_t id);
render::Instance parked_instance(const village::Parked& car);

}  // namespace ge::mountain::app
