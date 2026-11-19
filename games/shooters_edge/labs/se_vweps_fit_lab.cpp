// LAB `se-vweps-fit`: THE CARRIED WEAPONS CLOSE UP, IN THE HANDS. The visible weapons' stand-ins
// (plugins/classic/vweps.hpp) drawn on the CPU, orthographic and evenly lit, so how each weapon
// sits in the figure's hands can be judged without the map's light: the figure's own gun beside
// each carried one, the skins as painted, flat colours with the hands marked and the figure's own
// gun as a ghost (where the hands were made to hold), the triangles' edges, and the views from
// the side, above and in front.
//
// Without `--game` the made figure of `tests/classic_vweps_test.cpp` is measured (the digest).
// With `--game=<base>/<folder>` the pack's own figure and pickups are drawn: one sheet for every
// weapon, one for the rocket launcher through the figure's frames, and the fit measured (how far
// the weapon's grip line is from the hands, how long it is against the figure's own gun).
//
// ge:summary Lab: each carried weapon close up in the figure's hands, lit evenly, wired, flat
// ge:keywords lab se-vweps-fit vwep visible-weapons stand-in close-up wireframe hands fit
// ge:keywords orthographic software-raster even-light ghost carried-weapon
// ge:skill shooter labs morph
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "classic/vweps.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "morph/carry.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace mo = ge::morph;
namespace vw = ge::classic;

// The weapons in the fixed list's order (1..8) and their listed names.
const char* const kNames[] = {"w_axe",   "w_shot", "w_shot2", "w_nail",
                              "w_nail2", "w_rock", "w_rock2", "w_light"};
const char* const kWeapons[] = {
    "AXE",           "SHOTGUN",          "SUPER SHOTGUN",   "NAILGUN",
    "SUPER NAILGUN", "GRENADE LAUNCHER", "ROCKET LAUNCHER", "LIGHTNING GUN"};

constexpr gl::Rgba kBack{34, 36, 42};
constexpr gl::Rgba kBodyFlat{150, 150, 156};
constexpr gl::Rgba kRearHand{225, 30, 30};   // the hand on the grip
constexpr gl::Rgba kFrontHand{255, 95, 70};  // the hand the weapon rests on
constexpr gl::Rgba kWeaponFlat{240, 150, 40};
constexpr gl::Rgba kGhost{70, 210, 240};

// An orthographic look: `right` and `up` across the panel, `fwd` into it; `centre` at the
// panel's middle, `scale` pixels a unit.
struct Ortho {
  std::array<double, 3> right, up, fwd;
  std::array<double, 3> centre{};
  double scale = 6;
};

Ortho side_view(const double c[3], double scale) {  // from the figure's right (-y): +x right
  return {{1, 0, 0}, {0, 0, 1}, {0, 1, 0}, {c[0], c[1], c[2]}, scale};
}
Ortho other_side(const double c[3], double scale) {  // from its left (+y): +x left
  return {{-1, 0, 0}, {0, 0, 1}, {0, -1, 0}, {c[0], c[1], c[2]}, scale};
}
Ortho top_view(const double c[3], double scale) {  // from above: +x right, +y up the panel
  return {{1, 0, 0}, {0, 1, 0}, {0, 0, -1}, {c[0], c[1], c[2]}, scale};
}
Ortho front_view(const double c[3], double scale) {  // from in front (+x), looking back at it
  return {{0, 1, 0}, {0, 0, 1}, {-1, 0, 0}, {c[0], c[1], c[2]}, scale};
}

double dot3(const std::array<double, 3>& a, const double* b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

enum class Fill : uint8_t { skin, flat, none };

// What one panel draws: a model's frame, filled with its skin or a flat colour (a vertex marked
// in `hand` takes the hand's colour), and its edges in `wire` when its alpha is not 0.
struct Item {
  const mo::Model* model = nullptr;
  size_t frame = 0;
  Fill fill = Fill::skin;
  gl::Rgba flat = kBodyFlat;
  gl::Rgba wire{0, 0, 0, 0};
  const std::vector<uint8_t>* hand = nullptr;
};

// A panel w x h: every item's triangles depth-tested, lit by an even light (a wide soft key from
// the front above and a fill from behind, so no side goes dark), then the edges over them; drawn
// twice the size and filtered down.
gl::Picture draw_panel(int w, int h, const Ortho& o, const std::vector<Item>& items,
                       std::span<const uint8_t> palette) {
  constexpr int kSs = 2;
  const int W = w * kSs, H = h * kSs;
  gl::Picture pic = gl::create_picture(W, H, kBack);
  std::vector<double> depth(size_t(W) * size_t(H), 1e30);
  const double key[3] = {0.45, -0.55, 0.70}, back[3] = {-0.6, 0.6, 0.2};
  const auto unit = [](double* v) {
    const double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 0)
      for (int k = 0; k < 3; k++) v[k] /= l;
  };
  double kl[3] = {key[0], key[1], key[2]}, bl[3] = {back[0], back[1], back[2]};
  unit(kl);
  unit(bl);
  const double s = o.scale * kSs;
  const auto project = [&](const float* p, double& x, double& y, double& z) {
    const double d[3] = {p[0] - o.centre[0], p[1] - o.centre[1], p[2] - o.centre[2]};
    x = W / 2.0 + dot3(o.right, d) * s;
    y = H / 2.0 - dot3(o.up, d) * s;
    z = dot3(o.fwd, d);
  };
  for (const Item& it : items) {
    if (it.model == nullptr || it.fill == Fill::none || it.frame >= it.model->frames.size())
      continue;
    const mo::Model& m = *it.model;
    const std::vector<float>& P = m.frames[it.frame].positions;
    const mo::Skin* skin = m.skins.empty() ? nullptr : &m.skins[0];
    for (size_t t = 0; t + 2 < m.indices.size(); t += 3) {
      const uint32_t v[3] = {m.indices[t], m.indices[t + 1], m.indices[t + 2]};
      double x[3], y[3], z[3];
      for (int k = 0; k < 3; k++) project(&P[size_t(v[k]) * 3], x[k], y[k], z[k]);
      const float* a = &P[size_t(v[0]) * 3];
      const float* b = &P[size_t(v[1]) * 3];
      const float* c = &P[size_t(v[2]) * 3];
      const double e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
      const double e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
      double n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                     e1[0] * e2[1] - e1[1] * e2[0]};
      unit(n);
      // Either face lit as if it faced the eye: a pickup's winding is not trusted here.
      const double toward = -dot3(o.fwd, n);
      if (toward < 0)
        for (double& q : n) q = -q;
      const double light =
          std::min(1.25, 0.50 + 0.60 * std::max(0.0, n[0] * kl[0] + n[1] * kl[1] + n[2] * kl[2]) +
                             0.25 * std::max(0.0, n[0] * bl[0] + n[1] * bl[1] + n[2] * bl[2]));
      const double area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
      if (std::fabs(area) < 1e-9) continue;
      const int x0 = std::max(0, int(std::floor(std::min({x[0], x[1], x[2]}))));
      const int x1 = std::min(W - 1, int(std::ceil(std::max({x[0], x[1], x[2]}))));
      const int y0 = std::max(0, int(std::floor(std::min({y[0], y[1], y[2]}))));
      const int y1 = std::min(H - 1, int(std::ceil(std::max({y[0], y[1], y[2]}))));
      for (int py = y0; py <= y1; py++)
        for (int px = x0; px <= x1; px++) {
          const double cx = px + 0.5, cy = py + 0.5;
          const double w0 = ((x[1] - cx) * (y[2] - cy) - (x[2] - cx) * (y[1] - cy)) / area;
          const double w1 = ((x[2] - cx) * (y[0] - cy) - (x[0] - cx) * (y[2] - cy)) / area;
          const double w2 = 1 - w0 - w1;
          if (w0 < 0 || w1 < 0 || w2 < 0) continue;
          const double zz = w0 * z[0] + w1 * z[1] + w2 * z[2];
          double& dz = depth[size_t(py) * size_t(W) + size_t(px)];
          if (zz >= dz) continue;
          dz = zz;
          double rgb[3] = {double(it.flat.r), double(it.flat.g), double(it.flat.b)};
          if (it.fill == Fill::flat && it.hand != nullptr) {
            // A triangle with every corner in one hand is that hand's.
            const uint8_t h0 = (*it.hand)[v[0]];
            if (h0 != 0 && (*it.hand)[v[1]] == h0 && (*it.hand)[v[2]] == h0) {
              const gl::Rgba hc = h0 == 1 ? kRearHand : kFrontHand;
              rgb[0] = hc.r, rgb[1] = hc.g, rgb[2] = hc.b;
            }
          }
          if (it.fill == Fill::skin && skin != nullptr && palette.size() >= 768) {
            const double u = w0 * m.st[size_t(v[0]) * 2] + w1 * m.st[size_t(v[1]) * 2] +
                             w2 * m.st[size_t(v[2]) * 2];
            const double tv = w0 * m.st[size_t(v[0]) * 2 + 1] + w1 * m.st[size_t(v[1]) * 2 + 1] +
                              w2 * m.st[size_t(v[2]) * 2 + 1];
            const int sx = std::clamp(int(u * skin->width), 0, skin->width - 1);
            const int sy = std::clamp(int(tv * skin->height), 0, skin->height - 1);
            const uint8_t idx = skin->indices[size_t(sy) * size_t(skin->width) + size_t(sx)];
            for (int k = 0; k < 3; k++) rgb[k] = palette[size_t(idx) * 3 + size_t(k)];
          }
          uint8_t* q = &pic.rgba[(size_t(py) * size_t(W) + size_t(px)) * 4];
          for (int k = 0; k < 3; k++) q[k] = uint8_t(std::clamp(rgb[k] * light, 0.0, 255.0));
        }
    }
  }
  for (const Item& it : items) {  // the edges over everything, seen through
    if (it.model == nullptr || it.wire.a == 0 || it.frame >= it.model->frames.size()) continue;
    const mo::Model& m = *it.model;
    const std::vector<float>& P = m.frames[it.frame].positions;
    for (size_t t = 0; t + 2 < m.indices.size(); t += 3)
      for (int k = 0; k < 3; k++) {
        double xa, ya, za, xb, yb, zb;
        project(&P[size_t(m.indices[t + size_t(k)]) * 3], xa, ya, za);
        project(&P[size_t(m.indices[t + size_t((k + 1) % 3)]) * 3], xb, yb, zb);
        gl::draw_line(pic, xa, ya, xb, yb, it.wire, 1.0);
      }
  }
  return gl::resized(pic, w, h);
}

void label(gl::Picture& p, const std::string& text) {
  gl::fill_rect(p, 0, 0, gl::text_width(text) + 8, gl::kGlyphH + 6, {0, 0, 0, 200});
  gl::draw_text(p, 4, 3, text, {235, 235, 235});
}

// A sheet of panels, `cols` across, each w x h with a gap.
gl::Picture sheet(const std::vector<gl::Picture>& panels, int cols, int w, int h) {
  const int rows = (int(panels.size()) + cols - 1) / cols;
  constexpr int kGap = 4;
  gl::Picture out =
      gl::create_picture(cols * (w + kGap) + kGap, rows * (h + kGap) + kGap, {16, 16, 20});
  for (size_t i = 0; i < panels.size(); i++)
    gl::blit(out, panels[i], kGap + int(i % size_t(cols)) * (w + kGap),
             kGap + int(i / size_t(cols)) * (h + kGap));
  return out;
}

size_t frame_named(const mo::Model& m, std::string_view name) {
  for (size_t f = 0; f < m.frames.size(); f++)
    if (m.frames[f].name == name) return f;
  return 0;
}

// The middle of piece `vs` in frame `f`.
void centre_of(const mo::Model& m, const std::vector<uint32_t>& vs, size_t f, double c[3]) {
  c[0] = c[1] = c[2] = 0;
  if (vs.empty()) return;
  for (const uint32_t v : vs)
    for (size_t k = 0; k < 3; k++) c[k] += m.frames[f].positions[size_t(v) * 3 + k];
  for (int k = 0; k < 3; k++) c[k] /= double(vs.size());
}

// The figure's hands (classic/vweps.hpp's FigureParts) as flags on the body's vertices (the body
// keeps the figure's piece-0 vertices in order): 1 the rear hand, 2 the front one.
std::vector<uint8_t> hands_of(const vw::FigureParts& parts) {
  std::vector<uint8_t> on_figure(parts.piece_of.size(), 0), out;
  for (const uint32_t v : parts.rear_hand) on_figure[v] = 1;
  for (const uint32_t v : parts.front_hand) on_figure[v] = 2;
  for (size_t v = 0; v < parts.piece_of.size(); v++)
    if (parts.piece_of[v] == 0) out.push_back(on_figure[v]);
  return out;
}

void made(gl::Lab& lab) {
  // The made figure's parts and the made pickup's rear: the stand-in's rules without a pack.
  mo::Model m;
  for (const uint32_t b : {0u, 5u, 9u})
    m.indices.insert(m.indices.end(), {b, b + 1, b + 2, b, b + 2, b + 3, b, b + 3, b + 1});
  m.st.assign(size_t{13} * 2, 0.5f);
  mo::Frame stand, axe;
  stand.name = "stand1";
  axe.name = "axstnd1";
  const auto tetra = [](std::vector<float>& p, float x, float y, float z, float size) {
    const float pts[4][3] = {{0, 0, 0}, {size, 0, 0}, {0, size, 0}, {0, 0, size}};
    for (const auto& q : pts) p.insert(p.end(), {x + q[0], y + q[1], z + q[2]});
  };
  for (mo::Frame* f : {&stand, &axe}) {
    tetra(f->positions, -5, -5, -20, 20);
    f->positions.insert(f->positions.end(), {0, 0, 10});
  }
  stand.positions.insert(stand.positions.end(), {4, 0, 10, 20, 0, 10, 4, 2, 10, 4, 0, 12});
  tetra(axe.positions, -8, 0, 10, 2);
  tetra(stand.positions, 0, 0, 0, 0.5f);
  tetra(axe.positions, 6, 0, 0, 14);
  m.frames = {stand, axe};
  vw::FigureParts parts;
  lab.metric("made.parts-found", vw::figure_parts(m, parts) ? 1 : 0, "count");
  const vw::OpenModel open = [](std::string_view path, mo::Model& out) {
    if (path != "progs/g_rock.mdl") return false;
    out = {};
    out.indices = {0, 1, 2};
    out.st.assign(6, 0.0f);
    mo::Frame f;
    f.positions = {-10, -1, 24, 30, -1, 24, -10, 1, 30};
    out.frames = {f};
    return true;
  };
  mo::Model rock;
  const bool ok = vw::vwep_stand_in("progs/w_rock.mdl", m, parts, open, rock);
  lab.metric("made.w_rock.made", ok ? 1 : 0, "count");
  if (ok) {
    lab.metric("made.w_rock.rear-x", std::round(double(rock.frames[0].min[0]) * 100) / 100,
               "units");
    lab.metric("made.w_rock.length",
               std::round(double(rock.frames[0].max[0] - rock.frames[0].min[0]) * 100) / 100,
               "units");
  }
}

void real(gl::Lab& lab, const std::string& folder) {
  std::string error;
  cl::SearchPath files;
  const size_t cut = folder.find_last_of('/');
  const std::string base = cut == std::string::npos ? "." : folder.substr(0, cut);
  const std::string game_dir = cut == std::string::npos ? folder : folder.substr(cut + 1);
  if (!cl::open_game_dir(base, game_dir, files, error)) {
    lab.fail("se-vweps-fit: " + error, "Pass --game=<base>/<game folder>. Skill: shooter.");
    return;
  }
  std::vector<uint8_t> store;
  const std::span<const uint8_t> palette = cl::find_file(files, "gfx/palette.lmp", store);
  const cl::GameAdapter& game = cl::classic_adapter();
  const auto open_morph = [&](std::string_view path, mo::Model& out) {
    cl::OpenedModel o;
    std::string why;
    if (!game.open_model(game, files, std::string(path), o, why) || o.form != cl::ModelForm::morph)
      return false;
    out = std::move(o.morph);
    return true;
  };
  mo::Model figure;
  vw::FigureParts parts;
  if (!open_morph("progs/player.mdl", figure) || !vw::figure_parts(figure, parts)) {
    lab.fail("se-vweps-fit: the pack's progs/player.mdl did not open as a figure with a gun",
             "Pass a --game whose packs hold the first game's player. Skill: shooter.");
    return;
  }
  mo::Model body, plain_gun;
  vw::vwep_stand_in("progs/vwplayer.mdl", figure, parts, open_morph, body);
  vw::vwep_stand_in("progs/w_shot.mdl", figure, parts, open_morph, plain_gun);
  const size_t stand = parts.stand, axstnd = frame_named(figure, "axstnd1");
  const std::vector<uint8_t> hand = hands_of(parts);
  lab.metric("real.rear-hand.vertices", double(parts.rear_hand.size()), "count");
  lab.metric("real.front-hand.vertices", double(parts.front_hand.size()), "count");
  const std::vector<uint32_t> gun_vs = mo::piece_vertices(parts.piece_of, parts.gun);
  double gun_c[3];
  centre_of(figure, gun_vs, stand, gun_c);
  const double plain_len = double(plain_gun.frames[stand].max[0] - plain_gun.frames[stand].min[0]);
  lab.metric("real.plain-gun.length", std::round(plain_len * 10) / 10, "units");

  const int pw = lab.arg_nums("panel").size() > 0 ? int(lab.arg_nums("panel")[0]) : 320;
  const int ph = lab.arg_nums("panel").size() > 1 ? int(lab.arg_nums("panel")[1]) : 240;
  const double scale = ph / 34.0;  // about 34 units of the figure top to bottom
  // The figure alone, its gun cut away and its hands in red, from every side and in four frames.
  {
    std::vector<gl::Picture> ps;
    const double mid[3] = {2, 0, 2};
    const double big = ph / 60.0;
    const std::vector<Item> skin = {{&body, stand, Fill::skin}};
    const std::vector<Item> flat = {{&body, stand, Fill::flat, kBodyFlat, {0, 0, 0, 0}, &hand}};
    struct P {
      const std::vector<Item>* items;
      Ortho o;
      const char* what;
    };
    const P views[] = {{&skin, side_view(mid, big), "STAND1, FROM THE RIGHT"},
                       {&flat, side_view(mid, big), "HANDS: FROM THE RIGHT"},
                       {&flat, other_side(mid, big), "FROM THE LEFT"},
                       {&flat, front_view(mid, big), "FROM IN FRONT"},
                       {&flat, top_view(mid, big), "FROM ABOVE"}};
    for (const P& v : views) {
      gl::Picture pic = draw_panel(pw, ph, v.o, *v.items, palette);
      label(pic, v.what);
      ps.push_back(std::move(pic));
    }
    for (const std::vector<uint32_t>* h : {&parts.rear_hand, &parts.front_hand}) {
      double hc[3];
      centre_of(figure, *h, stand, hc);
      const bool rear = h == &parts.rear_hand;
      for (const Ortho& o :
           {side_view(hc, ph / 14.0), other_side(hc, ph / 14.0), front_view(hc, ph / 14.0)}) {
        gl::Picture pic = draw_panel(pw, ph, o, flat, palette);
        label(pic, rear ? "THE REAR HAND, CLOSE" : "THE FRONT HAND, CLOSE");
        ps.push_back(std::move(pic));
      }
    }
    for (const char* fname : {"rockrun3", "rockatt2", "nailatt1", "deatha5"}) {
      const size_t f = frame_named(figure, fname);
      gl::Picture pic =
          draw_panel(pw, ph, side_view(mid, big),
                     {{&body, f, Fill::flat, kBodyFlat, {0, 0, 0, 0}, &hand}}, palette);
      label(pic, std::string(fname) + ": HANDS");
      ps.push_back(std::move(pic));
    }
    lab.shot({"figure-hands",
              "THE FIGURE ALONE, ITS HANDS IN RED",
              "The player figure with its gun cut away. The hands are found as the two ends of the "
              "body's surface nearest the gun (the others are the head and the feet), each the "
              "vertices within 5.5 units of its end: dark red the rear hand on the grip, light red "
              "the front one.",
              "both hands red and nothing else: no forearm, no chest",
              {{"game", lab.arg("game")}}},
             sheet(ps, 3, pw, ph), gl::Det::measured);
  }
  std::vector<gl::Picture> panels;
  std::vector<mo::Model> weapons(std::size(kNames));
  for (size_t i = 0; i < std::size(kNames); i++) {
    mo::Model& wpn = weapons[i];
    const bool ok = vw::vwep_stand_in(vw::vwep_path(kNames[i]), figure, parts, open_morph, wpn);
    const std::string key = std::string("real.") + kNames[i];
    lab.metric(key + ".made", ok ? 1 : 0, "count");
    if (!ok) continue;
    const size_t f = i == 0 ? axstnd : stand;
    const mo::Frame& fr = wpn.frames[f];
    lab.metric(key + ".length", std::round(double(fr.max[0] - fr.min[0]) * 10) / 10, "units");
    lab.metric(key + ".rear-x", std::round(double(fr.min[0]) * 10) / 10, "units");
    lab.metric(key + ".mid-z", std::round(double(fr.min[2] + fr.max[2]) * 5) / 10, "units");
    double c[3];
    if (i == 0) {
      const std::vector<uint32_t> axe_vs = mo::piece_vertices(parts.piece_of, parts.axe);
      centre_of(figure, axe_vs, f, c);
    } else {
      for (int k = 0; k < 3; k++) c[k] = gun_c[k];
    }
    c[0] += 4;  // a little forward: the barrels' ends in the panel
    const std::vector<Item> plain = {{&figure, f, Fill::skin}};
    const std::vector<Item> carried = {{&body, f, Fill::skin}, {&wpn, f, Fill::skin}};
    const std::vector<Item> flat = {{&body, f, Fill::flat, kBodyFlat, {0, 0, 0, 0}, &hand},
                                    {&wpn, f, Fill::flat, kWeaponFlat},
                                    {&plain_gun, f, Fill::none, kGhost, kGhost}};
    const std::vector<Item> wire = {{&body, f, Fill::none, kBodyFlat, {150, 150, 156, 110}},
                                    {&wpn, f, Fill::none, kWeaponFlat, kWeaponFlat},
                                    {&plain_gun, f, Fill::none, kGhost, kGhost}};
    const std::string name = kWeapons[i];
    struct P {
      const std::vector<Item>* items;
      Ortho o;
      std::string what;
    };
    const P row[] = {{&plain, side_view(c, scale), name + ": THE FIGURE'S OWN"},
                     {&carried, side_view(c, scale), "CARRIED, FROM THE RIGHT"},
                     {&carried, other_side(c, scale), "CARRIED, FROM THE LEFT"},
                     {&flat, side_view(c, scale), "HANDS AND GHOST"},
                     {&wire, side_view(c, scale), "EDGES"},
                     {&flat, top_view(c, scale), "FROM ABOVE"},
                     {&flat, front_view(c, scale), "FROM IN FRONT"}};
    for (const P& p : row) {
      gl::Picture pic = draw_panel(pw, ph, p.o, *p.items, palette);
      label(pic, p.what);
      panels.push_back(std::move(pic));
    }
  }
  if (panels.size() == std::size(kNames) * 7) {
    lab.shot({"close-ups",
              "EVERY CARRIED WEAPON CLOSE UP IN THE HANDS",
              "Each row one weapon in the figure's standing frame, drawn on the CPU with an even "
              "light: the figure with its own gun, the carried weapon from the left and the right, "
              "flat colours with the hands in red (the grip's darker, the resting one lighter) and "
              "the figure's own gun as a cyan ghost, the "
              "edges, and the same from above and in front.",
              "each weapon's rear grip inside the dark red hand, its underside on the light red "
              "one; no weapon sunk "
              "in the body",
              {{"game", lab.arg("game")}}},
             sheet(panels, 7, pw, ph), gl::Det::measured);
  }
  // The rocket launcher through the figure's frames, carried and flat with its ghost.
  std::vector<gl::Picture> frames;
  const mo::Model& rock = weapons[6];
  for (const char* fname : {"stand1", "rockrun3", "rockatt2", "nailatt1", "pain3", "deatha5"}) {
    const size_t f = frame_named(figure, fname);
    if (rock.frames.size() <= f) continue;
    double c[3];
    centre_of(figure, gun_vs, f, c);
    const std::vector<Item> carried = {{&body, f, Fill::skin}, {&rock, f, Fill::skin}};
    const std::vector<Item> flat = {{&body, f, Fill::flat, kBodyFlat, {0, 0, 0, 0}, &hand},
                                    {&rock, f, Fill::flat, kWeaponFlat},
                                    {&plain_gun, f, Fill::none, kGhost, kGhost}};
    gl::Picture a = draw_panel(pw, ph, side_view(c, scale), carried, palette);
    label(a, std::string(fname));
    gl::Picture b = draw_panel(pw, ph, side_view(c, scale), flat, palette);
    label(b, std::string(fname) + ": FLAT");
    frames.push_back(std::move(a));
    frames.push_back(std::move(b));
  }
  if (!frames.empty())
    lab.shot({"rocket-frames",
              "THE ROCKET LAUNCHER THROUGH THE FIGURE'S FRAMES, CLOSE UP",
              "Standing, running, firing, firing the nailgun, pain and a death; each carried and "
              "flat with the hands and the figure's own gun as a ghost.",
              "the launcher stays in the hands in every frame",
              {{"game", lab.arg("game")}}},
             sheet(frames, 4, pw, ph), gl::Det::measured);
}

void run(gl::Lab& lab) {
  made(lab);
  if (!lab.arg("game").empty()) real(lab, lab.arg("game"));
}

const gl::LabSpec kSpec{
    "se-vweps-fit",
    gl::LabKind::picture,
    "game:shooters_edge",
    "plugins/classic/vweps (the stand-ins' fit in the hands), drawn on the CPU",
    "shooter",
    "Does each carried weapon sit in the figure's hands, its grip where the figure's own gun's "
    "is?",
    {{"game", "", "a game folder (<base>/<folder>) whose packs hold the figure and pickups"},
     {"panel", "320,240", "one panel's width and height, px"}},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
