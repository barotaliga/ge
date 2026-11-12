// A MAP FILE AS THE MOD SERVER PLAYS IT: a version 29 or BSP2 map read (plugins/brush), its brush
// models built as the server traces them (plugins/shooter/bsp_world.hpp), its entity lump as the
// entity text, and its visibility and hearing sets answering the server's `vis` hook.
//
// `read_bsp_map` fills a `BspMap` and the `MapSpec` `load_map` takes (name, models, entities);
// the spec's models point into the BspMap, so it must outlive the server that loads them (keep
// both together and never copy the BspMap once read). `use_map_sets` points the server's `vis`
// hook at the map: a point's set is its leaf's cluster's row of the PVS (`see`) or of the PHS,
// built at read; a point in a leaf with no cluster (solid, outside the map) sees and hears
// nothing, as the map's compiler left it.
//
// A map of a family that carries brushes instead of clip hulls (version 38) builds no hulls: the
// spec's `brushes` views the map in the BspMap, and the rules that trace brushes (the sequel's
// runner, server/sequel_server.hpp) play it.
//
// ge:summary A map file as the mod server plays it: read, its hulls and entities, its PVS and PHS
// ge:keywords bsp map load read_bsp_map use_map_sets pvs phs vis hearing entity-text models
// ge:keywords mapspec cluster leaf bsp_file_bounds model-bounds brushes version-38
// ge:skill shooter progs brush
// ge:test tests/shooters_edge_bsp_map_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "brush/map.hpp"
#include "shooter/bsp_world.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::server {

struct BspMap {
  ge::brush::Map map;  // the PHS built
  ge::shooter::BspWorld world;
};

// Reads the map file `bytes` named `name` (the model list's "maps/<name>.bsp") into `out` and
// `spec`; false with the reason and the next step in `error`.
bool read_bsp_map(std::span<const uint8_t> bytes, const std::string& name, BspMap* out,
                  MapSpec* spec, std::string& error);

// A brush model file's own bounds (`bytes`: "maps/b_explob.bsp"): its first model's mins and maxs
// as stored; false with the reason in `error` when it does not read.
bool bsp_file_bounds(std::span<const uint8_t> bytes, sh::Vec3f& mins, sh::Vec3f& maxs,
                     std::string& error);

// The server's visibility and hearing sets answered by the map's.
void use_map_sets(RulesCommon& s, const BspMap& map);

}  // namespace shooters_edge::server
