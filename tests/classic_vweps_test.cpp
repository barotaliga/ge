// Visible weapons (plugins/classic/vweps.hpp): the stuffed line read as a server sends it, the
// names' files, the fixed list's weapon by view model, and the stand-ins made from a made figure
// (a body, a gun out in front along +x in `stand1` and tucked in `axstnd1`, an axe the other way
// round): the body keeps only itself, `w_shot` and `w_axe` are the figure's own pieces, and a
// pickup model rides the gun with its rear end on the gun's.
//
// ge:summary Tests the vwep line, paths, the fixed list and the stand-ins from a figure
// ge:keywords test vwep read_vwep_line vwep_path vwep_of_view_model figure_parts vwep_stand_in
// ge:skill classic testing
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "classic/vweps.hpp"

namespace cl = ge::classic;
namespace mo = ge::morph;

namespace {

// A tetrahedron's four points at (x, y, z), `size` across (0 folds it to a point).
void tetra(std::vector<float>& p, float x, float y, float z, float size) {
  const float pts[4][3] = {{0, 0, 0}, {size, 0, 0}, {0, size, 0}, {0, 0, size}};
  for (const auto& q : pts) p.insert(p.end(), {x + q[0], y + q[1], z + q[2]});
}

// Body (0..4: five points), gun (5..8), axe (9..12); frames stand1 and axstnd1.
mo::Model figure() {
  mo::Model m;
  for (const uint32_t b : {0u, 5u, 9u})
    m.indices.insert(m.indices.end(), {b, b + 1, b + 2, b, b + 2, b + 3, b, b + 3, b + 1});
  m.indices.insert(m.indices.end(), {1, 2, 4});
  m.st.assign(size_t{13} * 2, 0.5f);
  mo::Frame stand, axe;
  stand.name = "stand1";
  axe.name = "axstnd1";
  for (mo::Frame* f : {&stand, &axe}) {
    tetra(f->positions, -5, -5, -20, 20);
    f->positions.insert(f->positions.end(), {0, 0, 10});
  }
  stand.positions.insert(stand.positions.end(),  // the gun out in front, long along +x
                         {4, 0, 10, 20, 0, 10, 4, 2, 10, 4, 0, 12});
  tetra(axe.positions, -8, 0, 10, 2);  // tucked away
  tetra(stand.positions, 0, 0, 0, 0.5f);
  tetra(axe.positions, 6, 0, 0, 14);  // the axe out
  m.frames = {stand, axe};
  return m;
}

mo::Model pickup() {
  mo::Model m;
  m.indices = {0, 1, 2};
  m.st.assign(6, 0.0f);
  mo::Frame f;
  f.positions = {-10, -1, 24, 30, -1, 24, -10, 1, 30};
  m.frames = {f};
  m.flags = 1;
  return m;
}

}  // namespace

TEST(the_line_is_read_as_a_server_sends_it) {
  std::vector<std::string> names;
  CHECK(cl::read_vwep_line("//vwep vwplayer w_axe w_shot -\n", names));
  CHECK(names.size() == 4 && names[0] == "vwplayer" && names[3] == "-");
  CHECK(!cl::read_vwep_line("//vwepx a\n", names));
  CHECK(!cl::read_vwep_line("say //vwep\n", names));
  CHECK(names.size() == 4);                                       // untouched
  CHECK(cl::read_vwep_line("//vwep\n", names) && names.empty());  // none: the feature off
  std::string many = "//vwep";
  for (int i = 0; i < 40; i++) many += " w" + std::to_string(i);
  CHECK(cl::read_vwep_line(many, names) && names.size() == cl::kMaxVweps);
}

TEST(a_name_opens_its_file) {
  bool refused = false;
  CHECK(cl::vwep_path("w_rock") == "progs/w_rock.mdl");
  CHECK(cl::vwep_path("models/w_x.md3") == "models/w_x.md3");
  CHECK(cl::vwep_path("w_odd.mdl") == "progs/w_odd.mdl");
  CHECK(cl::vwep_path("sub/w_rock") == "sub/w_rock.mdl");
  CHECK(cl::vwep_path("-").empty());
  CHECK(cl::vwep_path("../w", &refused).empty() && refused);
  CHECK(cl::vwep_path("/w", &refused).empty() && refused);
  CHECK(!cl::vwep_path("w", &refused).empty() && !refused);
}

TEST(the_fixed_list_names_a_weapon_by_its_view_model) {
  CHECK(cl::vwep_of_view_model("progs/v_axe.mdl") == 1);
  CHECK(cl::vwep_of_view_model("progs/v_light.mdl") == 8);
  CHECK(cl::kFixedVweps[size_t(cl::vwep_of_view_model("progs/v_rock2.mdl"))] == "w_rock2");
  CHECK(cl::vwep_of_view_model("progs/v_coil.mdl") == 0);
  CHECK(cl::is_death_frame(41) && cl::is_death_frame(102) && !cl::is_death_frame(103));
}

TEST(the_figure_gives_up_its_gun_and_axe) {
  const mo::Model m = figure();
  cl::FigureParts parts;
  CHECK(cl::figure_parts(m, parts));
  CHECK(parts.piece_of[0] == 0);
  CHECK(parts.gun == parts.piece_of[5]);
  CHECK(parts.axe == parts.piece_of[9]);
  const cl::OpenModel none;
  mo::Model body, shot, axe;
  CHECK(cl::vwep_stand_in("progs/vwplayer.mdl", m, parts, none, body));
  CHECK(body.vertex_count() == 5 && body.frames.size() == 2);
  CHECK(cl::vwep_stand_in("progs/w_shot.mdl", m, parts, none, shot) && shot.vertex_count() == 4);
  CHECK(cl::vwep_stand_in("progs/w_axe.mdl", m, parts, none, axe) && axe.vertex_count() == 4);
  CHECK(axe.frames[1].max[0] == 20.0f);  // the axe's own frames: out in axstnd1
  mo::Model rock;
  CHECK(!cl::vwep_stand_in("progs/w_rock.mdl", m, parts, none, rock));
  CHECK(!cl::vwep_stand_in("progs/vwplayer.mdl", mo::Model{}, cl::FigureParts{}, none, body));
}

TEST(a_pickup_rides_the_gun_from_its_rear_end) {
  const mo::Model m = figure();
  cl::FigureParts parts;
  CHECK(cl::figure_parts(m, parts));
  const cl::OpenModel open = [](std::string_view path, mo::Model& out) {
    if (path != "progs/g_rock.mdl") return false;
    out = pickup();
    return true;
  };
  mo::Model rock;
  CHECK(cl::vwep_stand_in("progs/w_rock.mdl", m, parts, open, rock));
  CHECK(rock.frames.size() == 2 && rock.frames[0].name == "stand1");
  CHECK(rock.indices.size() == 3 && rock.flags == 0);
  // In stand1 the pickup's rear end lands on the gun's (its points' least reach along the gun).
  const std::vector<float>& p = rock.frames[0].positions;
  float least = 1e9f;
  for (size_t v = 0; v < 3; v++) least = std::min(least, p[v * 3]);
  CHECK(least > 2.0f && least < 6.0f);
  // Scaled to the hands: 40 units long on the floor, shorter held.
  const float len = std::fabs(p[3] - p[0]);
  CHECK(len > 15 && len < 30);
}
