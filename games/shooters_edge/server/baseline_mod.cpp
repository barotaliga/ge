// The baseline mod compiled from its sources, and the arena's entity text for it.
//
// ge:summary The baseline QC deathmatch mod compiled from source, and its arena's entity text
// ge:keywords baseline-mod compile_baseline_mod entity-text
// ge:skill progs shooter
// ge:test tests/shooters_edge_baseline_mod_test.cpp
#include "shooters_edge/server/baseline_mod.hpp"

#include "progs/qc_compile.hpp"

namespace shooters_edge::server {

bool compile_baseline_mod(const std::filesystem::path& root, ge::progs::Progs& out,
                          std::string& error) {
  const std::filesystem::path src = root / kBaselineSource;
  ge::progs::QcProject project = ge::progs::compile_project(src);
  if (!project.result.ok) {
    error = "compile_baseline_mod: " + src.generic_string() + " did not compile:";
    for (const ge::progs::QcDiagnostic& d : project.result.diagnostics) {
      if (d.error) error += " " + ge::progs::diagnostic_text(d) + ";";
    }
    error += " next: `ge qc build " + std::string(kBaselineSource) + "` from the repository root";
    return false;
  }
  out = std::move(project.result.progs);
  return true;
}

// The arena (games/shooters_edge/sim/arena.hpp): the west room x -512..128, the east room
// 144..512, the doorway at x 128..144, y -64..64; the plat at x 192..320, y -384..-256 rising 96
// beside the ledge. Items stand clear of the spawn points' 84-unit check and of each other.
const char* const kBaselineArenaEntities = R"({
"classname" "worldspawn"
"message" "the test arena, baseline deathmatch"
}
{
"classname" "info_player_start"
"origin" "-416 0 24"
"angle" "0"
}
{
"classname" "info_player_deathmatch"
"origin" "256 -320 32"
"angle" "90"
"_comment" "on the plat: the first player in rides it up"
}
{
"classname" "info_player_deathmatch"
"origin" "-416 -256 24"
"angle" "45"
}
{
"classname" "info_player_deathmatch"
"origin" "-416 256 24"
"angle" "315"
}
{
"classname" "info_player_deathmatch"
"origin" "416 256 24"
"angle" "225"
}
{
"classname" "info_player_deathmatch"
"origin" "416 -96 24"
"angle" "180"
}
{
"classname" "info_player_deathmatch"
"origin" "0 384 24"
"angle" "270"
}
{
"classname" "info_player_deathmatch"
"origin" "0 -384 24"
"angle" "90"
}
{
"classname" "func_door"
"model" "*1"
"angle" "-1"
"speed" "100"
"wait" "3"
"lip" "8"
}
{
"classname" "func_plat"
"model" "*2"
"origin" "0 0 96"
"height" "96"
"speed" "64"
}
{
"classname" "weapon_rocketlauncher"
"origin" "-160 0 0"
}
{
"classname" "weapon_supershotgun"
"origin" "-256 448 0"
}
{
"classname" "weapon_nailgun"
"origin" "-256 -448 0"
}
{
"classname" "weapon_supernailgun"
"origin" "416 448 0"
}
{
"classname" "weapon_grenadelauncher"
"origin" "256 -448 104"
}
{
"classname" "weapon_lightning"
"origin" "448 -192 0"
}
{
"classname" "item_shells"
"origin" "-480 128 0"
}
{
"classname" "item_spikes"
"origin" "224 128 0"
}
{
"classname" "item_rockets"
"origin" "-480 -128 0"
"spawnflags" "1"
}
{
"classname" "item_cells"
"origin" "256 0 0"
}
{
"classname" "item_armor1"
"origin" "-160 384 0"
}
{
"classname" "item_armor2"
"origin" "288 384 0"
}
{
"classname" "item_armorInv"
"origin" "448 -448 104"
}
{
"classname" "item_health"
"origin" "-272 -8 0"
}
{
"classname" "item_health"
"origin" "-160 -256 0"
"spawnflags" "1"
}
{
"classname" "item_health"
"origin" "448 64 0"
"spawnflags" "2"
}
{
"classname" "light"
"origin" "0 0 200"
"light" "300"
}
)";

}  // namespace shooters_edge::server
