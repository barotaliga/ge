// THE MAP COMPILER'S TREE: brushes split into a binary space partition whose every leaf is wholly
// inside brushes of one contents or wholly open, the portals between its leaves, the flood that
// finds the inside, and the faces the portals leave. Family-free; compile.hpp says what reads it.
//
// - **The tree** (`build_tree`): the brushes are cut by their own sides' planes, a plane at a
//   time, until every side of every piece in a node lies on a plane some node above already split
//   on; such a node is a leaf, wholly inside each piece left in it, and takes the strongest of
//   their contents (solid, sky, lava, slime, water; open when none). Each split is chosen among the
//   pieces' untried sides, drawn sides before the sides a cut made, by the fewest pieces cut (5
//   each) and the even count either side, an axial plane before a slanted one; past 64 pieces only
//   48 planes spread over the candidates are weighed.
// - **Portals** (`make_portals`): the region's box is bounded by six portals to an outside leaf;
//   each node cuts a new portal on its plane by the portals bounding it, then splits those between
//   its children, so every leaf ends bounded by the convex polygons it shares with its neighbours.
// - **The flood** (`flood_fill`): from every point entity's leaf through the portals between open
//   leaves. Reaching the outside leaf is a LEAK: the map is open to the void, refused with the
//   entity it started from and the portals' centres on the way out. Every open leaf it never
//   reaches is filled solid, so the outside of a sealed map is gone.
// - **Faces** (`make_faces`): a portal between leaves of different contents shows the stronger
//   one's surface toward the weaker (a wall seen from the room, a pool's top from the air) and,
//   when the stronger is a liquid, from inside it too. The face takes the texture of the brush side
//   in the stronger leaf that lies on the portal's plane facing out of it. Faces on one node's
//   plane with one texture and side are merged where their union is convex.
//
// ge:summary The map compiler's tree: brushes split to a BSP, portals, the flood and leaks, faces
// ge:keywords map-compiler bsp brush-bsp split-plane portal portalize flood-fill leak outside
// ge:keywords fill faces merge texinfo leaf contents compile-tree
// ge:skill brush
// ge:test tests/brush_compile_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "brush/compile/geom.hpp"
#include "brush/compile/source.hpp"

namespace ge::brush::mapc {

// One texture projection a face uses; faces share them.
struct TexInfo {
  float vecs[2][4] = {};
  uint32_t texture = 0;  // into Compile::textures
  int32_t flags = 0;     // 1: special (a liquid or the sky: no lightmap)
  int32_t value = 0;     // the 38 family's surface value (a light's brightness, say)
  int32_t surf = 0;      // the 38 family's surface flags: the side's own, the sky 4, a liquid 8
};

// A brush as the tree splits it: sides with their polygons.
struct Piece {
  struct PSide {
    int plane = 0;  // oriented id, facing out of the piece
    Winding w;
    int texinfo = -1;  // −1: a cut's side (drawn nowhere)
    bool tested = false;
  };
  std::vector<PSide> sides;
  uint32_t contents = kSolid;
  int brush = 0;  // the source brush it is a piece of (into Model::brushes)
  V3 lo = {0, 0, 0}, hi = {0, 0, 0};
};

struct Node {
  int plane = -1;           // canonical index (front: its normal's side); −1 a leaf
  int child[2] = {-1, -1};  // [0] in front, [1] behind
  uint32_t contents = kEmpty;
  bool outside = false;      // the leaf beyond the region's box
  std::vector<int> brushes;  // a leaf's source brushes
  std::vector<int> portals;
  std::vector<int> faces;  // a node's faces (on its plane); a leaf's marked faces
  V3 lo = {0, 0, 0}, hi = {0, 0, 0};
  int reached = -1;  // the flood: the entity it came from
  int via = -1;      // the flood: the portal it came through
};

struct Portal {
  int plane = 0;  // canonical index: nodes[0] in front of it
  Winding w;
  int nodes[2] = {-1, -1};
  int onnode = -1;  // the node whose plane it lies on (−1: the region's box)
};

struct Face {
  int plane = 0;      // canonical index
  bool back = false;  // faces along −normal
  int texinfo = 0;
  Winding w;  // counter-clockwise seen from where it faces
  int node = -1;
  std::vector<int> leaves;  // the leaves that see it
  // The lightmap (light.hpp): first luxel and luxels each way, the styles, one byte a luxel.
  int64_t base[2] = {}, size[2] = {};
  uint8_t styles[4] = {255, 255, 255, 255};
  std::vector<uint8_t> luxels;
};

// A model's tree: the world, or one brush entity.
struct Tree {
  std::vector<Node> nodes;
  std::vector<Portal> portals;
  std::vector<Face> faces;
  int head = 0, outside = -1;
  V3 lo = {0, 0, 0}, hi = {0, 0, 0};  // the region (the brushes' bounds grown by 16)
};

// What a source brush's sides are after the compiler read them: oriented plane ids and texinfos,
// for the faces to take their textures from.
struct BrushSides {
  uint32_t contents = kSolid;
  std::vector<int> planes, texinfos;
};

// Pieces of brushes made from their planes (`texinfos[b][s]` side s of brush b's; sides that leave
// no polygon dropped); a brush that does not close is left out and named in `*warnings`. `*sides`
// gets every brush's sides, one entry a brush (empty for one left out); a piece's `brush` indexes
// it.
std::vector<Piece> make_pieces(const std::vector<Brush>& brushes,
                               const std::vector<std::vector<int>>& texinfos, Planes* planes,
                               std::vector<BrushSides>* sides, std::vector<std::string>* warnings);
// The tree over `pieces`; `lo..hi` the region.
void build_tree(std::vector<Piece> pieces, const V3& lo, const V3& hi, Planes* planes, Tree* tree);
void make_portals(Planes* planes, Tree* tree);
// The leaf a point is in.
int leaf_at(const Tree& tree, const Planes& planes, const V3& p);

struct Leak {
  int entity = -1;
  std::vector<V3> path;  // from the entity out
};
// Floods from `seeds` (a point and its entity); false with `*leak` when the outside is reached.
// No seeds is a leak with entity −1.
struct Seed {
  V3 at;
  int entity;
};
bool flood_fill(const std::vector<Seed>& seeds, const Planes& planes, Tree* tree, Leak* leak);
// The faces the portals show, textured off the brushes' sides; `*missing` counts faces with no side
// on their plane to take a texture from (they take the leaf's first brush's first side's).
void make_faces(const std::vector<BrushSides>& brushes, const std::vector<TexInfo>& texinfos,
                Tree* tree, size_t* missing);
// Faces on one node with one texture and side merged where convex.
void merge_faces(Tree* tree, const Planes& planes);
// Faces cut so no lightmap spans more than 240 texels (special faces kept whole).
void subdivide_faces(Tree* tree, const Planes& planes, const std::vector<TexInfo>& texinfos);
// Each leaf's marked faces, from the faces' leaves; each node's bounds from its leaves' portals.
void finish_tree(Tree* tree);

}  // namespace ge::brush::mapc
