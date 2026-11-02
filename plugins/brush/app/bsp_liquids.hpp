// A CLASSIC MAP'S LIQUIDS AS OURS (the v2 BSP renderer, bsp_draw.hpp): each group of a liquid's
// top surfaces (draw_data.hpp's `LiquidPatch`) made a body of engine/water (a volume from its
// level down to the deepest of the map's liquid under it, over its box), drawn by our water, lava
// or slime, each kind by its named row.
//
// - THE POOL'S SITE (`pool_site`), read off the map once: how deep its liquid goes (the map's
//   own leaves marched down from the top), how much of the dome over it is the sky, and the
//   share of the open map's wind that reaches it: rays from over the pool, level and climbing,
//   marched through the map's leaves to the sky or a wall. A ray that reaches the sky low down
//   and from the way the wind blows is a window or a doorway the wind comes in by; the sky
//   overhead lets a little down into a courtyard. A pool in a closed hall gets none: its water
//   lies still, as a pool indoors does, and only the room's faint stir moves it.
// - THE WIND (`liquid_world`'s `wind`, `from`: the weather's sv_wind and sv_winddir) times each
//   pool's share is the air its waves are grown from (water::lay_waves's own wind).
// - THE ROOM'S LIGHT (`light_rooms`): what the map's lightmaps say the pool's bed is lit by (the
//   light probe, plugins/brush/light_probe.hpp), floored like the walls, so the water's own colour
//   is the room's light in it and never the day's outside.
// - THE SUN ON THE WATER (`light_pools_by_sun`): the share of each pool the sun itself reaches,
//   the way to it marched through the leaves, for the caustics and the shafts.
//
// ge:summary A classic map's water, lava and slime tops made bodies of our water and liquids
// ge:keywords bsp brush liquids water lava slime body volume hull v2 add_bsp_liquids
// ge:keywords pool-site pool_site wind-exposure shelter still-pool depth room-light light_rooms
// ge:skill brush water render
#pragma once

#include <string>
#include <vector>

#include "brush/app/draw_data.hpp"
#include "gpu/device.hpp"
#include "render/liquid.hpp"
#include "render/water.hpp"
#include "water/world.hpp"

namespace ge::brush::app {

struct LiquidRows {
  std::string water = "water", water_look = "nordic-clear";  // the liquid's row, the water's look
  std::string lava = "lava", slime = "slime";                // rows of water/liquids.hpp
  double least_depth = 0.5;  // m: a pool's volume reaches at least this far under its top
  double lava_cover = 0.3;   // the share of the lava crusted over (the classic's lava is molten)
  // m: a crust plate's size and a young plate's crack (the classic's lava is fine-grained: its
  // texture's cells are a hand across, so the plates are too)
  double lava_plate = 0.25;
  double lava_crack = 0.02;
  double lava_patch = 0.6;  // m: the open melt's patches
  double lava_ropes = 0;    // the ropy folds on the crust (0 none: at this scale they alias)
  double lava_rough = 0.7;  // the crust's roughness: a dull, ash-dusted crust, not a glassy one
  // The slime drawn murky and grimy (a dark green ooze, scummed, dirtied, things afloat, a faint
  // glow) like the classic maps', not the toxic row's bright opaque green; false: the row's look.
  bool murky_slime = true;
  struct Grime {
    double murk = 1;       // how thick the ooze is to see into: 0 clearish, 1 murky, 2 opaque
    double scum = 0.35;    // the share a scum covers
    double grime = 0.8;    // 0..1: silt and rot mottled dark over it
    double debris = 1.5;   // floating bits a square metre
    double film = 300;     // nm: an oily sheen's thickness (0 none)
    double bubbles = 1.5;  // bubbles a square metre a second
    double glow = 0.4;     // its own green light, a share of the murky slime's
  } slime_grime;
  // The light of a room round a roofed pool whose bed has no lightmap, a share of a white face's
  // by day.
  double room = 0.3;
  // The slope of a still pool's own stir (the room's draughts, a drip, the warmth): the water is
  // never quite glass, but close.
  double stir = 0.004;
};

// What the map says of a pool, worked out once (`pool_site`).
struct PoolSite {
  double depth = 0;     // m: the deepest the liquid reaches under its top
  double sky = 0;       // 0..1: the share of the dome over it (weighed toward overhead) that is sky
  double upwind = 0;    // 0..1: the share of the low rays toward the wind that reach the sky
  double exposure = 0;  // 0..1: the share of the open map's wind on the water
  double light[3] = {0, 0, 0};  // the lightmaps' light on its bed (0..1 a channel); < 0: none
};

struct LiquidWorld {
  water::World world;
  std::vector<Kind> kinds;   // a body's: kWater, kSlime or kLava
  std::vector<double> open;  // the share of its surface the sky is over (open_sky_of)
  std::vector<PoolSite> sites;
  std::vector<LiquidPatch> patches;  // each body's top, as the map gave it
  double sun[3] = {0, 0, 0};         // the sun the pools' sunlit shares were last found for
  double wind = 0, from = 0;         // the open map's air the waves were laid under (m/s, rad)
};

// The share of the points over a pool's top from which straight up is the sky (a leaf of sky
// before any wall), on a grid of up to 8 x 8 over its box.
double open_sky_of(const Map& map, const LiquidPatch& patch);
// The pool's depth, its sky, its share of the wind from `from` (rad, the bearing it blows from)
// and its bed's light, read off `map`.
PoolSite pool_site(const Map& map, const LiquidPatch& patch, double from);

// The share of a pool's top the sun reaches along `sun` (unit, toward it): points on a grid over
// it from which the way to the sun meets the sky before a wall.
double pool_sunlit(const Map& map, const LiquidPatch& patch, const double sun[3]);
// Each water body's sunlit share (render/water.hpp's set_water_sunlit) for the sun along `sun`,
// found again only when it has moved half a degree: the caustics and the shafts only where the sun
// reaches the water, never in a wall's shade under the open sky.
void light_pools_by_sun(render::Water& water, const Map& map, LiquidWorld& world,
                        const double sun[3]);
// The bodies (at most render::kWaterBodies of water, the largest), each under its share of
// `wind` m/s from `from` (rad: the bearing it blows from; the weather's sv_wind and sv_winddir).
LiquidWorld liquid_world(const Scene& scene, const LiquidRows& rows, double wind = 3,
                         double from = 4.7);
// The waves laid again under another wind (the weather's cvars changed), into the passes.
void relay_liquid_wind(gpu::Device& device, render::Water& water, LiquidWorld& world, double wind,
                       double from);
// The room's light round the pools (each its bed's light off the lightmaps, else `rows.room`),
// floored like the walls (bsp_draw.hpp's `minlight`, in the lightmaps' units, times the
// renderer's brightness): sqrt(light^2 + (minlight brightness)^2).
void light_rooms(render::Water& water, render::Liquids& liquids, const LiquidWorld& world,
                 const LiquidRows& rows, double minlight, double brightness);
// Every body into the pass that draws it.
void add_bsp_liquids(gpu::Device& device, render::Water& water, render::Liquids& liquids,
                     const LiquidWorld& world, const LiquidRows& rows);
// The body the eye is in when the map says it is in a liquid (`map`'s leaf at the eye, engine
// metres): the water body whose box holds it, or the nearest over it (a flooded passage beyond
// the top's box); -1 in the air or in lava or slime. render/water.hpp's `prepare_water` takes it.
int eye_water_body(const Map& map, const LiquidWorld& world, const double eye[3]);

}  // namespace ge::brush::app
