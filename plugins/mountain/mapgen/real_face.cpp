// The definitions behind real_face.hpp.
//
// ge:summary R25 a real face's grid decoded and encoded, its bicubic sample and its globe
// ge:keywords real-face decode-face-grid encode-face-grid base64 varint zigzag
// ge:skill mountain
// ge:test tests/mountain_real_face_test.cpp
#include "mountain/mapgen/real_face.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include "core/libm/libm.hpp"
#include "core/math.hpp"

namespace ge::mountain::mapgen {

namespace {

namespace libm = ge::core::libm;

constexpr std::string_view kAlphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Metres a degree of latitude spans.
constexpr double kDegree = 111320;

// The value of a base64 character, or -1.
int sextet(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

// A Catmull–Rom weight set for a fraction `t` between samples 1 and 2.
std::array<double, 4> weights(double t) {
  const double t2 = t * t;
  const double t3 = t2 * t;
  return {-0.5 * t3 + t2 - 0.5 * t, 1.5 * t3 - 2.5 * t2 + 1, -1.5 * t3 + 2 * t2 + 0.5 * t,
          0.5 * t3 - 0.5 * t2};
}

}  // namespace

const FaceCrop* face_crop(std::string_view id) {
  for (const FaceCrop& c : kFaceCrops) {
    if (id == c.id) return &c;
  }
  return nullptr;
}

std::optional<RealFace> real_face(std::string_view id) {
  const FaceCrop* crop = face_crop(id);
  if (!crop) return std::nullopt;
  for (const FaceData* f : baked_faces()) {
    if (id != f->id) continue;
    auto heights = decode_face_grid(f->data, f->floor);
    if (!heights) return std::nullopt;
    return RealFace{crop, std::move(*heights)};
  }
  return std::nullopt;
}

std::optional<std::vector<float>> decode_face_grid(std::string_view data, double floor) {
  std::vector<uint8_t> bytes;
  bytes.reserve(data.size() * 3 / 4);
  uint32_t acc = 0;
  int bits = 0;
  size_t pad = 0;
  for (const char ch : data) {
    if (ch == '=') {
      pad++;
      continue;
    }
    const int v = sextet(ch);
    if (pad > 0) return std::nullopt;
    if (v < 0) return std::nullopt;
    acc = (acc << 6) | uint32_t(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      bytes.push_back(uint8_t((acc >> bits) & 0xff));
    }
  }
  constexpr size_t n = kFaceN;
  std::vector<int> q(n * n);
  size_t at = 0;
  for (size_t i = 0; i < n * n; i++) {
    uint32_t z = 0;
    int shift = 0;
    uint8_t b = 0;
    do {
      if (at >= bytes.size() || shift > 28) return std::nullopt;
      b = bytes[at++];
      z |= uint32_t(b & 0x7f) << shift;
      shift += 7;
    } while (b & 0x80);
    const int d = (z & 1) ? -int((z + 1) >> 1) : int(z >> 1);
    const size_t r = i / n;
    const size_t c = i % n;
    const int left = c > 0 ? q[i - 1] : 0;
    const int up = r > 0 ? q[i - n] : 0;
    const int diag = r > 0 && c > 0 ? q[i - n - 1] : 0;
    q[i] = d + left + up - diag;
  }
  if (at != bytes.size()) return std::nullopt;
  std::vector<float> out(n * n);
  for (size_t i = 0; i < n * n; i++) out[i] = static_cast<float>(floor + q[i] * kFaceStep);
  return out;
}

std::string encode_face_grid(std::span<const int> steps) {
  constexpr size_t n = kFaceN;
  std::vector<uint8_t> bytes;
  bytes.reserve(n * n * 2);
  const auto at = [&](size_t r, size_t c, bool on) { return on ? steps[r * n + c] : 0; };
  for (size_t r = 0; r < n; r++) {
    for (size_t c = 0; c < n; c++) {
      const int d = at(r, c, true) - at(r, c - 1, c > 0) - at(r - 1, c, r > 0) +
                    at(r - 1, c - 1, r > 0 && c > 0);
      uint32_t z = d >= 0 ? uint32_t(d) * 2 : uint32_t(-d) * 2 - 1;
      while (z >= 0x80) {
        bytes.push_back(uint8_t((z & 0x7f) | 0x80));
        z >>= 7;
      }
      bytes.push_back(uint8_t(z));
    }
  }
  std::string out;
  out.reserve((bytes.size() + 2) / 3 * 4);
  for (size_t i = 0; i < bytes.size(); i += 3) {
    const uint32_t b0 = bytes[i];
    const uint32_t b1 = i + 1 < bytes.size() ? bytes[i + 1] : 0;
    const uint32_t b2 = i + 2 < bytes.size() ? bytes[i + 2] : 0;
    const uint32_t v = (b0 << 16) | (b1 << 8) | b2;
    out += kAlphabet[(v >> 18) & 63];
    out += kAlphabet[(v >> 12) & 63];
    out += i + 1 < bytes.size() ? kAlphabet[(v >> 6) & 63] : '=';
    out += i + 2 < bytes.size() ? kAlphabet[v & 63] : '=';
  }
  return out;
}

double face_height(const RealFace& face, double x, double z) {
  constexpr int n = kFaceN;
  const double gx = ge::core::clamp(x / kFaceCell, 0, n - 1);
  const double gz = ge::core::clamp(z / kFaceCell, 0, n - 1);
  const int c = std::min(n - 2, int(std::floor(gx)));
  const int r = std::min(n - 2, int(std::floor(gz)));
  const std::array<double, 4> wx = weights(gx - c);
  const std::array<double, 4> wz = weights(gz - r);
  double v = 0;
  for (int j = 0; j < 4; j++) {
    const int rr = std::clamp(r - 1 + j, 0, n - 1);
    double row = 0;
    for (int i = 0; i < 4; i++) {
      const int cc = std::clamp(c - 1 + i, 0, n - 1);
      row += double(face.heights[size_t(rr) * size_t(n) + size_t(cc)]) * wx[size_t(i)];
    }
    v += row * wz[size_t(j)];
  }
  return v;
}

double face_mean(const RealFace& face, double x0, double x1, double z0, double z1) {
  double sum = 0;
  int count = 0;
  for (int j = 0; z0 + j * kFaceCell <= z1; j++) {
    for (int i = 0; x0 + i * kFaceCell <= x1; i++) {
      sum += face_height(face, x0 + i * kFaceCell, z0 + j * kFaceCell);
      count++;
    }
  }
  return count > 0 ? sum / count : face_height(face, x0, z0);
}

double face_extreme(const RealFace& face, double x0, double x1, double z0, double z1, bool low) {
  double best = x0;
  double best_v =
      low ? std::numeric_limits<double>::infinity() : -std::numeric_limits<double>::infinity();
  for (int i = 0; x0 + i * kFaceCell <= x1; i++) {
    const double x = x0 + i * kFaceCell;
    const double v = face_mean(face, x, x, z0, z1);
    if (low ? v < best_v : v > best_v) {
      best_v = v;
      best = x;
    }
  }
  return best;
}

std::pair<double, double> globe_at(const FaceCrop& crop, double x, double z) {
  const double th = crop.bearing * ge::core::kPi / 180;
  const double dx = x - kFaceSize / 2;
  const double dz = z - kFaceSize / 2;
  // +z down the fall line (the bearing), +x to its right.
  const double e = crop.east + dx * libm::cos(th) + dz * libm::sin(th);
  const double n = crop.north - dx * libm::sin(th) + dz * libm::cos(th);
  const double kx = kDegree * libm::cos(crop.lat * ge::core::kPi / 180);
  return {crop.lat + n / kDegree, crop.lon + e / kx};
}

std::pair<double, double> map_at(const FaceCrop& crop, double lat, double lon) {
  const double th = crop.bearing * ge::core::kPi / 180;
  const double kx = kDegree * libm::cos(crop.lat * ge::core::kPi / 180);
  const double e = (lon - crop.lon) * kx - crop.east;
  const double n = (lat - crop.lat) * kDegree - crop.north;
  const double dx = e * libm::cos(th) - n * libm::sin(th);
  const double dz = e * libm::sin(th) + n * libm::cos(th);
  return {kFaceSize / 2 + dx, kFaceSize / 2 + dz};
}

}  // namespace ge::mountain::mapgen
