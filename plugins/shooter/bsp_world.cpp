// A brush map's models as the server traces them (bsp_world.hpp).
//
// ge:summary Builds the server's brush models from a read map: the tree as hull 0, the clip nodes
// ge:keywords bsp_world hull0 point-nodes clipnodes models planes
// ge:skill shooter brush
// ge:test tests/shooter_bsp_world_test.cpp
#include "shooter/bsp_world.hpp"

namespace ge::shooter {

bool bsp_world(const brush::Map& map, BspWorld* out, std::string* error) {
  BspWorld& w = *out;
  w = BspWorld{};
  if (map.family != brush::Family::v29 && map.family != brush::Family::bsp2) {
    if (error) {
      *error = std::string("bsp_world: a version ") + brush::family_name(map.family) +
               " map sweeps through brushes, not clip hulls; next: load a version 29 or BSP2 map";
    }
    return false;
  }
  if (map.clip_nodes.empty()) {
    if (error) {
      *error = "bsp_world: the map has no clip hulls; next: compile the map with its hulls";
    }
    return false;
  }
  // A leaf's contents becomes a child in hull 0, where a number of 0 or more is a node: only the
  // format's contents numbers may stand there.
  for (size_t i = 0; i < map.leaves.size(); ++i) {
    const int32_t c = map.leaves[i].contents;
    if (c > -1 || c < -15) {
      if (error) {
        *error = "bsp_world: leaf " + std::to_string(i) + " has contents " + std::to_string(c) +
                 ", not one of the format's (-1 to -15); next: rebuild the map";
      }
      return false;
    }
  }
  w.planes.reserve(map.planes.size());
  for (const brush::Plane& p : map.planes) {
    Plane q;
    q.normal = {p.n[0], p.n[1], p.n[2]};
    q.dist = p.d;
    q.type = plane_type(q.normal);
    w.planes.push_back(q);
  }
  const auto leaf_contents = [&](int32_t child) {
    return map.leaves[size_t(-(int64_t(child) + 1))].contents;
  };
  w.point_nodes.reserve(map.nodes.size() + map.models.size());
  for (const brush::Node& n : map.nodes) {
    ClipNode c;
    c.plane = int32_t(n.plane);
    for (int k = 0; k < 2; ++k) {
      c.children[k] = n.child[k] >= 0 ? n.child[k] : leaf_contents(n.child[k]);
    }
    w.point_nodes.push_back(c);
  }
  w.clip_nodes.reserve(map.clip_nodes.size());
  for (const brush::ClipNode& n : map.clip_nodes) {
    w.clip_nodes.push_back(ClipNode{int32_t(n.plane), {n.child[0], n.child[1]}});
  }
  // Heads that are leaves first, so the arrays stop moving before any view is taken.
  std::vector<int32_t> heads(map.models.size());
  for (size_t i = 0; i < map.models.size(); ++i) {
    const int32_t h = map.models[i].head_node;
    if (h >= 0) {
      heads[i] = h;
    } else {
      const int32_t contents = leaf_contents(h);
      heads[i] = int32_t(w.point_nodes.size());
      w.point_nodes.push_back(ClipNode{0, {contents, contents}});
    }
  }
  const auto view = [](const std::vector<ClipNode>& nodes, const std::vector<Plane>& planes,
                       int32_t first) {
    return Hull{planes.data(), nodes.data(), first, int32_t(nodes.size()) - 1};
  };
  w.models.reserve(map.models.size());
  for (size_t i = 0; i < map.models.size(); ++i) {
    const brush::Model& md = map.models[i];
    BrushModel m;
    m.hulls[0] = view(w.point_nodes, w.planes, heads[i]);
    m.hulls[1] = view(w.clip_nodes, w.planes, md.hulls[0]);
    m.hulls[2] = view(w.clip_nodes, w.planes, md.hulls[1]);
    m.clip_mins[0] = {};
    m.clip_mins[1] = kPlayerMins;
    m.clip_mins[2] = kLargeMins;
    m.mins = {md.mins[0], md.mins[1], md.mins[2]};
    m.maxs = {md.maxs[0], md.maxs[1], md.maxs[2]};
    w.models.push_back(m);
  }
  return true;
}

}  // namespace ge::shooter
