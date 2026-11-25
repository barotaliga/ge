// THE DIRECTION TABLE of Quake II's protocol (and of both classic games' model formats, whose
// vertex normals index the same table): 162 unit vectors, a direction sent as one byte, its index.
//
// The vectors are the vertices of an icosahedron (the cyclic permutations of (0, ±phi, ±1),
// normalized) with each triangle's edges divided into four and every point pushed out onto the unit
// sphere: 10 × 4 × 4 + 2 = 162. They are built here, then put in the protocol's order by a
// permutation measured against the table a stock program holds, and rounded to six decimals as it
// holds them, its 24 components of magnitude cos 18° taken one sixth-place unit toward zero
// (porting/shooters-edge/q2-demo.md § 9 has the measurements).
//
// THE PICK (`direction_index`): the entry with the largest dot product, computed in 32-bit floats,
// a tie keeping the lower index; no positive dot product (a zero or missing direction) gives 0.
// The direction need not be normalized. A reader refuses an index of 162 or more.
//
// ge:summary Quake II protocol's table of 162 directions: built, ordered, and the pick by dot
// ge:keywords direction-table normals anorms byte-direction write-dir read-dir spark-direction
// ge:keywords direction_index directions icosahedron vertex-normal
// ge:skill classic shooter net
// ge:test tests/classic_protocol_test.cpp
#pragma once

#include <array>

namespace ge::classic::protocol {

inline constexpr int kDirections = 162;

using Direction = std::array<float, 3>;

// The table, in the protocol's order (built once, on first use).
const std::array<Direction, kDirections>& directions();
// The byte a direction `d` is sent as (above).
int direction_index(const float d[3]);

}  // namespace ge::classic::protocol
