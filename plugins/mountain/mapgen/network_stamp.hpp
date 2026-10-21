// R8, R10, R17, R18, R27 — THE RUNS PRESSED INTO THE MOUNTAIN, in the order they were graded.
//
// Each run is pressed as the single piste's corridor is (surface.hpp): level across its width
// but for its camber, a flat shoulder and the bench its windrow stands on, a bank back to the
// mountain behind that, and the packed field with its drifts. A ski area adds one rule: the
// CORE of every run pressed before (its width and its flat shoulders) is never touched by a run
// pressed after it. A merging run levels everything else its corridor reaches, the other's
// windrow on the side it comes in from among it, so the windrow opens where a run joins; and
// past the junction, along the run it merged into, it touches nothing.
//
// Held by tests/mountain_resort_build_test.cpp (the ground and the packed field once graded).
//
// ge:summary R8 R27 the walked runs pressed into the ground in order, cores kept, junctions open
// ge:keywords network-stamp stamp-run on-core core bench windrow bank packed corridor junction
// ge:skill mountain
// ge:test tests/mountain_resort_build_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "core/heightfield.hpp"
#include "mountain/mapgen/network.hpp"
#include "mountain/mapgen/types.hpp"

namespace ge::mountain::mapgen {

// What every run's press shares, a cell each: the mask of the cores pressed so far (0 open, 1 a
// windrow's bench, 2 a core), and each cell's distance to the nearest run's edge, read as a race
// piste's is from its line (the crust keeps off by it, R21).
struct NetworkStamp {
  std::vector<uint8_t> core;
  std::vector<float> dist;
};

NetworkStamp network_stamp(const ge::core::Heightfield& ground);

// Whether a plan point stands on the core of a run already pressed.
bool on_core(const NetworkStamp& shared, const ge::core::Heightfield& ground, double x, double z);

// R8, R10, R17, R18, R27 — press a graded run into `ground` and `packed` everywhere but the cores
// of the runs pressed before it, and mark its own core in `shared` (a lane's never: lanes are
// pressed last, and one crossing another levels across it). `onto` is the run it merges into.
void stamp_run(const WalkedRun& run, ge::core::Heightfield& ground, ge::core::Heightfield& packed,
               NetworkStamp& shared, std::span<const Drift> drifts, const WalkedRun* onto);

}  // namespace ge::mountain::mapgen
