// A ROOM LEVEL AS A BODY MEETS IT: the triangles a mover (engine/move) sweeps its hull through,
// answered by a mesh world (move/mesh_world.hpp), so a character walks the level the renderer
// draws without collision of the level's own.
//
// WHAT IS SOLID. A prism room is solid where it is stored: its flat floor, ceiling and walls with
// the openings cut out, the frames round them and its solids, built with every relief left off
// (build.hpp), so a rock floor is walked as the flat floor it grows from, never as its bumps,
// and a body does not catch on a relief a few centimetres deep. A cave is solid where it is
// drawn: its rock is the shape, so the body meets the grown surface itself. Both are a pure
// function of the level, so a server and every client meet the same walls. A brush
// (brush_build.hpp) is met where its faces survive the CSG, a solid's, sky's or clip's, hidden ones
// too; a liquid brush is not met.
//
// ge:summary The room level as the mover's world: flat prisms, grown caves, frames and solids
// ge:keywords rooms clip collision mover move trace mesh-world walk hull solid flat cave group
// ge:skill rooms move
// ge:test tests/rooms_clip_test.cpp
#pragma once

#include <cstddef>

#include "move/mesh_world.hpp"
#include "rooms/level.hpp"

namespace ge::rooms {

// Every room of a linked level (level.hpp `link`) as one mesh world in metres, the rooms built
// side by side on `workers` threads (the same bits for any count), bucketed in `cell_m` cells.
// Without `groups`, the brushes of a group (a door, a platform: movers.hpp moves them) are left
// out, and the still world is what is left.
move::MeshWorld clip_world(const Level& level, size_t workers, double cell_m = 1.0,
                           bool groups = true);
// The brushes of group `group` alone, built without the rest (so a door's faces against its
// frame are kept for when it slides out), where they stand in the level.
move::MeshWorld clip_group(const Level& level, uint16_t group, size_t workers, double cell_m = 1.0);

// The room whose footprint and height hold the point `p` (world metres), -1 when none does.
int room_at(const Level& level, const move::Vec3& p);

}  // namespace ge::rooms
