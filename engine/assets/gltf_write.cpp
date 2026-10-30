// The glTF writer's definitions: the binary buffer laid out accessor by accessor, the JSON
// beside it, both in one GLB container.
//
// ge:summary glTF writer definitions: buffer views, accessors, nodes, skin, clips, GLB chunks
// ge:keywords assets gltf glb writer json buffer accessor
// ge:skill assets
// ge:test tests/assets_gltf_test.cpp
#include "assets/gltf_write.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace ge::assets {

namespace {

constexpr int kFloat = 5126, kUByte = 5121, kUInt = 5125;

std::string num(float v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.9g", static_cast<double>(v));
  return b;
}

std::string quoted(const std::string& s) {
  std::string out = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out + "\"";
}

// The binary chunk and the accessors that read it.
struct Out {
  std::vector<uint8_t> bin;
  std::vector<std::string> views, accessors;

  // `bytes` as a buffer view (4-aligned) and an accessor over it; its index.
  int add(const void* bytes, size_t size, int component, const char* type, size_t count,
          const std::string& extra = "") {
    while (bin.size() % 4) bin.push_back(0);
    const size_t offset = bin.size();
    const auto* b = static_cast<const uint8_t*>(bytes);
    bin.insert(bin.end(), b, b + size);
    views.push_back("{\"buffer\":0,\"byteOffset\":" + std::to_string(offset) +
                    ",\"byteLength\":" + std::to_string(size) + "}");
    accessors.push_back("{\"bufferView\":" + std::to_string(views.size() - 1) +
                        ",\"componentType\":" + std::to_string(component) + ",\"count\":" +
                        std::to_string(count) + ",\"type\":\"" + type + "\"" + extra + "}");
    return static_cast<int>(accessors.size() - 1);
  }
};

std::string join(const std::vector<std::string>& items) {
  std::string s;
  for (size_t i = 0; i < items.size(); ++i) s += (i ? "," : "") + items[i];
  return s;
}

std::string list(const float* v, int n) {
  std::string s = "[";
  for (int i = 0; i < n; ++i) s += (i ? "," : "") + num(v[i]);
  return s + "]";
}

// glTF's frame is the engine's mirrored in x (assets/gltf.hpp): the same flips both ways.
void mirror_trs(Trs& t) {
  t.t[0] = 0.0f - t.t[0];
  t.r[1] = -t.r[1];
  t.r[2] = -t.r[2];
}

}  // namespace

std::vector<uint8_t> write_glb(const Model& model) {
  Out out;
  const size_t joints = model.joints.size();
  std::vector<std::string> nodes, meshes, materials, animations;

  for (size_t j = 0; j < joints; ++j) {
    const Joint& joint = model.joints[j];
    Trs t = joint.rest;
    mirror_trs(t);
    std::string node = "{\"name\":" + quoted(joint.name) + ",\"translation\":" + list(t.t, 3) +
                       ",\"rotation\":" + list(t.r, 4) + ",\"scale\":" + list(t.s, 3);
    std::vector<std::string> children;
    for (size_t c = 0; c < joints; ++c) {
      if (model.joints[c].parent == static_cast<int32_t>(j)) children.push_back(std::to_string(c));
    }
    if (!children.empty()) node += ",\"children\":[" + join(children) + "]";
    nodes.push_back(node + "}");
  }

  // The skin of every joint: its inverse binds, column-major, mirrored.
  std::vector<float> ibm(joints * 16, 0.0f);
  for (size_t j = 0; j < joints; ++j) {
    const float* m = model.joints[j].inverse_bind.m;
    float* c = &ibm[j * 16];
    for (int row = 0; row < 3; ++row) {
      for (int col = 0; col < 4; ++col) {
        const float sign = (row == 0) != (col == 0) ? -1.0f : 1.0f;
        c[col * 4 + row] = sign * m[row * 4 + col];
      }
    }
    c[15] = 1;
  }
  std::vector<std::string> skin_joints;
  for (size_t j = 0; j < joints; ++j) skin_joints.push_back(std::to_string(j));
  const int ibm_accessor = out.add(ibm.data(), ibm.size() * 4, kFloat, "MAT4", joints);
  const std::string skins = "[{\"inverseBindMatrices\":" + std::to_string(ibm_accessor) +
                            ",\"joints\":[" + join(skin_joints) + "]}]";

  for (const Material& m : model.materials) {
    const float rgba[4] = {m.rgb[0], m.rgb[1], m.rgb[2], 1};
    materials.push_back("{\"name\":" + quoted(m.name) +
                        ",\"pbrMetallicRoughness\":{\"baseColorFactor\":" + list(rgba, 4) +
                        ",\"metallicFactor\":0}}");
  }

  std::vector<std::string> roots;
  for (size_t j = 0; j < joints; ++j) {
    if (model.joints[j].parent < 0) roots.push_back(std::to_string(j));
  }
  std::vector<size_t> node_of_mesh;
  for (const Mesh& mesh : model.meshes) {
    // One primitive a material, with the vertices its triangles use (in their order in the
    // mesh) as accessors of its own: a vertex's part is its material, so no vertex is shared
    // across two. A triangle goes by its first vertex's part; the mirror turns the winding back
    // over.
    std::vector<std::string> prims;
    for (size_t part = 0; part < std::max<size_t>(model.materials.size(), 1); ++part) {
      std::vector<uint32_t> tris;
      std::vector<int64_t> remap(mesh.vertex_count(), -1);
      for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        if (mesh.parts[mesh.indices[t]] != part) continue;
        for (int k = 0; k < 3; ++k) {
          tris.push_back(mesh.indices[t + k]);
          remap[mesh.indices[t + k]] = 0;
        }
      }
      if (tris.empty()) continue;
      std::vector<float> pos, nrm, wgt;
      std::vector<uint8_t> jnt;
      uint32_t n = 0;
      for (uint32_t v = 0; v < mesh.vertex_count(); ++v) {
        if (remap[v] < 0) continue;
        remap[v] = n++;
        const float* p = &mesh.positions[size_t{v} * 3];
        const float* q = &mesh.normals[size_t{v} * 3];
        pos.insert(pos.end(), {0.0f - p[0], p[1], p[2]});
        nrm.insert(nrm.end(), {0.0f - q[0], q[1], q[2]});
        jnt.insert(jnt.end(), &mesh.joints[size_t{v} * 4], &mesh.joints[size_t{v} * 4] + 4);
        wgt.insert(wgt.end(), &mesh.weights[size_t{v} * 4], &mesh.weights[size_t{v} * 4] + 4);
      }
      float lo[3] = {pos[0], pos[1], pos[2]}, hi[3] = {pos[0], pos[1], pos[2]};
      for (size_t i = 0; i < pos.size(); ++i) {
        lo[i % 3] = std::min(lo[i % 3], pos[i]);
        hi[i % 3] = std::max(hi[i % 3], pos[i]);
      }
      std::vector<uint32_t> idx;
      for (size_t t = 0; t < tris.size(); t += 3) {
        idx.insert(idx.end(), {static_cast<uint32_t>(remap[tris[t]]),
                               static_cast<uint32_t>(remap[tris[t + 2]]),
                               static_cast<uint32_t>(remap[tris[t + 1]])});
      }
      const int a_pos = out.add(pos.data(), pos.size() * 4, kFloat, "VEC3", n,
                                ",\"min\":" + list(lo, 3) + ",\"max\":" + list(hi, 3));
      const int a_nrm = out.add(nrm.data(), nrm.size() * 4, kFloat, "VEC3", n);
      const int a_jnt = out.add(jnt.data(), jnt.size(), kUByte, "VEC4", n);
      const int a_wgt = out.add(wgt.data(), wgt.size() * 4, kFloat, "VEC4", n);
      const int a_idx = out.add(idx.data(), idx.size() * 4, kUInt, "SCALAR", idx.size());
      std::string prim =
          "{\"attributes\":{\"POSITION\":" + std::to_string(a_pos) +
          ",\"NORMAL\":" + std::to_string(a_nrm) + ",\"JOINTS_0\":" + std::to_string(a_jnt) +
          ",\"WEIGHTS_0\":" + std::to_string(a_wgt) + "},\"indices\":" + std::to_string(a_idx);
      if (part < model.materials.size()) prim += ",\"material\":" + std::to_string(part);
      // Each target's offsets for the primitive's vertices, mirrored like the positions.
      std::vector<std::string> targets;
      for (const MorphTarget& t : mesh.targets) {
        std::vector<float> dp, dn;
        for (uint32_t v = 0; v < mesh.vertex_count(); ++v) {
          if (remap[v] < 0) continue;
          const float* p = &t.positions[size_t{v} * 3];
          const float* q = &t.normals[size_t{v} * 3];
          dp.insert(dp.end(), {0.0f - p[0], p[1], p[2]});
          dn.insert(dn.end(), {0.0f - q[0], q[1], q[2]});
        }
        float dlo[3] = {dp[0], dp[1], dp[2]}, dhi[3] = {dp[0], dp[1], dp[2]};
        for (size_t i = 0; i < dp.size(); ++i) {
          dlo[i % 3] = std::min(dlo[i % 3], dp[i]);
          dhi[i % 3] = std::max(dhi[i % 3], dp[i]);
        }
        const int a_dp = out.add(dp.data(), dp.size() * 4, kFloat, "VEC3", n,
                                 ",\"min\":" + list(dlo, 3) + ",\"max\":" + list(dhi, 3));
        const int a_dn = out.add(dn.data(), dn.size() * 4, kFloat, "VEC3", n);
        targets.push_back("{\"POSITION\":" + std::to_string(a_dp) +
                          ",\"NORMAL\":" + std::to_string(a_dn) + "}");
      }
      if (!targets.empty()) prim += ",\"targets\":[" + join(targets) + "]";
      prims.push_back(prim + "}");
    }
    std::string extra;
    if (!mesh.targets.empty()) {
      std::vector<std::string> names;
      for (const MorphTarget& t : mesh.targets) names.push_back(quoted(t.name));
      extra = ",\"weights\":" +
              list(mesh.target_weights.data(), static_cast<int>(mesh.target_weights.size())) +
              ",\"extras\":{\"targetNames\":[" + join(names) + "]}";
    }
    meshes.push_back("{\"name\":" + quoted(mesh.name) + ",\"primitives\":[" + join(prims) + "]" +
                     extra + "}");
    node_of_mesh.push_back(nodes.size());
    roots.push_back(std::to_string(nodes.size()));
    nodes.push_back("{\"name\":" + quoted(mesh.name) +
                    ",\"mesh\":" + std::to_string(meshes.size() - 1) + ",\"skin\":0}");
  }

  for (const Clip& clip : model.clips) {
    if (clip.frames == 0) continue;
    std::vector<float> times(clip.frames);
    for (uint32_t f = 0; f < clip.frames; ++f) times[f] = static_cast<float>(f) / kClipRateHz;
    const float t_lo = times.front(), t_hi = times.back();
    const int a_time = out.add(times.data(), times.size() * 4, kFloat, "SCALAR", clip.frames,
                               ",\"min\":[" + num(t_lo) + "],\"max\":[" + num(t_hi) + "]");
    std::vector<std::string> samplers, channels;
    for (size_t j = 0; j < joints; ++j) {
      std::vector<float> t3, r4, s3;
      for (uint32_t f = 0; f < clip.frames; ++f) {
        Trs t = clip.samples[size_t{f} * joints + j];
        mirror_trs(t);
        t3.insert(t3.end(), t.t, t.t + 3);
        r4.insert(r4.end(), t.r, t.r + 4);
        s3.insert(s3.end(), t.s, t.s + 3);
      }
      const std::pair<const char*, std::vector<float>*> paths[3] = {
          {"translation", &t3}, {"rotation", &r4}, {"scale", &s3}};
      for (const auto& [path, values] : paths) {
        const bool rot = values == &r4;
        const int a_val =
            out.add(values->data(), values->size() * 4, kFloat, rot ? "VEC4" : "VEC3", clip.frames);
        samplers.push_back("{\"input\":" + std::to_string(a_time) + ",\"output\":" +
                           std::to_string(a_val) + ",\"interpolation\":\"LINEAR\"}");
        channels.push_back("{\"sampler\":" + std::to_string(samplers.size() - 1) +
                           ",\"target\":{\"node\":" + std::to_string(j) + ",\"path\":\"" + path +
                           "\"}}");
      }
    }
    for (const MorphTrack& track : clip.morphs) {
      if (track.mesh < 0 || static_cast<size_t>(track.mesh) >= model.meshes.size()) continue;
      const size_t n = model.meshes[static_cast<size_t>(track.mesh)].targets.size();
      if (n == 0 || track.weights.size() < size_t{clip.frames} * n) continue;
      const int a_val = out.add(track.weights.data(), size_t{clip.frames} * n * 4, kFloat, "SCALAR",
                                size_t{clip.frames} * n);
      samplers.push_back("{\"input\":" + std::to_string(a_time) +
                         ",\"output\":" + std::to_string(a_val) + ",\"interpolation\":\"LINEAR\"}");
      channels.push_back(
          "{\"sampler\":" + std::to_string(samplers.size() - 1) +
          ",\"target\":{\"node\":" + std::to_string(node_of_mesh[static_cast<size_t>(track.mesh)]) +
          ",\"path\":\"weights\"}}");
    }
    animations.push_back("{\"name\":" + quoted(clip.name) + ",\"samplers\":[" + join(samplers) +
                         "],\"channels\":[" + join(channels) + "]}");
  }
  while (out.bin.size() % 4) out.bin.push_back(0);

  std::string json = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"ge::assets::write_glb\"}";
  json += ",\"scene\":0,\"scenes\":[{\"nodes\":[" + join(roots) + "]}]";
  json += ",\"nodes\":[" + join(nodes) + "]";
  json += ",\"meshes\":[" + join(meshes) + "]";
  if (!materials.empty()) json += ",\"materials\":[" + join(materials) + "]";
  if (joints) json += ",\"skins\":" + skins;
  if (!animations.empty()) json += ",\"animations\":[" + join(animations) + "]";
  json += ",\"buffers\":[{\"byteLength\":" + std::to_string(out.bin.size()) + "}]";
  json += ",\"bufferViews\":[" + join(out.views) + "]";
  json += ",\"accessors\":[" + join(out.accessors) + "]}";
  while (json.size() % 4) json += ' ';

  // The GLB container: the header, the JSON chunk, the binary chunk.
  std::vector<uint8_t> glb;
  auto u32 = [&](uint32_t v) {
    for (int k = 0; k < 4; ++k) glb.push_back(static_cast<uint8_t>(v >> (8 * k)));
  };
  const auto total = static_cast<uint32_t>(12 + 8 + json.size() + 8 + out.bin.size());
  u32(0x46546C67);  // "glTF"
  u32(2);
  u32(total);
  u32(static_cast<uint32_t>(json.size()));
  u32(0x4E4F534A);  // "JSON"
  glb.insert(glb.end(), json.begin(), json.end());
  u32(static_cast<uint32_t>(out.bin.size()));
  u32(0x004E4942);  // "BIN"
  glb.insert(glb.end(), out.bin.begin(), out.bin.end());
  return glb;
}

}  // namespace ge::assets
