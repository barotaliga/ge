// Visible weapons (vweps.hpp): the line, the paths, the figure's pieces and the stand-ins.
//
// ge:summary The vwep line read, the names' paths, the figure's gun and axe found, stand-ins made
// ge:keywords vwep read_vwep_line vwep_path figure_parts vwep_stand_in mount pickup-model
// ge:skill classic morph
// ge:test tests/classic_vweps_test.cpp
#include "classic/vweps.hpp"

#include <algorithm>
#include <cmath>

#include "morph/carry.hpp"

namespace ge::classic {

namespace {

namespace mo = ge::morph;

// How much smaller a pickup model is drawn in the hands: the figure's gun is about half as long as
// the pickup shotgun lying on the floor.
constexpr double kHeldScale = 0.55;

size_t frame_named(const mo::Model& m, std::string_view name) {
  for (size_t f = 0; f < m.frames.size(); f++)
    if (m.frames[f].name == name) return f;
  return m.frames.size();
}

// The diagonal of piece `vs`'s box in frame `f`.
double extent(const mo::Model& m, const std::vector<uint32_t>& vs, size_t f) {
  if (vs.empty() || f >= m.frames.size()) return 0;
  double lo[3] = {1e30, 1e30, 1e30}, hi[3] = {-1e30, -1e30, -1e30};
  for (const uint32_t v : vs)
    for (int k = 0; k < 3; k++) {
      lo[k] = std::min(lo[k], double(m.frames[f].positions[v * 3 + k]));
      hi[k] = std::max(hi[k], double(m.frames[f].positions[v * 3 + k]));
    }
  double d = 0;
  for (int k = 0; k < 3; k++) d += (hi[k] - lo[k]) * (hi[k] - lo[k]);
  return std::sqrt(d);
}

void bounds(mo::Model& m) {
  for (mo::Frame& f : m.frames) {
    f.min = {1e30f, 1e30f, 1e30f};
    f.max = {-1e30f, -1e30f, -1e30f};
    for (size_t i = 0; i + 2 < f.positions.size(); i += 3)
      for (size_t k = 0; k < 3; k++) {
        f.min[k] = std::min(f.min[k], f.positions[i + k]);
        f.max[k] = std::max(f.max[k], f.positions[i + k]);
      }
  }
}

mo::Model one_piece(const mo::Model& m, const FigureParts& p, int piece) {
  std::vector<bool> keep(p.piece_of.size());
  for (size_t v = 0; v < keep.size(); v++) keep[v] = p.piece_of[v] == piece;
  mo::Model out = mo::keep_vertices(m, keep);
  bounds(out);
  return out;
}

// Where a pickup model's first frame goes in the stand frame: its length along the figure's gun,
// scaled to the hands, its up the gun's up, its rear end (the lowest x, centred across) at the
// gun's rear end.
mo::Rigid mount_on_gun(const mo::Model& figure, const std::vector<uint32_t>& gun, size_t stand,
                       const mo::Model& held) {
  const std::vector<float>& p = figure.frames[stand].positions;
  double c[3] = {0, 0, 0};
  for (const uint32_t v : gun)
    for (int k = 0; k < 3; k++) c[k] += p[v * 3 + k];
  for (double& x : c) x /= double(gun.size());
  double cov[3][3] = {};
  for (const uint32_t v : gun)
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++) cov[i][j] += (p[v * 3 + i] - c[i]) * (p[v * 3 + j] - c[j]);
  // The gun's long axis: the covariance's leading direction, by repeated products; facing +x
  // (the figure faces +x).
  double ax[3] = {1, 0, 0};
  for (int it = 0; it < 64; it++) {
    double n[3] = {0, 0, 0};
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++) n[i] += cov[i][j] * ax[j];
    const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (len <= 0) break;
    for (int i = 0; i < 3; i++) ax[i] = n[i] / len;
  }
  if (ax[0] < 0)
    for (double& x : ax) x = -x;
  double rear = 1e30;
  for (const uint32_t v : gun) {
    double s = 0;
    for (int k = 0; k < 3; k++) s += (p[v * 3 + k] - c[k]) * ax[k];
    rear = std::min(rear, s);
  }
  double up[3] = {0, 0, 1};
  const double along = up[0] * ax[0] + up[1] * ax[1] + up[2] * ax[2];
  for (int k = 0; k < 3; k++) up[k] -= ax[k] * along;
  const double ul = std::sqrt(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
  for (double& x : up) x /= ul;
  const double side[3] = {up[1] * ax[2] - up[2] * ax[1], up[2] * ax[0] - up[0] * ax[2],
                          up[0] * ax[1] - up[1] * ax[0]};
  // The pickup's rear, centred across.
  const mo::Frame& g = held.frames[0];
  double lo[3] = {1e30, 1e30, 1e30}, hi[3] = {-1e30, -1e30, -1e30};
  for (size_t i = 0; i + 2 < g.positions.size(); i += 3)
    for (size_t k = 0; k < 3; k++) {
      lo[k] = std::min(lo[k], double(g.positions[i + k]));
      hi[k] = std::max(hi[k], double(g.positions[i + k]));
    }
  const double g0[3] = {lo[0], (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2};
  mo::Rigid m;
  for (int i = 0; i < 3; i++) {
    m.r[size_t(i) * 3] = ax[i] * kHeldScale;
    m.r[size_t(i) * 3 + 1] = side[i] * kHeldScale;
    m.r[size_t(i) * 3 + 2] = up[i] * kHeldScale;
  }
  for (int i = 0; i < 3; i++)
    m.t[i] = c[i] + ax[i] * rear -
             (m.r[size_t(i) * 3] * g0[0] + m.r[size_t(i) * 3 + 1] * g0[1] +
              m.r[size_t(i) * 3 + 2] * g0[2]);
  return m;
}

std::string_view stem_of(std::string_view path) {
  if (path.starts_with("progs/")) path.remove_prefix(6);
  if (path.ends_with(".mdl")) path.remove_suffix(4);
  return path;
}

}  // namespace

bool read_vwep_line(std::string_view text, std::vector<std::string>& names) {
  constexpr std::string_view kWord = "//vwep";
  if (!text.starts_with(kWord)) return false;
  std::string_view rest = text.substr(kWord.size());
  if (!rest.empty() && rest[0] != ' ' && rest[0] != '\t' && rest[0] != '\n' && rest[0] != '\r')
    return false;  // `//vwepx`: another word
  names.clear();
  size_t i = 0;
  while (i < rest.size() && names.size() < kMaxVweps) {
    while (i < rest.size() &&
           (rest[i] == ' ' || rest[i] == '\t' || rest[i] == '\n' || rest[i] == '\r'))
      i++;
    const size_t start = i;
    while (i < rest.size() && rest[i] != ' ' && rest[i] != '\t' && rest[i] != '\n' &&
           rest[i] != '\r')
      i++;
    if (i > start) names.emplace_back(rest.substr(start, i - start));
  }
  return true;
}

std::string vwep_path(std::string_view name, bool* refused) {
  if (refused != nullptr) *refused = false;
  if (name.empty() || name == "-") return "";
  if (name.find("..") != std::string_view::npos || name[0] == '/' || name[0] == '\\') {
    if (refused != nullptr) *refused = true;
    return "";
  }
  std::string path =
      name.find('/') != std::string_view::npos ? std::string(name) : "progs/" + std::string(name);
  if (name.find('.') == std::string_view::npos) path += ".mdl";
  return path;
}

int vwep_of_view_model(std::string_view path) {
  constexpr std::string_view kViews[] = {"v_axe",   "v_shot", "v_shot2", "v_nail",
                                         "v_nail2", "v_rock", "v_rock2", "v_light"};
  const std::string_view stem = stem_of(path);
  for (size_t i = 0; i < std::size(kViews); i++)
    if (stem == kViews[i] && path.starts_with("progs/")) return int(i) + 1;
  return 0;
}

bool figure_parts(const mo::Model& figure, FigureParts& out) {
  out = {};
  out.stand = frame_named(figure, "stand1");
  const size_t axe_frame = frame_named(figure, "axstnd1");
  if (out.stand >= figure.frames.size() || axe_frame >= figure.frames.size()) return false;
  out.piece_of = mo::pieces(figure);
  int count = 0;
  for (const int p : out.piece_of) count = std::max(count, p + 1);
  double gun_size = 0, axe_size = 0;
  for (int p = 1; p < count; p++) {
    const double s = extent(figure, mo::piece_vertices(out.piece_of, p), out.stand);
    if (s > gun_size) {
      gun_size = s;
      out.gun = p;
    }
  }
  for (int p = 1; p < count; p++) {
    if (p == out.gun) continue;
    const double s = extent(figure, mo::piece_vertices(out.piece_of, p), axe_frame);
    if (s > axe_size) {
      axe_size = s;
      out.axe = p;
    }
  }
  return out.gun > 0 && out.axe > 0;
}

bool vwep_stand_in(std::string_view path, const mo::Model& figure, const FigureParts& parts,
                   const OpenModel& open, mo::Model& out, std::string* skin_of) {
  const std::string_view stem = stem_of(path);
  if (!path.starts_with("progs/") || parts.gun <= 0) return false;
  if (skin_of != nullptr) *skin_of = "progs/player.mdl";
  if (stem == "vwplayer") {
    out = one_piece(figure, parts, 0);  // the body: piece 0, every other cut
    return true;
  }
  if (stem == "w_axe" && parts.axe > 0) {
    out = one_piece(figure, parts, parts.axe);
    return true;
  }
  if (stem == "w_shot") {
    out = one_piece(figure, parts, parts.gun);
    return true;
  }
  if (!stem.starts_with("w_")) return false;
  // The pickup a weapon lies about as: `g_<stem>`, but the double-barrelled shotgun's is `g_shot`
  // (the single one has none: it is the figure's own gun).
  const std::string pickup_path =
      stem == "w_shot2" ? "progs/g_shot.mdl" : "progs/g_" + std::string(stem.substr(2)) + ".mdl";
  mo::Model pickup;
  if (!open || !open(pickup_path, pickup) || pickup.frames.empty()) return false;
  if (skin_of != nullptr) *skin_of = pickup_path;
  const std::vector<uint32_t> gun = mo::piece_vertices(parts.piece_of, parts.gun);
  const mo::Rigid mount = mount_on_gun(figure, gun, parts.stand, pickup);
  out = mo::carried(pickup, figure, gun, parts.stand, mount);
  out.flags = 0;  // the pickup's spin and trail are not the held weapon's
  out.synced = true;
  return true;
}

}  // namespace ge::classic
