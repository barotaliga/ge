// The real stars: the table decoded, the cube map built and uploaded, and the bake's parsing and
// encoding.
//
// ge:summary The real stars' table, cube map and texture, and the catalogue's parse and encoding
// ge:keywords render stars star-catalogue star-map parse encode decode base64 cube-cell
// ge:skill render
// ge:test tests/render_stars_test.cpp
#include "render/stars.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "core/libm/libm.hpp"

namespace ge::render {

extern const char* const kStarTable;  // render/star_data.cpp: generated

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
// B-V where the catalogue gives none: a sun-like star's.
constexpr double kNoColour = 0.65;
// A star's bytes in the table.
constexpr size_t kBytes = 6;

int sextet(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

// A fixed-width field (columns `from`..`to`, counted from 1 as the catalogue's description
// counts them) as a number; false when it is blank or not one.
bool field(std::string_view line, size_t from, size_t to, double& out) {
  if (line.size() < to) return false;
  std::string s(line.substr(from - 1, to - from + 1));
  const size_t a = s.find_first_not_of(' ');
  if (a == std::string::npos) return false;
  char* end = nullptr;
  out = std::strtod(s.c_str() + a, &end);
  return end != s.c_str() + a;
}

void put16(std::string& bytes, int v) {
  const auto u = static_cast<uint16_t>(static_cast<int16_t>(v));
  bytes.push_back(static_cast<char>(u & 0xff));
  bytes.push_back(static_cast<char>(u >> 8));
}

int get16(const std::vector<uint8_t>& b, size_t i) {
  return static_cast<int16_t>(static_cast<uint16_t>(b[i] | (b[i + 1] << 8)));
}

}  // namespace

bool parse_catalogue_line(std::string_view line, Star& out) {
  double rh = 0, rm = 0, rs = 0, dd = 0, dm = 0, ds = 0, v = 0, bv = 0;
  if (!field(line, 76, 77, rh) || !field(line, 78, 79, rm) || !field(line, 80, 83, rs) ||
      !field(line, 85, 86, dd) || !field(line, 87, 88, dm) || !field(line, 89, 90, ds) ||
      !field(line, 103, 107, v)) {
    return false;
  }
  const double sign = line[83] == '-' ? -1 : 1;
  out.ra = (rh + rm / 60 + rs / 3600) / 24 * 2 * kPi;
  out.dec = sign * (dd + dm / 60 + ds / 3600) / 180 * kPi;
  out.mag = v;
  out.bv = field(line, 110, 114, bv) ? bv : kNoColour;
  return true;
}

std::string encode_stars(const std::vector<Star>& stars) {
  std::string bytes;
  bytes.reserve(stars.size() * kBytes);
  for (const Star& s : stars) {
    double turn = s.ra / (2 * kPi);
    turn -= std::floor(turn);
    put16(bytes, static_cast<int>(std::lround(turn * 65536)) & 0xffff);
    put16(bytes, static_cast<int>(std::lround(s.dec / (kPi / 2) * 32767)));
    bytes.push_back(static_cast<char>(std::lround(std::clamp(s.mag + 2, 0.0, 12.75) * 20)));
    bytes.push_back(static_cast<char>(std::lround(std::clamp(s.bv + 0.5, 0.0, 3.6) * 70)));
  }
  std::string out;
  for (size_t i = 0; i < bytes.size(); i += 3) {
    const uint32_t a = static_cast<uint8_t>(bytes[i]);
    const uint32_t b = i + 1 < bytes.size() ? static_cast<uint8_t>(bytes[i + 1]) : 0;
    const uint32_t c = i + 2 < bytes.size() ? static_cast<uint8_t>(bytes[i + 2]) : 0;
    const uint32_t n = (a << 16) | (b << 8) | c;
    out.push_back(kAlphabet[(n >> 18) & 63]);
    out.push_back(kAlphabet[(n >> 12) & 63]);
    out.push_back(i + 1 < bytes.size() ? kAlphabet[(n >> 6) & 63] : '=');
    out.push_back(i + 2 < bytes.size() ? kAlphabet[n & 63] : '=');
  }
  return out;
}

bool decode_stars(std::string_view base64, std::vector<Star>& out) {
  std::vector<uint8_t> b;
  b.reserve(base64.size() * 3 / 4);
  uint32_t acc = 0;
  int bits = 0;
  for (char c : base64) {
    if (c == '=') break;
    const int s = sextet(c);
    if (s < 0) return false;
    acc = (acc << 6) | static_cast<uint32_t>(s);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      b.push_back(static_cast<uint8_t>((acc >> bits) & 0xff));
    }
  }
  if (b.size() % kBytes != 0) return false;
  out.clear();
  out.reserve(b.size() / kBytes);
  for (size_t i = 0; i < b.size(); i += kBytes) {
    Star s;
    s.ra = static_cast<uint16_t>(get16(b, i)) / 65536.0 * 2 * kPi;
    s.dec = get16(b, i + 2) / 32767.0 * (kPi / 2);
    s.mag = b[i + 4] / 20.0 - 2;
    s.bv = b[i + 5] / 70.0 - 0.5;
    out.push_back(s);
  }
  return true;
}

const std::vector<Star>& star_catalogue() {
  static const std::vector<Star> stars = [] {
    std::vector<Star> s;
    decode_stars(kStarTable, s);
    return s;
  }();
  return stars;
}

void star_direction(const Star& s, double out[3]) {
  const double cd = core::libm::cos(s.dec);
  out[0] = cd * core::libm::cos(s.ra);
  out[1] = cd * core::libm::sin(s.ra);
  out[2] = core::libm::sin(s.dec);
}

CubeCell cube_cell(const double d[3]) {
  // render/sky.wgsl's `stars`, step for step.
  const double ax = std::fabs(d[0]), ay = std::fabs(d[1]), az = std::fabs(d[2]);
  int face = 0;
  double u = d[1] / ax, v = d[2] / ax;
  if (ay >= ax && ay >= az) {
    face = 1;
    u = d[0] / ay;
    v = d[2] / ay;
  } else if (az >= ax && az >= ay) {
    face = 2;
    u = d[0] / az;
    v = d[1] / az;
  }
  if (d[face] < 0) face += 3;
  const double gu = (u * 0.5 + 0.5) * kStarCells, gv = (v * 0.5 + 0.5) * kStarCells;
  CubeCell c;
  c.face = face;
  c.col = std::min(static_cast<int>(std::floor(gu)), kStarCells - 1);
  c.row = std::min(static_cast<int>(std::floor(gv)), kStarCells - 1);
  c.u = gu - c.col;
  c.v = gv - c.row;
  return c;
}

StarMap star_map(const std::vector<Star>& stars) {
  constexpr size_t kWidth = size_t{6} * kStarCells;
  constexpr size_t kLayer = kWidth * kStarCells;
  StarMap m;
  m.cells.assign(kLayer * kStarSlots, StarCell{});
  // Brightest first, so a full cell keeps its brightest.
  std::vector<size_t> order(stars.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::stable_sort(order.begin(), order.end(),
                   [&](size_t a, size_t b) { return stars[a].mag < stars[b].mag; });
  for (size_t i : order) {
    double d[3];
    star_direction(stars[i], d);
    const CubeCell c = cube_cell(d);
    const size_t at = static_cast<size_t>(c.row) * kWidth +
                      static_cast<size_t>(c.face) * kStarCells + static_cast<size_t>(c.col);
    bool kept = false;
    for (size_t slot = 0; slot < kStarSlots && !kept; ++slot) {
      StarCell& cell = m.cells[slot * kLayer + at];
      if (cell.mag >= kNoStar) {
        cell = {static_cast<float>(c.u), static_cast<float>(c.v), static_cast<float>(stars[i].mag),
                static_cast<float>(stars[i].bv)};
        kept = true;
      }
    }
    if (!kept) ++m.dropped;
  }
  return m;
}

StarTexture create_star_texture(gpu::Device& device) {
  const StarMap m = star_map(star_catalogue());
  const auto w = static_cast<uint32_t>(6 * kStarCells), h = static_cast<uint32_t>(kStarCells);
  StarTexture t;
  wgpu::TextureDescriptor td{};
  td.label = "sky.stars";
  td.size = {w, h, static_cast<uint32_t>(kStarSlots)};
  td.format = wgpu::TextureFormat::RGBA8Unorm;
  td.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  t.texture = device.device.CreateTexture(&td);
  std::vector<uint8_t> bytes(m.cells.size() * 4);
  auto byte = [](double v) { return static_cast<uint8_t>(std::lround(std::clamp(v, 0.0, 255.0))); };
  for (size_t i = 0; i < m.cells.size(); ++i) {
    const StarCell& c = m.cells[i];
    bytes[i * 4] = byte(c.u * 255);
    bytes[i * 4 + 1] = byte(c.v * 255);
    bytes[i * 4 + 2] = byte((c.mag + 2) * 20);
    bytes[i * 4 + 3] = byte((c.bv + 0.5) * 70);
  }
  wgpu::TexelCopyTextureInfo dst{};
  dst.texture = t.texture;
  wgpu::TexelCopyBufferLayout layout{};
  layout.bytesPerRow = w * 4;
  layout.rowsPerImage = h;
  const wgpu::Extent3D size{w, h, static_cast<uint32_t>(kStarSlots)};
  device.queue.WriteTexture(&dst, bytes.data(), bytes.size(), &layout, &size);
  wgpu::TextureViewDescriptor vd{};
  vd.dimension = wgpu::TextureViewDimension::e2DArray;
  t.view = t.texture.CreateView(&vd);
  return t;
}

}  // namespace ge::render
