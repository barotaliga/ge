// A room level's grass (plugins/rooms/grass.hpp): a material's surface names grass with a recipe
// and nothing else does, the vault sample grows grass on its grass floor's top and nowhere else
// (not its sides, not its foot, not the flagstone), materials of one recipe share a kind and a
// recipe that does not read is a fault naming the material, a grown room's floors grow and its
// walls and ceilings do not, a cloned map grows on the texture its texture map surfaces as grass,
// and the same level grows the same blades to the bit, held in tests/golden/rooms_grass.txt.
//
// A deliberate change to where a level's grass grows moves the golden: rewrite it with
// `GE_GOLDEN_WRITE=1 build/<preset>/tests/rooms_grass_test` and say why in the PR.
//
// ge:summary Test: a room level's grass: recipes, the vault's grass floor, rooms, a clone, golden
// ge:keywords test rooms grass surface recipe cover floor vault clone texmap golden
// ge:skill rooms grass testing
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "golden.hpp"
#include "grass/cover.hpp"
#include "rooms/brush_build.hpp"
#include "rooms/build.hpp"
#include "rooms/clone.hpp"
#include "rooms/grass.hpp"
#include "rooms/text.hpp"

namespace {

namespace rm = ge::rooms;
namespace br = ge::brush;
namespace gr = ge::grass;
constexpr const char* kGolden = "tests/golden/rooms_grass.txt";

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

std::string read_text(const char* rel) {
  std::ifstream in(ge::test::root_path(rel));
  std::stringstream s;
  s << in.rdbuf();
  return s.str();
}

rm::Level load(const char* rel) {
  rm::Level level;
  std::string error;
  CHECK_MSG(rm::parse(read_text(rel), &level, &error), error);
  return level;
}

size_t material_named(const rm::Level& level, const std::string& name) {
  for (size_t m = 0; m < level.materials.size(); ++m) {
    if (level.materials[m].name == name) return m;
  }
  CHECK_MSG(false, "no material " + name);
  return 0;
}

double cover_area(const gr::Cover& cover) {
  double a = 0;
  for (const gr::Tri& t : cover.tris) a += gr::tri_area(t);
  return a;
}

// The area of a brush mesh's drawn triangles of `material` that face straight up.
double up_area(const rm::BrushMesh& mesh, size_t material) {
  double a = 0;
  for (size_t t = 0; t < mesh.triangles(); ++t) {
    if (mesh.material[t] != material || mesh.hidden[t]) continue;
    const float* p[3];
    for (int k = 0; k < 3; ++k) p[k] = &mesh.pos[size_t(mesh.idx[t * 3 + k]) * 3];
    const double u[3] = {p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2]};
    const double v[3] = {p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2]};
    const double n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                         u[0] * v[1] - u[1] * v[0]};
    const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (len > 0 && n[1] / len > 0.99) a += len / 2;
  }
  return a;
}

uint64_t blades_digest(const std::vector<gr::Blade>& blades) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (const gr::Blade& b : blades) {
    const auto* c = reinterpret_cast<const uint8_t*>(&b);
    for (size_t i = 0; i < sizeof b; ++i) h = (h ^ c[i]) * 0x100000001b3ull;
  }
  return h;
}

rm::LevelGrass vault_grass() {
  const rm::Level level = load("plugins/rooms/levels/vault.gelv.txt");
  rm::LevelGrass g = rm::level_grass(level);
  rm::grow_on_brushes(g, rm::build_brushes(level, 1));
  return g;
}

}  // namespace

TEST(a_surface_names_grass_with_a_recipe_and_nothing_else_does) {
  rm::Material m;
  m.surface = "grass meadow height=0.6";
  CHECK(rm::grass_recipe(m) == "meadow height=0.6");
  m.surface = "grass";
  CHECK(rm::grass_recipe(m) == "meadow");
  m.surface = "  grass   lawn";
  CHECK(rm::grass_recipe(m) == "lawn");
  m.surface = "grassland";
  CHECK(rm::grass_recipe(m).empty());
  m.surface = "";
  CHECK(rm::grass_recipe(m).empty());
  m.surface = "moss";
  CHECK(rm::grass_recipe(m).empty());
}

TEST(the_vault_grows_grass_on_its_grass_floor_and_nowhere_else) {
  const rm::Level level = load("plugins/rooms/levels/vault.gelv.txt");
  const size_t grass = material_named(level, "grass");
  rm::LevelGrass g = rm::level_grass(level);
  CHECK(g.faults.empty());
  CHECK(g.cover.kinds.size() == 1);
  CHECK(g.cover.kinds[0].name == "meadow");
  CHECK(g.kind[grass] == 0);
  for (size_t m = 0; m < level.materials.size(); ++m) CHECK(m == grass || g.kind[m] < 0);
  const rm::BrushMesh mesh = rm::build_brushes(level, 1);
  rm::grow_on_brushes(g, mesh);
  CHECK(!g.cover.tris.empty());
  // Only the floor brush's top: every corner at y = 0, and the area the grass faces up over.
  for (const gr::Tri& t : g.cover.tris) {
    CHECK(t.a[1] == 0 && t.b[1] == 0 && t.c[1] == 0);
    CHECK(t.kind == 0);
  }
  const double want = up_area(mesh, grass);
  const double got = cover_area(g.cover);
  CHECK_MSG(want > 20 && std::fabs(got - want) < 1e-3 * want,
            "grass " + std::to_string(got) + " m2, the grass floor " + std::to_string(want));
  CHECK(!gr::scatter_all(g.cover).empty());
}

TEST(one_recipe_is_one_kind_and_a_bad_recipe_is_a_fault) {
  rm::Level level = load("plugins/rooms/levels/vault.gelv.txt");
  level.materials[material_named(level, "floor")].surface = "grass meadow height=0.6 density=300";
  level.materials[material_named(level, "wood")].surface = "grass lawn";
  level.materials[material_named(level, "wall")].surface = "grass bogus";
  const rm::LevelGrass g = rm::level_grass(level);
  CHECK(g.cover.kinds.size() == 2);
  CHECK(g.kind[material_named(level, "floor")] == g.kind[material_named(level, "grass")]);
  CHECK(g.kind[material_named(level, "wood")] == 1);
  CHECK(g.kind[material_named(level, "wall")] == -1);
  CHECK(g.faults.size() == 1);
  CHECK_MSG(g.faults[0].find("'wall'") != std::string::npos &&
                g.faults[0].find("next:") != std::string::npos,
            g.faults[0]);
  // Every grass material's up-facing faces grow, and the wall whose recipe failed grows none.
  rm::LevelGrass grown = g;
  const rm::BrushMesh mesh = rm::build_brushes(level, 1);
  rm::grow_on_brushes(grown, mesh);
  const double want = up_area(mesh, material_named(level, "floor")) +
                      up_area(mesh, material_named(level, "wood")) +
                      up_area(mesh, material_named(level, "grass"));
  CHECK(std::fabs(cover_area(grown.cover) - want) < 1e-3 * want);
  CHECK(cover_area(grown.cover) >= cover_area(vault_grass().cover));
}

TEST(a_grown_rooms_floors_grow_and_its_walls_and_ceilings_do_not) {
  rm::Level level = load("plugins/rooms/levels/halls.gelv.txt");
  const size_t flags = material_named(level, "flags");
  const size_t gravel = material_named(level, "gravel");
  level.materials[flags].surface = "grass lawn";
  level.materials[gravel].surface = "grass tussock";
  rm::LevelGrass g = rm::level_grass(level);
  CHECK(g.faults.empty() && g.cover.kinds.size() == 2);
  const std::vector<rm::Mesh> rooms = rm::build_level(level, 2);
  rm::grow_on_rooms(g, rooms);
  double lawn = 0, tussock = 0;
  for (const gr::Tri& t : g.cover.tris) {
    // Every triangle faces up (the floors), so none is above the rooms' floors by much.
    const double u[3] = {t.b[0] - t.a[0], t.b[1] - t.a[1], t.b[2] - t.a[2]};
    const double v[3] = {t.c[0] - t.a[0], t.c[1] - t.a[1], t.c[2] - t.a[2]};
    const double ny = u[2] * v[0] - u[0] * v[2];
    const double len = 2 * gr::tri_area(t);
    CHECK(len > 0 && std::fabs(ny) / len >= rm::kFloorUp - 1e-4);
    (t.kind == 0 ? lawn : tussock) += gr::tri_area(t);
  }
  // The hall (10 by 8) and the gallery (8 by 6) are flagstone floors, flat at y = 0.
  CHECK_MSG(lawn >= 128 - 1e-3, "lawn " + std::to_string(lawn));
  CHECK_MSG(tussock > 50, "tussock " + std::to_string(tussock));
}

TEST(a_cloned_map_grows_on_the_texture_its_map_surfaces_as_grass) {
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_bsp(br::sample_fixture(), br::Family::v29, &bytes, &error), error);
  br::Map map;
  CHECK_MSG(br::read_bsp(bytes, &map, &error), error);
  std::string text;
  for (const br::Texture& t : map.textures) {
    text += "texture " + t.name + " " + t.name + "_recipe\n";
    if (t.name == "tile") text += "surface tile grass lawn\n";
  }
  rm::TextureMap tm;
  CHECK_MSG(rm::parse_texture_map(text, &tm, &error), error);
  rm::Level level;
  rm::CloneReport rep;
  CHECK_MSG(rm::clone_map(map, tm, rm::CloneOptions{}, &level, &rep, &error), error);
  rm::LevelGrass g = rm::level_grass(level);
  CHECK(g.faults.empty() && g.cover.kinds.size() == 1 && g.cover.kinds[0].name == "lawn");
  size_t tile = level.materials.size();
  for (size_t m = 0; m < level.materials.size(); ++m) {
    if (g.kind[m] == 0) tile = m;
  }
  CHECK(tile < level.materials.size());
  const rm::BrushMesh mesh = rm::build_brushes(level, 1);
  rm::grow_on_brushes(g, mesh);
  // The side room's floor alone: one height, and the area its tile faces up over.
  CHECK(!g.cover.tris.empty());
  const float y = g.cover.tris[0].a[1];
  for (const gr::Tri& t : g.cover.tris) CHECK(t.a[1] == y && t.b[1] == y && t.c[1] == y);
  const double want = up_area(mesh, tile);
  CHECK_MSG(want > 50 && std::fabs(cover_area(g.cover) - want) < 1e-3 * want,
            "grass " + std::to_string(cover_area(g.cover)) + ", tile " + std::to_string(want));
}

TEST(the_same_level_grows_the_same_blades_held_by_the_golden) {
  const rm::LevelGrass a = vault_grass();
  const rm::LevelGrass b = vault_grass();
  const std::vector<gr::Blade> blades = gr::scatter_all(a.cover);
  CHECK(blades_digest(blades) == blades_digest(gr::scatter_all(b.cover)));
  rm::Level halls = load("plugins/rooms/levels/halls.gelv.txt");
  halls.materials[material_named(halls, "gravel")].surface = "grass tussock";
  rm::LevelGrass h1 = rm::level_grass(halls);
  rm::LevelGrass h4 = rm::level_grass(halls);
  rm::grow_on_rooms(h1, rm::build_level(halls, 1));
  rm::grow_on_rooms(h4, rm::build_level(halls, 4));
  const std::vector<gr::Blade> hb = gr::scatter_all(h1.cover);
  CHECK(blades_digest(hb) == blades_digest(gr::scatter_all(h4.cover)));

  char line[160];
  std::string table = "# level tris blades digest (rooms_grass_test, GE_GOLDEN_WRITE=1)\n";
  std::snprintf(line, sizeof line, "vault %zu %zu %016llx\n", a.cover.tris.size(), blades.size(),
                static_cast<unsigned long long>(blades_digest(blades)));
  table += line;
  std::snprintf(line, sizeof line, "halls-gravel %zu %zu %016llx\n", h1.cover.tris.size(),
                hb.size(), static_cast<unsigned long long>(blades_digest(hb)));
  table += line;
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream(ge::test::root_path(kGolden)) << table;
    std::printf("  wrote %s\n", kGolden);
    return;
  }
  CHECK_MSG(table == read_text(kGolden),
            "the level's grass moved; rewrite " + std::string(kGolden) +
                " with GE_GOLDEN_WRITE=1 if that was meant, and say why in the PR");
}
