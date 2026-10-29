// ge:summary A room level's grass: grass-surfaced materials' up-facing floors grown into a cover
// ge:keywords grass surface level floor cover recipe material brush room grow
// ge:skill rooms grass
// ge:test tests/rooms_grass_test.cpp
#include "rooms/grass.hpp"

#include <cmath>

#include "grass/kind.hpp"

namespace ge::rooms {

namespace {

constexpr std::string_view kGrassWord = "grass";

// The normal's y of triangle (a, b, c), counter-clockwise seen from the side it faces.
float up_of(const float* a, const float* b, const float* c) {
  const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
  const float v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
  const float n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                      u[0] * v[1] - u[1] * v[0]};
  const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  return len > 0 ? n[1] / len : 0;
}

// Each kind's triangles handed to the cover in one call, kinds in order.
void add_by_kind(LevelGrass& grass, std::span<const float> pos,
                 std::vector<std::vector<uint32_t>>& by_kind, float min_up) {
  for (size_t k = 0; k < by_kind.size(); ++k) {
    if (by_kind[k].empty()) continue;
    grass::add_mesh(grass.cover, pos, by_kind[k], uint32_t(k), 1, min_up);
    by_kind[k].clear();
  }
}

}  // namespace

std::string_view grass_recipe(const Material& material) {
  std::string_view s = material.surface;
  while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
  if (s.substr(0, kGrassWord.size()) != kGrassWord) return {};
  s.remove_prefix(kGrassWord.size());
  if (!s.empty() && s.front() != ' ') return {};  // a word that only starts with "grass"
  while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
  return s.empty() ? std::string_view("meadow") : s;
}

LevelGrass level_grass(const Level& level) {
  LevelGrass g;
  g.kind.assign(level.materials.size(), -1);
  std::vector<std::string> recipes;
  for (size_t m = 0; m < level.materials.size(); ++m) {
    const Material& mat = level.materials[m];
    const std::string_view recipe = grass_recipe(mat);
    if (recipe.empty()) continue;
    int found = -1;
    for (size_t r = 0; r < recipes.size(); ++r) {
      if (recipes[r] == recipe) found = int(r);
    }
    if (found < 0) {
      std::string error;
      const auto kind = grass::parse_kind(recipe, &error);
      if (!kind) {
        g.faults.push_back("rooms: material '" + mat.name + "' grows grass '" +
                           std::string(recipe) + "', which does not read: " + error +
                           "; next: write a preset (lawn, meadow, long, tussock, steppe, reeds) "
                           "and its overrides, as engine/grass/kind.hpp says");
        continue;
      }
      if (g.cover.kinds.size() == grass::kMaxKinds) {
        g.faults.push_back("rooms: material '" + mat.name + "' would be grass kind " +
                           std::to_string(grass::kMaxKinds + 1) + ", past the " +
                           std::to_string(grass::kMaxKinds) +
                           " a cover holds; next: give materials the same recipe to share a kind");
        continue;
      }
      found = int(recipes.size());
      recipes.emplace_back(recipe);
      g.cover.kinds.push_back(*kind);
    }
    g.kind[m] = found;
  }
  uint32_t seed = 0;
  for (const Room& r : level.rooms) seed = grass::hash(seed ^ r.seed);
  g.cover.seed = seed;
  return g;
}

void grow_on_brushes(LevelGrass& grass, const BrushMesh& mesh, float min_up) {
  std::vector<std::vector<uint32_t>> by_kind(grass.cover.kinds.size());
  for (size_t t = 0; t < mesh.triangles(); ++t) {
    if (mesh.hidden[t] || mesh.contents[t] != Contents::solid) continue;
    const size_t mat = mesh.material[t];
    if (mat >= grass.kind.size() || grass.kind[mat] < 0) continue;
    const uint32_t* i = &mesh.idx[t * 3];
    if (up_of(&mesh.pos[size_t(i[0]) * 3], &mesh.pos[size_t(i[1]) * 3],
              &mesh.pos[size_t(i[2]) * 3]) < min_up) {
      continue;
    }
    by_kind[size_t(grass.kind[mat])].insert(by_kind[size_t(grass.kind[mat])].end(), i, i + 3);
  }
  add_by_kind(grass, mesh.pos, by_kind, min_up);
}

void grow_on_rooms(LevelGrass& grass, std::span<const Mesh> rooms, float min_up) {
  std::vector<std::vector<uint32_t>> by_kind(grass.cover.kinds.size());
  for (const Mesh& m : rooms) {
    for (size_t t = 0; t < m.triangles(); ++t) {
      const uint32_t* i = &m.idx[t * 3];
      const size_t mat = m.mat[i[0]];
      if (m.mat[i[1]] != mat || m.mat[i[2]] != mat) continue;
      if (mat >= grass.kind.size() || grass.kind[mat] < 0) continue;
      // A room's normals point into the room, so a floor's point up whatever the winding.
      float up = 0;
      for (int k = 0; k < 3; ++k) up += m.nrm[size_t(i[k]) * 3 + 1];
      if (up < 3 * min_up) continue;
      by_kind[size_t(grass.kind[mat])].insert(by_kind[size_t(grass.kind[mat])].end(), i, i + 3);
    }
    add_by_kind(grass, m.pos, by_kind, min_up);
  }
}

}  // namespace ge::rooms
