// The skeletal model file read and written (assets/iqm.hpp).
//
// ge:summary The .iqm reader and writer: lumps checked, the frame swapped, the clips baked
// ge:keywords assets iqm load_iqm write_iqm lump vertex-array pose frame bake slerp winding
// ge:skill assets
// ge:test tests/assets_iqm_test.cpp
#include "assets/iqm.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>

#include "core/libm/libm.hpp"

namespace ge::assets {
namespace {

constexpr char kMagic[16] = "INTERQUAKEMODEL";  // fifteen letters and a NUL
constexpr uint32_t kVersion = 2;
constexpr size_t kHeaderBytes = 16 + 27 * 4;

// The vertex arrays' types and formats.
enum : uint8_t { kPosition = 0, kTexcoord = 1, kNormal = 2, kBlendIndexes = 4, kBlendWeights = 5 };
enum : uint8_t {
  kByte = 0,
  kUbyte = 1,
  kShort = 2,
  kUshort = 3,
  kInt = 4,
  kUint = 5,
  kHalf = 6,
  kFloat = 7,
  kDouble = 8
};
constexpr uint32_t kLoop = 1;

// The header's counts and offsets, in the file's order.
struct Header {
  uint32_t version, filesize, flags;
  uint32_t num_text, ofs_text;
  uint32_t num_meshes, ofs_meshes;
  uint32_t num_vertexarrays, num_vertexes, ofs_vertexarrays;
  uint32_t num_triangles, ofs_triangles, ofs_adjacency;
  uint32_t num_joints, ofs_joints;
  uint32_t num_poses, ofs_poses;
  uint32_t num_anims, ofs_anims;
  uint32_t num_frames, num_framechannels, ofs_frames, ofs_bounds;
  uint32_t num_comment, ofs_comment;
  uint32_t num_extensions, ofs_extensions;
};
static_assert(sizeof(Header) == size_t{27} * 4);

uint32_t u32(const uint8_t* p) {
  return uint32_t{p[0]} | uint32_t{p[1]} << 8 | uint32_t{p[2]} << 16 | uint32_t{p[3]} << 24;
}
float f32(const uint8_t* p) { return std::bit_cast<float>(u32(p)); }

// The engine's frame from the file's (assets/iqm.hpp § THE FRAME).
void point_in(const float f[3], double metres, float out[3]) {
  out[0] = static_cast<float>(-f[1] * metres);
  out[1] = static_cast<float>(f[2] * metres);
  out[2] = static_cast<float>(f[0] * metres);
}
void normal_in(const float f[3], float out[3]) {
  out[0] = 0.0f - f[1];
  out[1] = f[2];
  out[2] = f[0];
}
void rotation_in(const float f[4], float out[4]) {
  out[0] = f[1];
  out[1] = 0.0f - f[2];
  out[2] = 0.0f - f[0];
  out[3] = f[3];
}
void scale_in(const float f[3], float out[3]) {
  out[0] = f[1];
  out[1] = f[2];
  out[2] = f[0];
}
// And back.
void point_out(const float e[3], double metres, float out[3]) {
  out[0] = static_cast<float>(e[2] / metres);
  out[1] = static_cast<float>(-e[0] / metres);
  out[2] = static_cast<float>(e[1] / metres);
}
void normal_out(const float e[3], float out[3]) {
  out[0] = e[2];
  out[1] = 0.0f - e[0];
  out[2] = e[1];
}
void rotation_out(const float e[4], float out[4]) {
  out[0] = 0.0f - e[2];
  out[1] = e[0];
  out[2] = 0.0f - e[1];
  out[3] = e[3];
}
void scale_out(const float e[3], float out[3]) {
  out[0] = e[2];
  out[1] = e[0];
  out[2] = e[1];
}

void normalise4(float q[4]) {
  const double n = std::sqrt(double{q[0]} * q[0] + double{q[1]} * q[1] + double{q[2]} * q[2] +
                             double{q[3]} * q[3]);
  if (n <= 0) {
    q[0] = q[1] = q[2] = 0;
    q[3] = 1;
    return;
  }
  for (int k = 0; k < 4; ++k) q[k] = static_cast<float>(q[k] / n);
}

void normalise3(float v[3]) {
  const double n = std::sqrt(double{v[0]} * v[0] + double{v[1]} * v[1] + double{v[2]} * v[2]);
  if (n <= 1e-20) {
    v[0] = 0;
    v[1] = 1;
    v[2] = 0;
    return;
  }
  for (int k = 0; k < 3; ++k) v[k] = static_cast<float>(v[k] / n);
}

// a to b by f, on the arc the short way round.
void slerp(const float a[4], const float b[4], double f, float out[4]) {
  double dot =
      double{a[0]} * b[0] + double{a[1]} * b[1] + double{a[2]} * b[2] + double{a[3]} * b[3];
  double sb = 1;
  if (dot < 0) {
    dot = -dot;
    sb = -1;
  }
  double wa = 1 - f, wb = f;
  if (dot < 0.9995) {
    const double theta = core::libm::acos(dot);
    const double s = core::libm::sin(theta);
    wa = core::libm::sin((1 - f) * theta) / s;
    wb = core::libm::sin(f * theta) / s;
  }
  for (int k = 0; k < 4; ++k) out[k] = static_cast<float>(wa * a[k] + sb * wb * b[k]);
  normalise4(out);
}

struct Reader {
  std::span<const uint8_t> bytes;
  std::string where;
  std::string error;

  bool fits(uint64_t ofs, uint64_t count, uint64_t size, const char* what) {
    if (ofs + count * size <= bytes.size()) return true;
    error = where + ": the " + what + " runs past the end of the file (" + std::to_string(count) +
            " at byte " + std::to_string(ofs) + ", the file is " + std::to_string(bytes.size()) +
            " bytes); next: the file is cut short or damaged, export it again";
    return false;
  }
  std::string text(const Header& h, uint32_t at) const {
    if (at >= h.num_text) return "";
    const auto* p = reinterpret_cast<const char*>(bytes.data() + h.ofs_text + at);
    const size_t room = h.num_text - at;
    return {p, strnlen(p, room)};
  }
};

// One component of a vertex array as a number (an integer format as its integer).
double component(uint32_t format, const uint8_t* p) {
  switch (format) {
    case kByte:
      return static_cast<int8_t>(p[0]);
    case kUbyte:
      return p[0];
    case kShort:
      return static_cast<int16_t>(uint16_t(p[0] | p[1] << 8));
    case kUshort:
      return uint16_t(p[0] | p[1] << 8);
    case kInt:
      return static_cast<int32_t>(u32(p));
    case kUint:
      return u32(p);
    case kFloat:
      return f32(p);
    case kDouble: {
      uint64_t v = 0;
      for (int k = 7; k >= 0; --k) v = v << 8 | p[k];
      return std::bit_cast<double>(v);
    }
    default:
      return 0;
  }
}
size_t format_bytes(uint32_t format) {
  switch (format) {
    case kByte:
    case kUbyte:
      return 1;
    case kShort:
    case kUshort:
    case kHalf:
      return 2;
    case kInt:
    case kUint:
    case kFloat:
      return 4;
    case kDouble:
      return 8;
    default:
      return 0;
  }
}

struct Array {
  uint32_t format = 0, size = 0, offset = 0;
  bool present = false;
  // Component c of vertex v (0 past the array's components).
  double at(std::span<const uint8_t> b, uint32_t v, uint32_t c) const {
    if (!present || c >= size) return 0;
    return component(format, b.data() + offset + (size_t{v} * size + c) * format_bytes(format));
  }
};

}  // namespace

LoadResult load_iqm(std::span<const uint8_t> bytes, std::string_view where_in,
                    const IqmOptions& options) {
  LoadResult out;
  Reader r{bytes, std::string(where_in), {}};
  const auto fail = [&](const std::string& why) {
    out.ok = false;
    out.model = {};
    out.error = r.error.empty() ? r.where + ": " + why : r.error;
    return out;
  };
  if (bytes.size() < kHeaderBytes || std::memcmp(bytes.data(), kMagic, 16) != 0)
    return fail(
        "not a skeletal model file (no `INTERQUAKEMODEL` at its start); next: give a .iqm file");
  Header h{};
  {
    uint32_t fields[27];
    for (size_t k = 0; k < 27; ++k) fields[k] = u32(bytes.data() + 16 + k * 4);
    std::memcpy(&h, fields, sizeof h);
  }
  if (h.version != kVersion)
    return fail("version " + std::to_string(h.version) +
                ", the reader takes version 2; next: export it again as version 2");
  if (!r.fits(h.ofs_text, h.num_text, 1, "text") ||
      !r.fits(h.ofs_meshes, h.num_meshes, 24, "mesh table") ||
      !r.fits(h.ofs_vertexarrays, h.num_vertexarrays, 20, "vertex array table") ||
      !r.fits(h.ofs_triangles, h.num_triangles, 12, "triangle list") ||
      !r.fits(h.ofs_joints, h.num_joints, 48, "joint table") ||
      !r.fits(h.ofs_poses, h.num_poses, 88, "pose table") ||
      !r.fits(h.ofs_anims, h.num_anims, 20, "animation table") ||
      !r.fits(h.ofs_frames, uint64_t{h.num_frames} * h.num_framechannels, 2, "frames"))
    return fail("");
  const double metres = options.metres;
  Model& m = out.model;

  // The vertex arrays.
  Array pos, uv, nrm, idx, wgt;
  for (uint32_t i = 0; i < h.num_vertexarrays; ++i) {
    const uint8_t* p = bytes.data() + h.ofs_vertexarrays + size_t{i} * 20;
    const uint32_t type = u32(p), format = u32(p + 8), size = u32(p + 12), ofs = u32(p + 16);
    Array* a = type == kPosition       ? &pos
               : type == kTexcoord     ? &uv
               : type == kNormal       ? &nrm
               : type == kBlendIndexes ? &idx
               : type == kBlendWeights ? &wgt
                                       : nullptr;
    if (a == nullptr) continue;  // a tangent, a colour, a custom array: passed over
    if (format_bytes(format) == 0 || format == kHalf)
      return fail("vertex array " + std::to_string(i) + " has format " + std::to_string(format) +
                  ", which the reader does not take (bytes, shorts, ints, floats, doubles); "
                  "next: export it with float positions and byte blend arrays");
    if (size == 0 || size > 4)
      return fail("vertex array " + std::to_string(i) + " has " + std::to_string(size) +
                  " components; next: export it again (1 to 4)");
    if (!r.fits(ofs, uint64_t{h.num_vertexes} * size, format_bytes(format), "vertex array"))
      return fail("");
    *a = {format, size, ofs, true};
  }
  if (h.num_meshes > 0 && (!pos.present || pos.size < 3))
    return fail("the vertices have no positions; next: export it with its positions");
  // Byte and short weights are fractions of their largest value.
  const double weight_scale = wgt.format == kUbyte    ? 1.0 / 255
                              : wgt.format == kUshort ? 1.0 / 65535
                                                      : 1;

  // The joints at rest.
  if (h.num_joints > static_cast<uint32_t>(kMaxJoints))
    return fail(std::to_string(h.num_joints) + " joints, more than the engine's " +
                std::to_string(kMaxJoints) + "; next: export it with fewer bones");
  std::vector<Affine> world;
  for (uint32_t i = 0; i < h.num_joints; ++i) {
    const uint8_t* p = bytes.data() + h.ofs_joints + size_t{i} * 48;
    Joint j;
    j.name = r.text(h, u32(p));
    j.parent = static_cast<int32_t>(u32(p + 4));
    if (j.parent >= static_cast<int32_t>(i) || j.parent < -1)
      return fail("joint " + std::to_string(i) + " (`" + j.name + "`) names parent " +
                  std::to_string(j.parent) +
                  ", which is not before it; next: export it again (parents come first)");
    float t[3], q[4], s[3];
    for (int k = 0; k < 3; ++k) t[k] = f32(p + 8 + static_cast<size_t>(k) * 4);
    for (int k = 0; k < 4; ++k) q[k] = f32(p + 20 + static_cast<size_t>(k) * 4);
    for (int k = 0; k < 3; ++k) s[k] = f32(p + 36 + static_cast<size_t>(k) * 4);
    normalise4(q);
    point_in(t, metres, j.rest.t);
    rotation_in(q, j.rest.r);
    scale_in(s, j.rest.s);
    const Affine local = to_affine(j.rest);
    world.push_back(j.parent < 0 ? local : compose(world[static_cast<size_t>(j.parent)], local));
    j.inverse_bind = invert(world.back());
    m.joints.push_back(std::move(j));
  }
  const bool rooted = m.joints.empty();
  if (rooted) m.joints.push_back({"root", -1, {}, {}});
  const auto njoints = static_cast<uint32_t>(m.joints.size());

  // The meshes.
  int agree = 0, disagree = 0;
  for (uint32_t i = 0; i < h.num_meshes; ++i) {
    const uint8_t* p = bytes.data() + h.ofs_meshes + size_t{i} * 24;
    const std::string name = r.text(h, u32(p)), material = r.text(h, u32(p + 4));
    const uint32_t first_v = u32(p + 8), count_v = u32(p + 12);
    const uint32_t first_t = u32(p + 16), count_t = u32(p + 20);
    if (uint64_t{first_v} + count_v > h.num_vertexes ||
        uint64_t{first_t} + count_t > h.num_triangles)
      return fail("mesh `" + name +
                  "` runs past the vertices or the triangles; next: export it again");
    int part = -1;
    for (size_t k = 0; k < m.materials.size(); ++k)
      if (m.materials[k].name == material) part = static_cast<int>(k);
    if (part < 0) {
      if (m.materials.size() >= static_cast<size_t>(kMaxParts))
        return fail("more than " + std::to_string(kMaxParts) + " materials; next: merge some");
      m.materials.push_back({material, {1, 1, 1}, -1, -1});
      part = static_cast<int>(m.materials.size()) - 1;
    }
    Mesh mesh;
    mesh.name = name;
    mesh.positions.resize(size_t{count_v} * 3);
    mesh.normals.resize(size_t{count_v} * 3);
    mesh.joints.assign(size_t{count_v} * kJointsPerVertex, 0);
    mesh.weights.assign(size_t{count_v} * kJointsPerVertex, 0);
    mesh.parts.assign(count_v, static_cast<uint8_t>(part));
    mesh.uv.assign(size_t{count_v} * 2, 0);
    for (uint32_t v = 0; v < count_v; ++v) {
      const uint32_t fv = first_v + v;
      const float fp[3] = {static_cast<float>(pos.at(bytes, fv, 0)),
                           static_cast<float>(pos.at(bytes, fv, 1)),
                           static_cast<float>(pos.at(bytes, fv, 2))};
      point_in(fp, metres, &mesh.positions[size_t{v} * 3]);
      if (nrm.present) {
        const float fn[3] = {static_cast<float>(nrm.at(bytes, fv, 0)),
                             static_cast<float>(nrm.at(bytes, fv, 1)),
                             static_cast<float>(nrm.at(bytes, fv, 2))};
        normal_in(fn, &mesh.normals[size_t{v} * 3]);
        normalise3(&mesh.normals[size_t{v} * 3]);
      }
      mesh.uv[size_t{v} * 2] = static_cast<float>(uv.at(bytes, fv, 0));
      mesh.uv[size_t{v} * 2 + 1] = static_cast<float>(uv.at(bytes, fv, 1));
      // The influences: the strongest four, summing to 1; none at all binds it to joint 0.
      double sum = 0;
      for (uint32_t c = 0; c < kJointsPerVertex; ++c) {
        const double w = rooted || !idx.present ? 0 : wgt.at(bytes, fv, c) * weight_scale;
        const double jn = idx.at(bytes, fv, c);
        if (w > 0 && (jn < 0 || jn >= njoints))
          return fail("vertex " + std::to_string(fv) + " is bound to joint " +
                      std::to_string(static_cast<long long>(jn)) + " of " +
                      std::to_string(njoints) + "; next: export it again");
        mesh.joints[size_t{v} * kJointsPerVertex + c] = static_cast<uint8_t>(w > 0 ? jn : 0);
        mesh.weights[size_t{v} * kJointsPerVertex + c] = static_cast<float>(w);
        sum += w;
      }
      float* wv = &mesh.weights[size_t{v} * kJointsPerVertex];
      if (sum <= 0) {
        wv[0] = 1;
        for (int c = 1; c < kJointsPerVertex; ++c) wv[c] = 0;
      } else {
        for (int c = 0; c < kJointsPerVertex; ++c) wv[c] = static_cast<float>(wv[c] / sum);
      }
    }
    for (uint32_t t = 0; t < count_t; ++t) {
      const uint8_t* tp = bytes.data() + h.ofs_triangles + (size_t{first_t} + t) * 12;
      uint32_t ix[3];
      for (int k = 0; k < 3; ++k) {
        ix[k] = u32(tp + static_cast<size_t>(k) * 4);
        if (ix[k] < first_v || ix[k] >= first_v + count_v)
          return fail("triangle " + std::to_string(first_t + t) + " of mesh `" + name +
                      "` names vertex " + std::to_string(ix[k]) + ", outside the mesh's " +
                      std::to_string(first_v) + ".." + std::to_string(first_v + count_v) +
                      "; next: export it again");
        ix[k] -= first_v;
      }
      // The mirror turns the winding over: the last two swap.
      mesh.indices.insert(mesh.indices.end(), {ix[0], ix[2], ix[1]});
      if (nrm.present) {
        const float* a = &mesh.positions[size_t{ix[0]} * 3];
        const float* b = &mesh.positions[size_t{ix[2]} * 3];
        const float* c = &mesh.positions[size_t{ix[1]} * 3];
        const double e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const double e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const double fn[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                              e1[0] * e2[1] - e1[1] * e2[0]};
        double d = 0;
        for (const uint32_t vi : ix)
          for (int k = 0; k < 3; ++k) d += fn[k] * mesh.normals[size_t{vi} * 3 + k];
        if (d > 0) agree++;
        if (d < 0) disagree++;
      }
    }
    // A run under the name of the mesh before it is more of that mesh (a part a run).
    if (!m.meshes.empty() && m.meshes.back().name == mesh.name) {
      Mesh& to = m.meshes.back();
      const uint32_t base = to.vertex_count();
      for (const uint32_t ix : mesh.indices) to.indices.push_back(base + ix);
      const auto join = [](auto& x, const auto& y) { x.insert(x.end(), y.begin(), y.end()); };
      join(to.positions, mesh.positions);
      join(to.normals, mesh.normals);
      join(to.joints, mesh.joints);
      join(to.weights, mesh.weights);
      join(to.parts, mesh.parts);
      join(to.uv, mesh.uv);
    } else {
      m.meshes.push_back(std::move(mesh));
    }
  }
  // Which way the triangles face: as the normals say, else as the mirror leaves them.
  if (disagree > agree)
    for (Mesh& mesh : m.meshes)
      for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3)
        std::swap(mesh.indices[t + 1], mesh.indices[t + 2]);
  if (!nrm.present) {
    out.notes.push_back(r.where + ": no normals in the file: smooth ones made from its faces");
    for (Mesh& mesh : m.meshes) {
      std::fill(mesh.normals.begin(), mesh.normals.end(), 0.0f);
      for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const float* a = &mesh.positions[size_t{mesh.indices[t]} * 3];
        const float* b = &mesh.positions[size_t{mesh.indices[t + 1]} * 3];
        const float* c = &mesh.positions[size_t{mesh.indices[t + 2]} * 3];
        const double e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const double e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const double fn[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                              e1[0] * e2[1] - e1[1] * e2[0]};
        for (int k = 0; k < 3; ++k)
          for (int q = 0; q < 3; ++q)
            mesh.normals[size_t{mesh.indices[t + k]} * 3 + q] += static_cast<float>(fn[q]);
      }
      for (size_t v = 0; v * 3 < mesh.normals.size(); ++v) normalise3(&mesh.normals[v * 3]);
    }
  }

  // The animations, baked.
  if (h.num_anims > 0 && h.num_frames > 0 && !rooted) {
    if (h.num_poses != h.num_joints)
      return fail(std::to_string(h.num_poses) + " poses for " + std::to_string(h.num_joints) +
                  " joints; next: export it again (a pose a joint)");
    struct Pose {
      uint32_t mask;
      float offset[10], scale[10];
    };
    std::vector<Pose> poses(h.num_poses);
    uint32_t channels = 0;
    for (uint32_t i = 0; i < h.num_poses; ++i) {
      const uint8_t* p = bytes.data() + h.ofs_poses + size_t{i} * 88;
      poses[i].mask = u32(p + 4);
      for (int c = 0; c < 10; ++c) {
        poses[i].offset[c] = f32(p + 8 + static_cast<size_t>(c) * 4);
        poses[i].scale[c] = f32(p + 48 + static_cast<size_t>(c) * 4);
        if ((poses[i].mask >> c & 1) != 0) channels++;
      }
    }
    if (channels != h.num_framechannels)
      return fail("the poses store " + std::to_string(channels) +
                  " channels a frame, the header says " + std::to_string(h.num_framechannels) +
                  "; next: export it again");
    // Every frame's local transforms, in the engine's frame.
    std::vector<Trs> frames(size_t{h.num_frames} * njoints);
    for (uint32_t f = 0; f < h.num_frames; ++f) {
      const uint8_t* fp = bytes.data() + h.ofs_frames + size_t{f} * h.num_framechannels * 2;
      uint32_t k = 0;
      for (uint32_t j = 0; j < njoints; ++j) {
        float ch[10];
        for (int c = 0; c < 10; ++c) {
          ch[c] = poses[j].offset[c];
          if ((poses[j].mask >> c & 1) != 0) {
            ch[c] += poses[j].scale[c] *
                     static_cast<float>(fp[size_t{k} * 2] | fp[size_t{k} * 2 + 1] << 8);
            k++;
          }
        }
        normalise4(ch + 3);
        Trs& o = frames[size_t{f} * njoints + j];
        point_in(ch, metres, o.t);
        rotation_in(ch + 3, o.r);
        scale_in(ch + 7, o.s);
      }
    }
    for (uint32_t a = 0; a < h.num_anims; ++a) {
      const uint8_t* p = bytes.data() + h.ofs_anims + size_t{a} * 20;
      Clip clip;
      clip.name = r.text(h, u32(p));
      const uint32_t first = u32(p + 4), count = u32(p + 8);
      double fps = f32(p + 12);
      const bool loop = (u32(p + 16) & kLoop) != 0;
      if (count == 0) continue;
      if (uint64_t{first} + count > h.num_frames)
        return fail("animation `" + clip.name + "` runs past the frames; next: export it again");
      if (!(fps > 0) || !std::isfinite(fps)) {
        out.notes.push_back(r.where + ": animation `" + clip.name +
                            "` gives no frame rate: taken as 10 a second");
        fps = 10;
      }
      const double duration = (loop ? count : count - 1) / fps;
      const auto baked = static_cast<uint32_t>(std::lround(duration * kClipRateHz)) + 1;
      clip.frames = baked;
      clip.samples.resize(size_t{baked} * njoints);
      for (uint32_t k = 0; k < baked; ++k) {
        const double at = k * fps / kClipRateHz;
        auto i0 = static_cast<uint32_t>(std::floor(at));
        const double f = at - i0;
        uint32_t i1 = i0 + 1;
        if (loop) {
          i0 %= count;
          i1 %= count;
        } else {
          i0 = std::min(i0, count - 1);
          i1 = std::min(i1, count - 1);
        }
        for (uint32_t j = 0; j < njoints; ++j) {
          const Trs& x = frames[(size_t{first} + i0) * njoints + j];
          const Trs& y = frames[(size_t{first} + i1) * njoints + j];
          Trs& o = clip.samples[size_t{k} * njoints + j];
          for (int c = 0; c < 3; ++c) {
            o.t[c] = static_cast<float>(x.t[c] + (y.t[c] - x.t[c]) * f);
            o.s[c] = static_cast<float>(x.s[c] + (y.s[c] - x.s[c]) * f);
          }
          slerp(x.r, y.r, f, o.r);
        }
      }
      m.clips.push_back(std::move(clip));
    }
  }
  out.ok = true;
  return out;
}

LoadResult load_iqm_file(const std::string& path, const IqmOptions& options) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    LoadResult out;
    out.error = path + ": cannot open it; next: check the path";
    return out;
  }
  const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in), {}};
  return load_iqm(bytes, path, options);
}

namespace {

struct Writer {
  std::vector<uint8_t> b;
  void pad() {
    while (b.size() % 4 != 0) b.push_back(0);
  }
  void u32(uint32_t v) {
    for (int k = 0; k < 4; ++k) b.push_back(static_cast<uint8_t>(v >> (8 * k)));
  }
  void f32(float v) { u32(std::bit_cast<uint32_t>(v)); }
  void put(size_t at, uint32_t v) {
    for (int k = 0; k < 4; ++k) b[at + k] = static_cast<uint8_t>(v >> (8 * k));
  }
};

}  // namespace

std::vector<uint8_t> write_iqm(const Model& model, const IqmOptions& options) {
  const double metres = options.metres;
  Writer w;
  Header h{};
  h.version = kVersion;
  w.b.assign(kHeaderBytes, 0);
  std::memcpy(w.b.data(), kMagic, 16);

  // The text: offset 0 is the empty name.
  std::string text(1, '\0');
  const auto name = [&](const std::string& s) {
    const auto at = static_cast<uint32_t>(text.size());
    text += s;
    text.push_back('\0');
    return at;
  };
  // A file's mesh has one material, so each mesh goes out as a run a part: its vertices of
  // that part in their order, the triangles whose first vertex is of it (renumbered), all
  // under the mesh's name, which the reader joins again.
  struct Run {
    const Mesh* mesh;
    uint32_t name, material;
    std::vector<uint32_t> verts, tris;
  };
  std::vector<Run> runs;
  std::vector<uint32_t> joint_names, clip_names;
  for (const Mesh& m : model.meshes) {
    const uint32_t mesh_name = name(m.name);
    const size_t nv = m.positions.size() / 3;
    bool seen[kMaxParts] = {};
    for (size_t v = 0; v < nv; ++v) seen[v < m.parts.size() ? m.parts[v] : 0] = true;
    for (int part = 0; part < kMaxParts; ++part) {
      if (!seen[part]) continue;
      const auto pv = [&](size_t v) { return v < m.parts.size() ? m.parts[v] : 0; };
      Run run{&m,
              mesh_name,
              name(static_cast<size_t>(part) < model.materials.size()
                       ? model.materials[static_cast<size_t>(part)].name
                       : ""),
              {},
              {}};
      std::vector<uint32_t> renumber(nv, UINT32_MAX);
      for (size_t v = 0; v < nv; ++v)
        if (pv(v) == part) {
          renumber[v] = static_cast<uint32_t>(run.verts.size());
          run.verts.push_back(static_cast<uint32_t>(v));
        }
      for (size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        if (pv(m.indices[t]) != part) continue;
        const uint32_t ix[3] = {renumber[m.indices[t]], renumber[m.indices[t + 1]],
                                renumber[m.indices[t + 2]]};
        if (ix[1] == UINT32_MAX || ix[2] == UINT32_MAX) continue;  // spans two parts: not kept
        run.tris.insert(run.tris.end(), {ix[0], ix[1], ix[2]});
      }
      runs.push_back(std::move(run));
    }
  }
  for (const Joint& j : model.joints) joint_names.push_back(name(j.name));
  for (const Clip& c : model.clips) clip_names.push_back(name(c.name));
  h.ofs_text = static_cast<uint32_t>(w.b.size());
  h.num_text = static_cast<uint32_t>(text.size());
  w.b.insert(w.b.end(), text.begin(), text.end());
  w.pad();

  // The meshes.
  uint32_t vertices = 0, triangles = 0;
  h.ofs_meshes = static_cast<uint32_t>(w.b.size());
  h.num_meshes = static_cast<uint32_t>(runs.size());
  for (const Run& run : runs) {
    const auto nv = static_cast<uint32_t>(run.verts.size());
    const auto nt = static_cast<uint32_t>(run.tris.size() / 3);
    w.u32(run.name);
    w.u32(run.material);
    w.u32(vertices);
    w.u32(nv);
    w.u32(triangles);
    w.u32(nt);
    vertices += nv;
    triangles += nt;
  }
  h.num_vertexes = vertices;

  // The vertex arrays: their table, then their data.
  h.ofs_vertexarrays = static_cast<uint32_t>(w.b.size());
  h.num_vertexarrays = 5;
  const uint32_t kinds[5][3] = {{kPosition, kFloat, 3},
                                {kTexcoord, kFloat, 2},
                                {kNormal, kFloat, 3},
                                {kBlendIndexes, kUbyte, 4},
                                {kBlendWeights, kUbyte, 4}};
  size_t table = w.b.size();
  for (const auto& k : kinds) {
    w.u32(k[0]);
    w.u32(0);
    w.u32(k[1]);
    w.u32(k[2]);
    w.u32(0);  // the offset, filled below
  }
  for (int a = 0; a < 5; ++a) {
    w.pad();
    w.put(table + size_t(a) * 20 + 16, static_cast<uint32_t>(w.b.size()));
    for (const Run& run : runs) {
      const Mesh& m = *run.mesh;
      for (const uint32_t v : run.verts) {
        float o[3];
        switch (a) {
          case 0:
            point_out(&m.positions[size_t{v} * 3], metres, o);
            for (const float x : o) w.f32(x);
            break;
          case 1:
            w.f32(m.uv.size() >= (size_t{v} + 1) * 2 ? m.uv[size_t{v} * 2] : 0.0f);
            w.f32(m.uv.size() >= (size_t{v} + 1) * 2 ? m.uv[size_t{v} * 2 + 1] : 0.0f);
            break;
          case 2:
            normal_out(&m.normals[size_t{v} * 3], o);
            for (const float x : o) w.f32(x);
            break;
          case 3:
            for (int c = 0; c < kJointsPerVertex; ++c)
              w.b.push_back(m.joints[size_t{v} * kJointsPerVertex + c]);
            break;
          default: {
            // Weights in 255ths, summing to 255: the largest takes what rounding left over.
            const float* wv = &m.weights[size_t{v} * kJointsPerVertex];
            int q[kJointsPerVertex], total = 0, top = 0;
            for (int c = 0; c < kJointsPerVertex; ++c) {
              q[c] = static_cast<int>(std::lround(std::clamp(wv[c], 0.0f, 1.0f) * 255));
              total += q[c];
              if (wv[c] > wv[top]) top = c;
            }
            q[top] = std::clamp(q[top] + 255 - total, 0, 255);
            for (const int c : q) w.b.push_back(static_cast<uint8_t>(c));
          }
        }
      }
    }
  }
  w.pad();

  // The triangles, absolute, the mirror's swap undone.
  h.ofs_triangles = static_cast<uint32_t>(w.b.size());
  h.num_triangles = triangles;
  {
    uint32_t base = 0;
    for (const Run& run : runs) {
      for (size_t t = 0; t + 2 < run.tris.size(); t += 3) {
        w.u32(base + run.tris[t]);
        w.u32(base + run.tris[t + 2]);
        w.u32(base + run.tris[t + 1]);
      }
      base += static_cast<uint32_t>(run.verts.size());
    }
  }

  // The joints at rest.
  h.ofs_joints = static_cast<uint32_t>(w.b.size());
  h.num_joints = static_cast<uint32_t>(model.joints.size());
  const size_t nj = model.joints.size();
  for (size_t i = 0; i < nj; ++i) {
    const Joint& j = model.joints[i];
    w.u32(joint_names[i]);
    w.u32(static_cast<uint32_t>(j.parent));
    float t[3], q[4], s[3];
    point_out(j.rest.t, metres, t);
    rotation_out(j.rest.r, q);
    scale_out(j.rest.s, s);
    for (const float x : t) w.f32(x);
    for (const float x : q) w.f32(x);
    for (const float x : s) w.f32(x);
  }

  // Every clip's frames in the file's frame, then each channel's range.
  std::vector<float> channel;  // frame-major: 10 a joint
  uint32_t total_frames = 0;
  for (const Clip& c : model.clips) {
    for (uint32_t f = 0; f < c.frames; ++f)
      for (size_t j = 0; j < nj; ++j) {
        const Trs& x = c.samples[size_t{f} * nj + j];
        float t[3], q[4], s[3];
        point_out(x.t, metres, t);
        rotation_out(x.r, q);
        scale_out(x.s, s);
        channel.insert(channel.end(), {t[0], t[1], t[2], q[0], q[1], q[2], q[3], s[0], s[1], s[2]});
      }
    total_frames += c.frames;
  }
  std::vector<float> lo(nj * 10, 0), hi(nj * 10, 0);
  for (size_t j = 0; j < nj; ++j) {
    const Joint& jt = model.joints[j];
    float t[3], q[4], s[3];
    point_out(jt.rest.t, metres, t);
    rotation_out(jt.rest.r, q);
    scale_out(jt.rest.s, s);
    const float rest[10] = {t[0], t[1], t[2], q[0], q[1], q[2], q[3], s[0], s[1], s[2]};
    for (int c = 0; c < 10; ++c) {
      lo[j * 10 + c] = hi[j * 10 + c] = total_frames > 0 ? channel[j * 10 + c] : rest[c];
      for (uint32_t f = 0; f < total_frames; ++f) {
        const float v = channel[(size_t{f} * nj + j) * 10 + c];
        lo[j * 10 + c] = std::min(lo[j * 10 + c], v);
        hi[j * 10 + c] = std::max(hi[j * 10 + c], v);
      }
    }
  }
  h.ofs_poses = static_cast<uint32_t>(w.b.size());
  h.num_poses = static_cast<uint32_t>(nj);
  uint32_t channels = 0;
  for (size_t j = 0; j < nj; ++j) {
    uint32_t mask = 0;
    for (int c = 0; c < 10; ++c)
      if (hi[j * 10 + c] > lo[j * 10 + c]) mask |= 1u << c;
    w.u32(static_cast<uint32_t>(model.joints[j].parent));
    w.u32(mask);
    for (int c = 0; c < 10; ++c) w.f32(lo[j * 10 + c]);
    for (int c = 0; c < 10; ++c)
      w.f32((mask >> c & 1) != 0 ? (hi[j * 10 + c] - lo[j * 10 + c]) / 65535.0f : 0.0f);
    channels += static_cast<uint32_t>(std::popcount(mask));
  }

  // The animations and their frames.
  h.ofs_anims = static_cast<uint32_t>(w.b.size());
  h.num_anims = static_cast<uint32_t>(model.clips.size());
  {
    uint32_t first = 0;
    for (size_t i = 0; i < model.clips.size(); ++i) {
      w.u32(clip_names[i]);
      w.u32(first);
      w.u32(model.clips[i].frames);
      w.f32(kClipRateHz);
      w.u32(0);
      first += model.clips[i].frames;
    }
  }
  h.ofs_frames = static_cast<uint32_t>(w.b.size());
  h.num_frames = total_frames;
  h.num_framechannels = channels;
  for (uint32_t f = 0; f < total_frames; ++f)
    for (size_t j = 0; j < nj; ++j)
      for (int c = 0; c < 10; ++c) {
        const size_t at = j * 10 + c;
        if (!(hi[at] > lo[at])) continue;
        const float scale = (hi[at] - lo[at]) / 65535.0f;
        const double v = (channel[(size_t{f} * nj + j) * 10 + c] - lo[at]) / scale;
        const auto q = static_cast<uint16_t>(std::clamp<long>(std::lround(v), 0, 65535));
        w.b.push_back(static_cast<uint8_t>(q & 0xff));
        w.b.push_back(static_cast<uint8_t>(q >> 8));
      }
  w.pad();
  h.filesize = static_cast<uint32_t>(w.b.size());
  uint32_t fields[27];
  std::memcpy(fields, &h, sizeof h);
  for (size_t k = 0; k < 27; ++k) w.put(16 + k * 4, fields[k]);
  return w.b;
}

}  // namespace ge::assets
