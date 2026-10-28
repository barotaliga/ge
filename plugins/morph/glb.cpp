// The .glb writer: the binary chunk and its accessors, the JSON beside it, and the clips.
//
// ge:summary The morph model's glTF writer: buffers, accessors, targets, clips, the container
// ge:keywords morph gltf glb accessor buffer-view target weights step clip stem container
// ge:skill morph
// ge:test tests/morph_glb_test.cpp
#include "morph/glb.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "morph/smooth.hpp"

namespace ge::morph {

namespace {

constexpr int kFloat = 5126, kUInt = 5125;

std::string num(float v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.9g", static_cast<double>(v));
  return b;
}

std::string quoted(const std::string& s) {
  std::string out = "\"";
  for (const char c : s) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (static_cast<unsigned char>(c) < 0x20) {
      char b[8];
      std::snprintf(b, sizeof b, "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
      out += b;
    } else {
      out += c;
    }
  }
  return out + "\"";
}

std::string join(const std::vector<std::string>& items) {
  std::string s;
  for (size_t i = 0; i < items.size(); ++i) s += (i ? "," : "") + items[i];
  return s;
}

// The binary chunk, its views and the accessors over them.
struct Out {
  std::vector<uint8_t> bin;
  std::vector<std::string> views, accessors;

  int view(const void* bytes, size_t size) {
    while (bin.size() % 4) bin.push_back(0);
    const size_t offset = bin.size();
    const auto* b = static_cast<const uint8_t*>(bytes);
    bin.insert(bin.end(), b, b + size);
    views.push_back("{\"buffer\":0,\"byteOffset\":" + std::to_string(offset) +
                    ",\"byteLength\":" + std::to_string(size) + "}");
    return static_cast<int>(views.size() - 1);
  }
  int accessor(const void* bytes, size_t size, int component, const char* type, size_t count,
               const std::string& extra = "") {
    const int v = view(bytes, size);
    accessors.push_back(
        "{\"bufferView\":" + std::to_string(v) + ",\"componentType\":" + std::to_string(component) +
        ",\"count\":" + std::to_string(count) + ",\"type\":\"" + type + "\"" + extra + "}");
    return static_cast<int>(accessors.size() - 1);
  }
  // Three floats a vertex, with the bounds glTF asks of a position.
  int vec3(const std::vector<float>& v, bool bounds) {
    std::string extra;
    if (bounds) {
      float lo[3] = {0, 0, 0}, hi[3] = {0, 0, 0};
      for (size_t i = 0; i < v.size(); i++) {
        const size_t k = i % 3;
        lo[k] = i < 3 ? v[i] : std::min(lo[k], v[i]);
        hi[k] = i < 3 ? v[i] : std::max(hi[k], v[i]);
      }
      extra = ",\"min\":[" + num(lo[0]) + "," + num(lo[1]) + "," + num(lo[2]) + "],\"max\":[" +
              num(hi[0]) + "," + num(hi[1]) + "," + num(hi[2]) + "]";
    }
    return accessor(v.data(), v.size() * 4, kFloat, "VEC3", v.size() / 3, extra);
  }
};

// The file's x forward, y left, z up to glTF's z forward, x left, y up.
void to_gltf(const float* p, float scale, std::vector<float>& out) {
  out.insert(out.end(), {p[1] * scale, p[2] * scale, p[0] * scale});
}

std::string stem(const std::string& name) {
  size_t end = name.size();
  while (end > 0 && name[end - 1] >= '0' && name[end - 1] <= '9') end--;
  return end == 0 ? (name.empty() ? std::string("frame") : name) : name.substr(0, end);
}

}  // namespace

Clips clips_of(const Model& m) {
  Clips c;
  for (size_t i = 0; i < m.frames.size(); i++) {
    const Frame& f = m.frames[i];
    bool fresh = i == 0;
    if (!fresh) {
      const Frame& p = m.frames[i - 1];
      const bool in_group = f.interval > 0, was_group = p.interval > 0;
      fresh = in_group ? f.group != p.group : (was_group || stem(f.name) != stem(p.name));
    }
    if (fresh) {
      const std::string base = stem(f.name);
      std::string name = base;
      for (int n = 2; std::find(c.names.begin(), c.names.end(), name) != c.names.end(); n++)
        name = base + "_" + std::to_string(n);
      c.names.push_back(name);
    }
    c.of_frame.push_back(static_cast<int>(c.names.size() - 1));
  }
  return c;
}

std::vector<uint8_t> write_glb(const Model& m, const std::string& name,
                               const std::vector<GlbSkin>& skins, const GlbOptions& opts) {
  Out out;
  const uint32_t verts = m.vertex_count();
  const size_t frames = m.frames.size();

  // The mesh at the first frame, and every frame as its difference from it.
  std::vector<std::vector<float>> pos(frames), nrm(frames);
  for (size_t f = 0; f < frames; f++) {
    const std::vector<float> n = frame_normals(m, m.frames[f]);
    for (uint32_t v = 0; v < verts; v++) {
      to_gltf(&m.frames[f].positions[size_t(v) * 3], opts.unit_m, pos[f]);
      to_gltf(&n[size_t(v) * 3], 1.0f, nrm[f]);
    }
  }
  std::string attributes = "{}", targets;
  if (frames > 0) {
    attributes = "{\"POSITION\":" + std::to_string(out.vec3(pos[0], true)) +
                 ",\"NORMAL\":" + std::to_string(out.vec3(nrm[0], false)) + ",\"TEXCOORD_0\":" +
                 std::to_string(out.accessor(m.st.data(), m.st.size() * 4, kFloat, "VEC2", verts)) +
                 "}";
    std::vector<std::string> t;
    for (size_t f = 0; f < frames; f++) {
      std::vector<float> dp(pos[f].size()), dn(nrm[f].size());
      for (size_t i = 0; i < dp.size(); i++) {
        dp[i] = pos[f][i] - pos[0][i];
        dn[i] = nrm[f][i] - nrm[0][i];
      }
      t.push_back("{\"POSITION\":" + std::to_string(out.vec3(dp, true)) +
                  ",\"NORMAL\":" + std::to_string(out.vec3(dn, false)) + "}");
    }
    targets = ",\"targets\":[" + join(t) + "]";
  }
  const int a_idx =
      out.accessor(m.indices.data(), m.indices.size() * 4, kUInt, "SCALAR", m.indices.size());

  // The skins: an image, a texture and a material each.
  std::vector<std::string> images, textures, materials;
  for (size_t s = 0; s < skins.size(); s++) {
    auto image = [&](const std::vector<uint8_t>& png) {
      const int v = out.view(png.data(), png.size());
      images.push_back("{\"bufferView\":" + std::to_string(v) + ",\"mimeType\":\"image/png\"}");
      textures.push_back("{\"sampler\":0,\"source\":" + std::to_string(images.size() - 1) + "}");
      return std::to_string(textures.size() - 1);
    };
    std::string mat =
        "{\"name\":" + quoted(s == 0 ? name : "skin" + std::to_string(s)) +
        ",\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":" + image(skins[s].png) +
        "},\"metallicFactor\":0,\"roughnessFactor\":1}";
    if (!skins[s].glow_png.empty())
      mat += ",\"emissiveTexture\":{\"index\":" + image(skins[s].glow_png) +
             "},\"emissiveFactor\":[1,1,1]";
    materials.push_back(mat + "}");
  }

  // The clips: one-hot weights over the targets, a key a frame and one more at the clip's end.
  const Clips clips = clips_of(m);
  std::vector<std::string> animations;
  for (size_t c = 0; c < clips.names.size(); c++) {
    std::vector<float> times, weights;
    float t = 0;
    size_t last = 0;
    for (size_t f = 0; f < frames; f++) {
      if (clips.of_frame[f] != static_cast<int>(c)) continue;
      times.push_back(t);
      for (size_t k = 0; k < frames; k++) weights.push_back(k == f ? 1.0f : 0.0f);
      t += m.frames[f].interval > 0 ? m.frames[f].interval : opts.frame_seconds;
      last = f;
    }
    times.push_back(t);
    for (size_t k = 0; k < frames; k++) weights.push_back(k == last ? 1.0f : 0.0f);
    const int a_time =
        out.accessor(times.data(), times.size() * 4, kFloat, "SCALAR", times.size(),
                     ",\"min\":[" + num(times.front()) + "],\"max\":[" + num(times.back()) + "]");
    const int a_w =
        out.accessor(weights.data(), weights.size() * 4, kFloat, "SCALAR", weights.size());
    animations.push_back("{\"name\":" + quoted(clips.names[c]) + ",\"samplers\":[{\"input\":" +
                         std::to_string(a_time) + ",\"output\":" + std::to_string(a_w) +
                         ",\"interpolation\":\"STEP\"}],\"channels\":[{\"sampler\":0,"
                         "\"target\":{\"node\":0,\"path\":\"weights\"}}]}");
  }
  while (out.bin.size() % 4) out.bin.push_back(0);

  std::vector<std::string> target_names;
  for (const Frame& f : m.frames) target_names.push_back(quoted(f.name));
  std::string prim = "{\"attributes\":" + attributes + ",\"indices\":" + std::to_string(a_idx);
  if (!materials.empty()) prim += ",\"material\":0";
  prim += targets + "}";
  std::string mesh = "{\"name\":" + quoted(name) + ",\"primitives\":[" + prim + "]";
  if (frames > 0) {
    std::vector<std::string> zeros(frames, "0");
    mesh += ",\"weights\":[" + join(zeros) + "],\"extras\":{\"targetNames\":[" +
            join(target_names) + "]}";
  }
  mesh += "}";

  std::string json = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"ge::morph::write_glb\"}";
  json += ",\"scene\":0,\"scenes\":[{\"nodes\":[0]}]";
  json += ",\"nodes\":[{\"name\":" + quoted(name) + ",\"mesh\":0}]";
  json += ",\"meshes\":[" + mesh + "]";
  if (!materials.empty()) {
    json += ",\"materials\":[" + join(materials) + "]";
    json +=
        ",\"samplers\":[{\"magFilter\":9729,\"minFilter\":9987,\"wrapS\":10497,"
        "\"wrapT\":10497}]";
    json += ",\"images\":[" + join(images) + "],\"textures\":[" + join(textures) + "]";
  }
  if (!animations.empty()) json += ",\"animations\":[" + join(animations) + "]";
  json += ",\"buffers\":[{\"byteLength\":" + std::to_string(out.bin.size()) + "}]";
  json += ",\"bufferViews\":[" + join(out.views) + "]";
  json += ",\"accessors\":[" + join(out.accessors) + "]}";
  while (json.size() % 4) json += ' ';

  std::vector<uint8_t> glb;
  auto u32 = [&](uint32_t v) {
    for (int k = 0; k < 4; ++k) glb.push_back(static_cast<uint8_t>(v >> (8 * k)));
  };
  u32(0x46546C67);  // "glTF"
  u32(2);
  u32(static_cast<uint32_t>(12 + 8 + json.size() + 8 + out.bin.size()));
  u32(static_cast<uint32_t>(json.size()));
  u32(0x4E4F534A);  // "JSON"
  glb.insert(glb.end(), json.begin(), json.end());
  u32(static_cast<uint32_t>(out.bin.size()));
  u32(0x004E4942);  // "BIN"
  glb.insert(glb.end(), out.bin.begin(), out.bin.end());
  return glb;
}

}  // namespace ge::morph
