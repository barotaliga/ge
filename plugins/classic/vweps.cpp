// Visible weapons (vweps.hpp): the line, the paths, the figure's pieces and the stand-ins.
//
// ge:summary The vwep line read, the names' paths, the figure's gun and axe found, stand-ins made
// ge:keywords vwep read_vwep_line vwep_path figure_parts vwep_stand_in mount pickup-model
// ge:skill classic morph
// ge:test tests/classic_vweps_test.cpp
#include "classic/vweps.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "morph/carry.hpp"

namespace ge::classic {

namespace {

namespace mo = ge::morph;

// How long a pickup is drawn in the hands: the figure's own gun's length for a pickup of
// kTypicalPickup units, a longer one longer by the square root of its length over that, so every
// weapon reaches from the rear hand to the front one and still a launcher outgrows a nailgun
// (measured in the `se-vweps-fit` lab: the pickups run 24 to 35 units, the figure's gun 23
// long and about 27 along its tilted axis).
constexpr double kTypicalPickup = 30;
constexpr double kHeldLength = 0.8;  // of the gun's reach along its axis, hand to muzzle

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
// scaled to the hands (kHeldLength of the gun's reach, by the root of its own length), its up the
// gun's up, its rear end at the gun's rear end, its underside on the gun's (the grips in the
// hands) and centred across on its points' mean.
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
  // The pickup's rear, on the line through its points' mean (its handle and sights pull a box's
  // middle off the barrels), and its length.
  const mo::Frame& g = held.frames[0];
  double lo = 1e30, hi = -1e30, floor = 1e30, mean[3] = {0, 0, 0};
  const size_t n = g.positions.size() / 3;
  for (size_t i = 0; i < n; i++) {
    lo = std::min(lo, double(g.positions[i * 3]));
    hi = std::max(hi, double(g.positions[i * 3]));
    floor = std::min(floor, double(g.positions[i * 3 + 2]));
    for (size_t k = 0; k < 3; k++) mean[k] += g.positions[i * 3 + k] / double(n);
  }
  // The gun's front end along its axis and its underside (where the hands close round it).
  double front = -1e30, under = 1e30;
  for (const uint32_t v : gun) {
    double s = 0, u = 0;
    for (int k = 0; k < 3; k++) {
      s += (p[v * 3 + k] - c[k]) * ax[k];
      u += (p[v * 3 + k] - c[k]) * up[k];
    }
    front = std::max(front, s);
    under = std::min(under, u);
  }
  const double length = std::max(1e-3, hi - lo);
  const double scale = kHeldLength * (front - rear) * std::sqrt(length / kTypicalPickup) / length;
  const double g0[3] = {lo, mean[1], floor};
  mo::Rigid m;
  for (int i = 0; i < 3; i++) {
    m.r[size_t(i) * 3] = ax[i] * scale;
    m.r[size_t(i) * 3 + 1] = side[i] * scale;
    m.r[size_t(i) * 3 + 2] = up[i] * scale;
  }
  for (int i = 0; i < 3; i++)
    m.t[i] = c[i] + ax[i] * rear + up[i] * under -
             (m.r[size_t(i) * 3] * g0[0] + m.r[size_t(i) * 3 + 1] * g0[1] +
              m.r[size_t(i) * 3 + 2] * g0[2]);
  return m;
}

// A unit vector's three numbers; 0 for a zero one.
void norm3(double v[3]) {
  const double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (l > 0)
    for (int k = 0; k < 3; k++) v[k] /= l;
}

void centroid(const mo::Model& m, const std::vector<uint32_t>& vs, size_t f, double c[3]) {
  c[0] = c[1] = c[2] = 0;
  for (const uint32_t v : vs)
    for (size_t k = 0; k < 3; k++) c[k] += m.frames[f].positions[size_t(v) * 3 + k];
  for (int k = 0; k < 3; k++) c[k] /= double(std::max<size_t>(1, vs.size()));
}

// The rotation taking basis (a1, a2, a3) onto (b1, b2, b3), times `scale`, with `from` moved onto
// `to`.
mo::Rigid basis_onto(const double a[3][3], const double b[3][3], double scale, const double from[3],
                     const double to[3]) {
  mo::Rigid m;
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++) {
      double r = 0;
      for (int k = 0; k < 3; k++) r += b[k][i] * a[k][j];
      m.r[size_t(i) * 3 + size_t(j)] = r * scale;
    }
  for (int i = 0; i < 3; i++)
    m.t[i] = to[i] - (m.r[size_t(i) * 3] * from[0] + m.r[size_t(i) * 3 + 1] * from[1] +
                      m.r[size_t(i) * 3 + 2] * from[2]);
  return m;
}

// The pickup's length (x) along `aim` and its up (z) as near `up` as square to it, times `scale`,
// with `from` moved onto `to`.
mo::Rigid basis_along(const double aim[3], const double up[3], double scale, const double from[3],
                      const double to[3]) {
  double a[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
  double b[3][3];
  for (int k = 0; k < 3; k++) {
    b[0][k] = aim[k];
    b[2][k] = up[k];
  }
  const double d = b[2][0] * b[0][0] + b[2][1] * b[0][1] + b[2][2] * b[0][2];
  for (int k = 0; k < 3; k++) b[2][k] -= b[0][k] * d;
  norm3(b[2]);
  b[1][0] = b[2][1] * b[0][2] - b[2][2] * b[0][1];
  b[1][1] = b[2][2] * b[0][0] - b[2][0] * b[0][2];
  b[1][2] = b[2][0] * b[0][1] - b[2][1] * b[0][0];
  return basis_onto(a, b, scale, from, to);
}

// Where a hand holds a pickup and where it rests on the other, in the pickup's own units: the rear
// grip (the rearmost piece hanging under the rest: taller than long, reaching the bottom), else a
// point on the underside a third of the way back; and the underside's height `ahead` units in
// front of the grip.
struct Holds;
double underside(const Holds& h, const mo::Model& held, double x);

struct Holds {
  double grip[3] = {0, 0, 0};
  std::vector<int> piece_of;
  std::vector<bool> is_grip;  // a piece's
  double floor = 0, lo = 0, hi = 0, mid_y = 0;
};

Holds holds_of(const mo::Model& held) {
  Holds h;
  const mo::Frame& g = held.frames[0];
  h.piece_of = mo::pieces(held);
  int count = 0;
  for (const int q : h.piece_of) count = std::max(count, q + 1);
  h.is_grip.assign(size_t(count), false);
  const size_t n = g.positions.size() / 3;
  h.lo = h.floor = 1e30;
  h.hi = -1e30;
  for (size_t i = 0; i < n; i++) {
    h.lo = std::min(h.lo, double(g.positions[i * 3]));
    h.hi = std::max(h.hi, double(g.positions[i * 3]));
    h.floor = std::min(h.floor, double(g.positions[i * 3 + 2]));
    h.mid_y += g.positions[i * 3 + 1] / double(n);
  }
  double best = 1e30;
  for (int q = 0; q < count; q++) {
    double lo[3] = {1e30, 1e30, 1e30}, hi[3] = {-1e30, -1e30, -1e30}, c[3] = {0, 0, 0};
    const std::vector<uint32_t> vs = mo::piece_vertices(h.piece_of, q);
    for (const uint32_t v : vs)
      for (size_t k = 0; k < 3; k++) {
        const double x = g.positions[size_t(v) * 3 + k];
        lo[k] = std::min(lo[k], x);
        hi[k] = std::max(hi[k], x);
        c[k] += x / double(vs.size());
      }
    const double tall = hi[2] - lo[2], len = hi[0] - lo[0];
    if (tall < 4 || tall < 1.2 * len || lo[2] > h.floor + 2.5) continue;
    h.is_grip[size_t(q)] = true;
    if (c[0] < best) {
      best = c[0];
      for (int k = 0; k < 3; k++) h.grip[k] = c[k];
    }
  }
  if (best > 1e29) {  // no grip: a third of the way back, on the underside there
    h.grip[0] = h.lo + (h.hi - h.lo) / 3;
    h.grip[1] = h.mid_y;
    h.grip[2] = underside(h, held, h.grip[0]);
  }
  return h;
}

// The underside's height at `x`: the lowest point at `x` of any triangle edge of the pieces that
// are not grips, the pickup's bottom when none spans it.
double underside(const Holds& h, const mo::Model& held, double x) {
  const std::vector<float>& P = held.frames[0].positions;
  double z = 1e30;
  for (size_t t = 0; t + 2 < held.indices.size(); t += 3) {
    const uint32_t v0 = held.indices[t];
    if (size_t(h.piece_of[v0]) < h.is_grip.size() && h.is_grip[size_t(h.piece_of[v0])]) continue;
    for (size_t k = 0; k < 3; k++) {
      const size_t a = held.indices[t + k], b = held.indices[t + (k + 1) % 3];
      const double xa = P[a * 3], xb = P[b * 3], za = P[a * 3 + 2], zb = P[b * 3 + 2];
      if (x < std::min(xa, xb) || x > std::max(xa, xb)) continue;
      z = std::min(z, xb == xa ? std::min(za, zb) : za + (zb - za) * (x - xa) / (xb - xa));
    }
  }
  return z > 1e29 ? h.floor : z;
}

constexpr double kRestAbove = 0.0;    // the underside rests no lower than the front hand's middle
constexpr double kHandsApart = 0.35;  // a frame whose hands are this much nearer or farther apart
                                      // than standing follows the gun instead,
constexpr double kHandsAlong = 0.15;  // as does one whose hands' line turns this much farther
                                      // (in cosine) from the gun's than standing

// `held` placed in every frame of `figure`: the grip in the rear hand, the underside on the front
// hand, the up the figure's gun's up in that frame, `stand_mount`'s scale; a frame whose hands
// came apart moves with the gun from the standing placement.
std::vector<mo::Rigid> in_hands(const mo::Model& figure, const FigureParts& parts,
                                const std::vector<uint32_t>& gun, const mo::Model& held,
                                const mo::Rigid& stand_mount) {
  const double scale =
      std::sqrt(stand_mount.r[0] * stand_mount.r[0] + stand_mount.r[3] * stand_mount.r[3] +
                stand_mount.r[6] * stand_mount.r[6]);
  const double up0[3] = {stand_mount.r[2] / scale, stand_mount.r[5] / scale,
                         stand_mount.r[8] / scale};
  const double axis0[3] = {stand_mount.r[0] / scale, stand_mount.r[3] / scale,
                           stand_mount.r[6] / scale};
  const Holds h = holds_of(held);
  const auto place = [&](size_t f, mo::Rigid& out, double& along) {
    double pr[3], pf[3];
    centroid(figure, parts.rear_hand, f, pr);
    centroid(figure, parts.front_hand, f, pf);
    const mo::Rigid gm = mo::fit_rigid(figure, gun, parts.stand, f);
    double up[3];
    for (int i = 0; i < 3; i++)
      up[i] = gm.r[size_t(i) * 3] * up0[0] + gm.r[size_t(i) * 3 + 1] * up0[1] +
              gm.r[size_t(i) * 3 + 2] * up0[2];
    double apart = 0;
    for (int k = 0; k < 3; k++) apart += (pf[k] - pr[k]) * (pf[k] - pr[k]);
    apart = std::sqrt(apart);
    // The weapon points the way the figure's own gun points in this frame, its grip in the rear
    // hand; where that would sink its underside below the front hand's middle, it tips up about
    // the grip just enough to rest on it.
    double hands[3], aim[3];
    for (int k = 0; k < 3; k++) hands[k] = pf[k] - pr[k];
    norm3(hands);
    for (int i = 0; i < 3; i++)
      aim[i] = gm.r[size_t(i) * 3] * axis0[0] + gm.r[size_t(i) * 3 + 1] * axis0[1] +
               gm.r[size_t(i) * 3 + 2] * axis0[2];
    for (int pass = 0;; pass++) {
      out = basis_along(aim, up, scale, h.grip, pr);
      if (pass == 2) break;
      // The front hand in the weapon's own units, and the underside's height over it there.
      double q[3], loc[3];
      for (int k = 0; k < 3; k++) q[k] = pf[k] - out.t[k];
      for (int j = 0; j < 3; j++)
        loc[j] =
            (out.r[size_t(j)] * q[0] + out.r[3 + size_t(j)] * q[1] + out.r[6 + size_t(j)] * q[2]) /
            (scale * scale);
      const double gap = (underside(h, held, loc[0]) - loc[2]) * scale;
      const double reach = (loc[0] - h.grip[0]) * scale;
      if (gap >= kRestAbove || reach <= 1) break;
      for (int k = 0; k < 3; k++) aim[k] = aim[k] * reach + up[k] * (kRestAbove - gap);
      norm3(aim);
    }
    // How far the hands' line strays from the gun's: the cosine between them.
    along = 0;
    for (int i = 0; i < 3; i++) {
      double g = 0;
      for (int j = 0; j < 3; j++) g += gm.r[size_t(i) * 3 + size_t(j)] * axis0[j];
      along += g * hands[i];
    }
    return apart;
  };
  std::vector<mo::Rigid> out(figure.frames.size());
  mo::Rigid standing;
  double along0 = 0, along = 0;
  const double apart0 = place(parts.stand, standing, along0);
  for (size_t f = 0; f < figure.frames.size(); f++) {
    const double apart = place(f, out[f], along);
    if (along < along0 - kHandsAlong || std::fabs(apart - apart0) > kHandsApart * apart0)
      out[f] = mo::compose(mo::fit_rigid(figure, gun, parts.stand, f), standing);
  }
  return out;
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

// The hands: the two ends of the body's surface (from its middle along the surface) nearest the
// gun, each with the body's vertices within kHandReach of it along the surface.
void find_hands(const mo::Model& figure, FigureParts& p) {
  const std::vector<uint32_t> body = mo::piece_vertices(p.piece_of, 0);
  const std::vector<uint32_t> gun = mo::piece_vertices(p.piece_of, p.gun);
  if (body.empty() || gun.empty()) return;
  double gc[3];
  centroid(figure, gun, p.stand, gc);
  const std::vector<float>& P = figure.frames[p.stand].positions;
  const auto far2 = [&](uint32_t v, const double c[3]) {
    double d = 0;
    for (size_t k = 0; k < 3; k++)
      d += (P[size_t(v) * 3 + k] - c[k]) * (P[size_t(v) * 3 + k] - c[k]);
    return d;
  };
  // The root: the body's middle along its surface (the vertex whose farthest reach is least),
  // so every limb's end is an end from it.
  uint32_t root = body[0];
  double least = std::numeric_limits<double>::infinity();
  for (const uint32_t v : body) {
    const std::vector<double> d = mo::surface_distance(figure, p.stand, v);
    double most = 0;
    for (const uint32_t w : body) most = std::max(most, d[w]);
    if (most < least) {
      least = most;
      root = v;
    }
  }
  std::vector<uint32_t> ends = mo::surface_ends(figure, p.stand, root);
  if (ends.size() < 2) return;
  std::stable_sort(ends.begin(), ends.end(),
                   [&](uint32_t a, uint32_t b) { return far2(a, gc) < far2(b, gc); });
  // The nearest end is one hand; the other is the next end not on the same hand (a hand's box
  // may have more than one corner that is an end).
  const std::vector<double> d0 = mo::surface_distance(figure, p.stand, ends[0]);
  size_t second = 1;
  while (second < ends.size() && d0[ends[second]] <= 2 * kHandReach) second++;
  if (second >= ends.size()) return;
  const std::vector<double> d1 = mo::surface_distance(figure, p.stand, ends[second]);
  std::vector<uint32_t> hand[2];
  for (const uint32_t v : body) {
    if (d0[v] <= kHandReach) hand[0].push_back(v);
    if (d1[v] <= kHandReach) hand[1].push_back(v);
  }
  double c0[3], c1[3];
  centroid(figure, hand[0], p.stand, c0);
  centroid(figure, hand[1], p.stand, c1);
  const bool first_rear = c0[0] < c1[0];  // the figure faces +x
  p.rear_hand = std::move(hand[first_rear ? 0 : 1]);
  p.front_hand = std::move(hand[first_rear ? 1 : 0]);
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
  if (out.gun > 0) find_hands(figure, out);
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
  if (parts.rear_hand.empty() || parts.front_hand.empty()) {
    out = mo::carried(pickup, figure, gun, parts.stand, mount);
  } else {
    const std::vector<mo::Rigid> placed = in_hands(figure, parts, gun, pickup, mount);
    out = mo::carried_by(pickup, figure, placed);
  }
  out.flags = 0;  // the pickup's spin and trail are not the held weapon's
  out.synced = true;
  return true;
}

}  // namespace ge::classic
