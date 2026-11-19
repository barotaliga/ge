// A figure's pieces, the cut and the carried model (carry.hpp): the union of vertices, the
// quaternion fit by Jacobi sweeps over the 4x4 symmetric matrix.
//
// ge:summary The pieces by union-find, the rigid fit by quaternion, the carried model's frames
// ge:keywords pieces union-find jacobi eigen quaternion fit_rigid carried keep_vertices
// ge:skill morph
// ge:test tests/morph_carry_test.cpp
#include "morph/carry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>

namespace ge::morph {

namespace {

uint32_t root(std::vector<uint32_t>& up, uint32_t v) {
  while (up[v] != v) {
    up[v] = up[up[v]];
    v = up[v];
  }
  return v;
}

void join(std::vector<uint32_t>& up, uint32_t a, uint32_t b) {
  a = root(up, a);
  b = root(up, b);
  if (a != b) up[std::max(a, b)] = std::min(a, b);
}

// The eigenvector of the largest eigenvalue of the symmetric 4x4 `a` (destroyed), by Jacobi.
void largest_eigenvector(double a[4][4], double out[4]) {
  double v[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
  for (int sweep = 0; sweep < 50; sweep++) {
    double off = 0;
    for (int p = 0; p < 4; p++)
      for (int q = p + 1; q < 4; q++) off += a[p][q] * a[p][q];
    if (off < 1e-24) break;
    for (int p = 0; p < 4; p++)
      for (int q = p + 1; q < 4; q++) {
        if (std::fabs(a[p][q]) < 1e-300) continue;
        const double theta = (a[q][q] - a[p][p]) / (2 * a[p][q]);
        const double t =
            (theta >= 0 ? 1.0 : -1.0) / (std::fabs(theta) + std::sqrt(theta * theta + 1));
        const double c = 1 / std::sqrt(t * t + 1), s = t * c;
        for (int k = 0; k < 4; k++) {  // columns p and q
          const double akp = a[k][p], akq = a[k][q];
          a[k][p] = c * akp - s * akq;
          a[k][q] = s * akp + c * akq;
        }
        for (int k = 0; k < 4; k++) {  // rows p and q
          const double apk = a[p][k], aqk = a[q][k];
          a[p][k] = c * apk - s * aqk;
          a[q][k] = s * apk + c * aqk;
        }
        for (int k = 0; k < 4; k++) {
          const double vkp = v[k][p], vkq = v[k][q];
          v[k][p] = c * vkp - s * vkq;
          v[k][q] = s * vkp + c * vkq;
        }
      }
  }
  int best = 0;
  for (int i = 1; i < 4; i++)
    if (a[i][i] > a[best][best]) best = i;
  for (int k = 0; k < 4; k++) out[k] = v[k][best];
}

}  // namespace

Rigid compose(const Rigid& a, const Rigid& b) {
  Rigid o;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      double s = 0;
      for (int k = 0; k < 3; k++) s += a.r[size_t(i) * 3 + k] * b.r[k * 3 + j];
      o.r[size_t(i) * 3 + j] = s;
    }
    double s = a.t[i];
    for (int k = 0; k < 3; k++) s += a.r[size_t(i) * 3 + k] * b.t[k];
    o.t[i] = s;
  }
  return o;
}

void apply(const Rigid& m, const float in[3], float out[3]) {
  for (int i = 0; i < 3; i++)
    out[i] = static_cast<float>(m.r[size_t(i) * 3] * in[0] + m.r[size_t(i) * 3 + 1] * in[1] +
                                m.r[size_t(i) * 3 + 2] * in[2] + m.t[i]);
}

std::vector<int> pieces(const Model& m) {
  const uint32_t n = m.vertex_count();
  std::vector<uint32_t> up(n);
  std::iota(up.begin(), up.end(), 0u);
  for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
    join(up, m.indices[i], m.indices[i + 1]);
    join(up, m.indices[i], m.indices[i + 2]);
  }
  if (m.source_vertex.size() == n) {  // a seam's copies of one file vertex are one piece
    std::vector<uint32_t> first(n, ~0u);
    for (uint32_t v = 0; v < n; v++) {
      const int s = m.source_vertex[v];
      if (s < 0 || static_cast<uint32_t>(s) >= n) continue;
      if (first[uint32_t(s)] == ~0u)
        first[uint32_t(s)] = v;
      else
        join(up, first[uint32_t(s)], v);
    }
  }
  // Number the roots by size, the largest first; a tie by the lower root.
  std::vector<uint32_t> size(n, 0);
  for (uint32_t v = 0; v < n; v++) size[root(up, v)]++;
  std::vector<uint32_t> roots;
  for (uint32_t v = 0; v < n; v++)
    if (root(up, v) == v) roots.push_back(v);
  std::stable_sort(roots.begin(), roots.end(),
                   [&](uint32_t a, uint32_t b) { return size[a] > size[b]; });
  std::vector<int> number(n, -1), out(n, 0);
  for (size_t i = 0; i < roots.size(); i++) number[roots[i]] = static_cast<int>(i);
  for (uint32_t v = 0; v < n; v++) out[v] = number[root(up, v)];
  return out;
}

std::vector<uint32_t> piece_vertices(std::span<const int> piece_of, int piece) {
  std::vector<uint32_t> out;
  for (size_t v = 0; v < piece_of.size(); v++)
    if (piece_of[v] == piece) out.push_back(static_cast<uint32_t>(v));
  return out;
}

Model keep_vertices(const Model& m, const std::vector<bool>& keep) {
  const uint32_t n = m.vertex_count();
  std::vector<uint32_t> to(n, ~0u);
  std::vector<bool> used(n, false);
  Model out = m;
  out.indices.clear();
  for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
    const uint32_t a = m.indices[i], b = m.indices[i + 1], c = m.indices[i + 2];
    if (a >= n || b >= n || c >= n || a >= keep.size() || b >= keep.size() || c >= keep.size() ||
        !keep[a] || !keep[b] || !keep[c])
      continue;
    used[a] = used[b] = used[c] = true;
    out.indices.insert(out.indices.end(), {a, b, c});
  }
  uint32_t next = 0;
  for (uint32_t v = 0; v < n; v++)
    if (used[v]) to[v] = next++;
  for (uint32_t& i : out.indices) i = to[i];
  out.st.clear();
  out.source_vertex.clear();
  for (uint32_t v = 0; v < n; v++) {
    if (!used[v]) continue;
    out.st.push_back(m.st[size_t{v} * 2]);
    out.st.push_back(m.st[v * 2 + 1]);
    if (v < m.source_vertex.size()) out.source_vertex.push_back(m.source_vertex[v]);
  }
  for (size_t f = 0; f < m.frames.size(); f++) {
    Frame& fr = out.frames[f];
    fr.positions.clear();
    for (uint32_t v = 0; v < n; v++)
      if (used[v])
        for (int k = 0; k < 3; k++) fr.positions.push_back(m.frames[f].positions[v * 3 + k]);
  }
  return out;
}

Rigid fit_rigid(const Model& m, std::span<const uint32_t> of, size_t from, size_t to) {
  Rigid out;
  if (of.size() < 3 || from >= m.frames.size() || to >= m.frames.size()) return out;
  const std::vector<float>& a = m.frames[from].positions;
  const std::vector<float>& b = m.frames[to].positions;
  double ca[3] = {0, 0, 0}, cb[3] = {0, 0, 0};
  for (const uint32_t v : of)
    for (int k = 0; k < 3; k++) {
      ca[k] += a[v * 3 + k];
      cb[k] += b[v * 3 + k];
    }
  for (int k = 0; k < 3; k++) {
    ca[k] /= double(of.size());
    cb[k] /= double(of.size());
  }
  double s[3][3] = {};  // sum of (a - ca)(b - cb)^T
  for (const uint32_t v : of)
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++) s[i][j] += (a[v * 3 + i] - ca[i]) * (b[v * 3 + j] - cb[j]);
  double n[4][4] = {
      {s[0][0] + s[1][1] + s[2][2], s[1][2] - s[2][1], s[2][0] - s[0][2], s[0][1] - s[1][0]},
      {s[1][2] - s[2][1], s[0][0] - s[1][1] - s[2][2], s[0][1] + s[1][0], s[2][0] + s[0][2]},
      {s[2][0] - s[0][2], s[0][1] + s[1][0], -s[0][0] + s[1][1] - s[2][2], s[1][2] + s[2][1]},
      {s[0][1] - s[1][0], s[2][0] + s[0][2], s[1][2] + s[2][1], -s[0][0] - s[1][1] + s[2][2]}};
  double q[4];
  largest_eigenvector(n, q);
  const double len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  if (len <= 0) return out;
  const double w = q[0] / len, x = q[1] / len, y = q[2] / len, z = q[3] / len;
  const double r[9] = {
      w * w + x * x - y * y - z * z, 2 * (x * y - w * z),           2 * (x * z + w * y),
      2 * (x * y + w * z),           w * w - x * x + y * y - z * z, 2 * (y * z - w * x),
      2 * (x * z - w * y),           2 * (y * z + w * x),           w * w - x * x - y * y + z * z};
  std::copy(r, r + 9, out.r);
  for (int i = 0; i < 3; i++)
    out.t[i] = cb[i] - (r[size_t(i) * 3] * ca[0] + r[size_t(i) * 3 + 1] * ca[1] +
                        r[size_t(i) * 3 + 2] * ca[2]);
  return out;
}

namespace {

// One frame of `held` moved by `to`, named and timed as the figure's frame `f`.
Frame placed_frame(const Model& held, const Model& figure, size_t f, const Rigid& to) {
  const std::vector<float>& rest = held.frames[0].positions;
  const uint32_t n = held.vertex_count();
  Frame fr;
  fr.name = figure.frames[f].name;
  fr.interval = figure.frames[f].interval;
  fr.group = figure.frames[f].group;
  fr.positions.resize(size_t(n) * 3);
  fr.min = {1e30f, 1e30f, 1e30f};
  fr.max = {-1e30f, -1e30f, -1e30f};
  for (uint32_t v = 0; v < n; v++) {
    float* p = &fr.positions[size_t(v) * 3];
    apply(to, &rest[size_t(v) * 3], p);
    for (int k = 0; k < 3; k++) {
      fr.min[size_t(k)] = std::min(fr.min[size_t(k)], p[k]);
      fr.max[size_t(k)] = std::max(fr.max[size_t(k)], p[k]);
    }
  }
  return fr;
}

// Each vertex's neighbours: the triangles' edges, with their lengths in `frame`, and a seam's
// copies of one file vertex at no distance.
std::vector<std::vector<std::pair<uint32_t, double>>> neighbours(const Model& m, size_t frame) {
  const uint32_t n = m.vertex_count();
  std::vector<std::vector<std::pair<uint32_t, double>>> out(n);
  const std::vector<float>& p = m.frames[frame].positions;
  const auto edge = [&](uint32_t a, uint32_t b, double len) {
    out[a].push_back({b, len});
    out[b].push_back({a, len});
  };
  for (size_t i = 0; i + 2 < m.indices.size(); i += 3)
    for (size_t k = 0; k < 3; k++) {
      const uint32_t a = m.indices[i + k], b = m.indices[i + (k + 1) % 3];
      double d = 0;
      for (size_t c = 0; c < 3; c++) {
        const double x = double(p[size_t(a) * 3 + c]) - p[size_t(b) * 3 + c];
        d += x * x;
      }
      edge(a, b, std::sqrt(d));
    }
  if (m.source_vertex.size() == n) {
    std::vector<uint32_t> first(n, ~0u);
    for (uint32_t v = 0; v < n; v++) {
      const int s = m.source_vertex[v];
      if (s < 0 || static_cast<uint32_t>(s) >= n) continue;
      if (first[uint32_t(s)] == ~0u)
        first[uint32_t(s)] = v;
      else
        edge(first[uint32_t(s)], v, 0);
    }
  }
  return out;
}

}  // namespace

std::vector<double> surface_distance(const Model& m, size_t frame, uint32_t from) {
  const uint32_t n = m.vertex_count();
  std::vector<double> dist(n, std::numeric_limits<double>::infinity());
  if (from >= n || frame >= m.frames.size()) return dist;
  const auto next = neighbours(m, frame);
  using Item = std::pair<double, uint32_t>;
  std::priority_queue<Item, std::vector<Item>, std::greater<>> open;
  dist[from] = 0;
  open.push({0, from});
  while (!open.empty()) {
    const auto [d, v] = open.top();
    open.pop();
    if (d > dist[v]) continue;
    for (const auto& [w, len] : next[v])
      if (d + len < dist[w]) {
        dist[w] = d + len;
        open.push({dist[w], w});
      }
  }
  return dist;
}

std::vector<uint32_t> surface_ends(const Model& m, size_t frame, uint32_t from) {
  std::vector<uint32_t> out;
  const uint32_t n = m.vertex_count();
  if (from >= n || frame >= m.frames.size()) return out;
  const std::vector<double> dist = surface_distance(m, frame, from);
  const auto next = neighbours(m, frame);
  std::vector<bool> taken(n, false);  // a file vertex already given by a lower copy
  for (uint32_t v = 0; v < n; v++) {
    if (!std::isfinite(dist[v]) || dist[v] <= 0) continue;
    const int s = m.source_vertex.size() == n ? m.source_vertex[v] : int(v);
    if (s >= 0 && uint32_t(s) < n && taken[uint32_t(s)]) continue;
    bool end = true;
    for (const auto& [w, len] : next[v]) end = end && dist[w] <= dist[v];
    if (!end) continue;
    if (s >= 0 && uint32_t(s) < n) taken[uint32_t(s)] = true;
    out.push_back(v);
  }
  std::stable_sort(out.begin(), out.end(),
                   [&](uint32_t a, uint32_t b) { return dist[a] > dist[b]; });
  return out;
}

Model carried_by(const Model& held, const Model& figure, std::span<const Rigid> per_frame) {
  Model out = held;
  out.frames.clear();
  if (held.frames.empty() || per_frame.empty()) return out;
  for (size_t f = 0; f < figure.frames.size(); f++)
    out.frames.push_back(
        placed_frame(held, figure, f, per_frame[std::min(f, per_frame.size() - 1)]));
  out.radius = std::max(out.radius, figure.radius);
  return out;
}

Model carried(const Model& held, const Model& figure, std::span<const uint32_t> grip,
              size_t reference, const Rigid& mount) {
  Model out = held;
  out.frames.clear();
  if (held.frames.empty()) return out;
  for (size_t f = 0; f < figure.frames.size(); f++)
    out.frames.push_back(
        placed_frame(held, figure, f, compose(fit_rigid(figure, grip, reference, f), mount)));
  out.radius = std::max(out.radius, figure.radius);
  return out;
}

}  // namespace ge::morph
