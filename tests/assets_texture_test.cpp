// engine/assets/image.hpp and the glTF loader's base colour textures:
//
//   * THE PNG READER takes a palette image at a packed bit depth with its transparency, and
//     refuses a broken checksum.
//   * THE TEXEL is the nearest one, repeating outside 0..1; sRGB's transfer holds its ends.
//   * THE BAKE: a quad of two triangles sharing two vertices, each triangle's middle on another
//     swatch of a colour atlas, loads as two baked parts (each the texel's colour, linear, times
//     the base colour factor), the shared vertices split so every triangle is one part.
//   * A TEXTURE IT CANNOT READ (not a PNG) leaves the base colour factor and says so.
//
// ge:summary Tests for the PNG reader, the nearest texel and the loader's texture bake
// ge:keywords test assets image png palette trns texel srgb base-colour-texture bake atlas
// ge:skill assets testing
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "assets/gltf.hpp"
#include "assets/image.hpp"
#include "check.hpp"

namespace {

namespace a = ge::assets;

void put32(std::vector<uint8_t>& out, uint32_t v) {
  for (int s = 24; s >= 0; s -= 8) out.push_back(static_cast<uint8_t>(v >> s));
}

void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
  put32(out, static_cast<uint32_t>(data.size()));
  const size_t start = out.size();
  out.insert(out.end(), type, type + 4);
  out.insert(out.end(), data.begin(), data.end());
  put32(out, a::crc32(&out[start], out.size() - start));
}

// A palette PNG of one row (`depth` bits an index, filter 0), its colours and transparencies.
std::vector<uint8_t> palette_png(int width, int depth, const std::vector<uint8_t>& indices,
                                 const std::vector<uint8_t>& rgb,
                                 const std::vector<uint8_t>& alpha) {
  std::vector<uint8_t> raw{0};
  uint8_t byte = 0;
  int used = 0;
  for (uint8_t i : indices) {
    byte = static_cast<uint8_t>(byte | (i << (8 - depth - used)));
    used += depth;
    if (used == 8) raw.push_back(byte), byte = 0, used = 0;
  }
  if (used) raw.push_back(byte);
  std::vector<uint8_t> z{
      0x78, 0x01, 1, static_cast<uint8_t>(raw.size()), 0, static_cast<uint8_t>(~raw.size()), 0xff};
  z.insert(z.end(), raw.begin(), raw.end());
  uint32_t s1 = 1, s2 = 0;
  for (uint8_t b : raw) s1 = (s1 + b) % 65521, s2 = (s2 + s1) % 65521;
  put32(z, (s2 << 16) | s1);
  std::vector<uint8_t> out{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  std::vector<uint8_t> ihdr;
  put32(ihdr, static_cast<uint32_t>(width));
  put32(ihdr, 1);
  ihdr.insert(ihdr.end(), {static_cast<uint8_t>(depth), 3, 0, 0, 0});
  chunk(out, "IHDR", ihdr);
  chunk(out, "PLTE", rgb);
  if (!alpha.empty()) chunk(out, "tRNS", alpha);
  chunk(out, "IDAT", z);
  chunk(out, "IEND", {});
  return out;
}

std::string base64(const std::vector<uint8_t>& b) {
  static const char* k = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string s;
  for (size_t i = 0; i < b.size(); i += 3) {
    const uint32_t n = (uint32_t{b[i]} << 16) | (i + 1 < b.size() ? uint32_t{b[i + 1]} << 8 : 0) |
                       (i + 2 < b.size() ? b[i + 2] : 0);
    s += k[(n >> 18) & 63];
    s += k[(n >> 12) & 63];
    s += i + 1 < b.size() ? k[(n >> 6) & 63] : '=';
    s += i + 2 < b.size() ? k[n & 63] : '=';
  }
  return s;
}

void put(std::vector<uint8_t>& b, std::initializer_list<float> v) {
  for (float f : v) {
    uint8_t x[4];
    std::memcpy(x, &f, 4);
    b.insert(b.end(), x, x + 4);
  }
}

// A quad in glTF's xy plane, two triangles (0 1 2) and (0 2 3), textured by `image`: the first
// triangle's middle on the atlas's left half, the second's on its right.
a::LoadResult textured_quad(const std::vector<uint8_t>& image) {
  std::vector<uint8_t> buf;
  put(buf, {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0});               // 0: four positions
  put(buf, {0.0f, 0.5f, 0.3f, 0.5f, 0.7f, 0.5f, 0.95f, 0.5f});  // 48: four uvs
  for (const uint8_t i : std::vector<uint8_t>{0, 1, 2, 0, 2, 3})
    buf.insert(buf.end(), {i, uint8_t{0}});  // 80: indices
  const size_t at = buf.size();              // 92: the image
  buf.insert(buf.end(), image.begin(), image.end());
  const std::string json =
      R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],
  "nodes":[{"name":"quad","mesh":0}],
  "meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,
                            "material":0}]}],
  "materials":[{"name":"atlas","pbrMetallicRoughness":{"baseColorFactor":[0.5,1,1,1],
                "baseColorTexture":{"index":0}}}],
  "textures":[{"source":0}],
  "images":[{"bufferView":3,"mimeType":"image/png"}],
  "buffers":[{"byteLength":)" +
      std::to_string(buf.size()) + R"(,"uri":"data:application/octet-stream;base64,)" +
      base64(buf) + R"("}],
  "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},
                 {"buffer":0,"byteOffset":48,"byteLength":32},
                 {"buffer":0,"byteOffset":80,"byteLength":12},
                 {"buffer":0,"byteOffset":)" +
      std::to_string(at) + R"(,"byteLength":)" + std::to_string(image.size()) + R"(}],
  "accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3",
                "min":[0,0,0],"max":[1,1,0]},
               {"bufferView":1,"componentType":5126,"count":4,"type":"VEC2"},
               {"bufferView":2,"componentType":5123,"count":6,"type":"SCALAR"}]})";
  return a::load_gltf({reinterpret_cast<const uint8_t*>(json.data()), json.size()}, "quad.gltf");
}

}  // namespace

TEST(a_palette_png_reads_at_a_packed_depth_with_its_transparency) {
  // Five pixels at 2 bits an index: red, blue, half-clear green, blue, red.
  const auto png =
      palette_png(5, 2, {0, 1, 2, 1, 0}, {255, 0, 0, 0, 0, 255, 0, 255, 0}, {255, 255, 128});
  const auto img = a::decode_png(png);
  CHECK(img && img->width == 5 && img->height == 1);
  const uint8_t want[5][4] = {
      {255, 0, 0, 255}, {0, 0, 255, 255}, {0, 255, 0, 128}, {0, 0, 255, 255}, {255, 0, 0, 255}};
  for (int x = 0; x < 5; ++x) CHECK(std::memcmp(&img->rgba[size_t(x) * 4], want[x], 4) == 0);
  // A flipped bit in a chunk fails its checksum.
  auto bad = png;
  bad[bad.size() - 20] ^= 1;
  CHECK(!a::decode_png(bad));
}

TEST(the_texel_is_the_nearest_and_repeats) {
  const auto img = *a::decode_png(palette_png(2, 8, {0, 1}, {10, 20, 30, 40, 50, 60}, {}));
  CHECK(a::texel(img, 0.2f, 0.5f)[0] == 10);
  CHECK(a::texel(img, 0.7f, 0.5f)[0] == 40);
  CHECK(a::texel(img, 1.2f, 0.5f)[0] == 10);
  CHECK(a::texel(img, -0.3f, 0.5f)[0] == 40);
  CHECK(a::srgb_to_linear(0) == 0.0f);
  CHECK(std::fabs(a::srgb_to_linear(255) - 1.0f) < 1e-6f);
  CHECK(std::fabs(a::srgb_to_linear(188) - 0.5f) < 0.01f);
}

TEST(a_colour_atlas_is_baked_into_parts_and_shared_vertices_split) {
  const a::LoadResult r = textured_quad(palette_png(2, 8, {0, 1}, {255, 0, 0, 0, 0, 255}, {}));
  CHECK(r.ok);
  const a::Mesh& m = r.model.meshes[0];
  // The atlas's own material, then a part a colour: red, then blue.
  CHECK(r.model.materials.size() == 3);
  CHECK(r.model.materials[1].name == "atlas#ff0000");
  CHECK(r.model.materials[2].name == "atlas#0000ff");
  CHECK(std::fabs(r.model.materials[1].rgb[0] - 0.5f) < 1e-6f);  // times the factor's 0.5
  CHECK(r.model.materials[1].rgb[1] == 0.0f && r.model.materials[2].rgb[2] == 1.0f);
  // Two vertices shared by both triangles are copied: six vertices, each triangle one part.
  CHECK(m.vertex_count() == 6 && m.indices.size() == 6);
  for (size_t t = 0; t < 6; t += 3) {
    const uint8_t p = m.parts[m.indices[t]];
    CHECK(p == m.parts[m.indices[t + 1]] && p == m.parts[m.indices[t + 2]]);
  }
  CHECK(m.parts[m.indices[0]] == 1 && m.parts[m.indices[3]] == 2);
  CHECK(m.positions.size() == 18 && m.normals.size() == 18 && m.joints.size() == 24 &&
        m.weights.size() == 24);
}

TEST(a_texture_it_cannot_read_keeps_the_factor_and_says_so) {
  const a::LoadResult r = textured_quad({0xff, 0xd8, 0xff, 0xe0, 1, 2, 3, 4});  // a JPEG's start
  CHECK(r.ok);
  CHECK(r.model.materials.size() == 1 && r.model.materials[0].rgb[0] == 0.5f);
  bool said = false;
  for (const auto& n : r.notes) said |= n.find("not an 8-bit PNG") != std::string::npos;
  CHECK(said);
}
