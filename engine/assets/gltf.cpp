// The glTF loader's definitions: cgltf's parse and buffers, then the scene's nodes into joints,
// the skins' inverse binds, the meshes' primitives, the materials and the clips' bake, each
// mirrored into the engine's frame. cgltf's own code is compiled in cgltf.cpp.
//
// ge:summary glTF loader definitions: cgltf parse, joints, skins, meshes, materials, clip bake
// ge:keywords assets gltf cgltf parse mirror bake sampler slerp hermite decompose
// ge:skill assets
// ge:test tests/assets_gltf_test.cpp
#include "assets/gltf.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <tuple>

#include "assets/image.hpp"
#include "core/libm/libm.hpp"

#include <cgltf.h>

namespace ge::assets {

namespace {

// ── The engine's frame: glTF mirrored in x ──

void mirror_point(float p[3]) { p[0] = 0.0f - p[0]; }  // 0 - x: a zero stays +0
void mirror_rotation(float q[4]) {
  q[1] = -q[1];
  q[2] = -q[2];
}
// S M S with S = diag(-1, 1, 1): every element whose row and column differ in x flips.
Affine mirror(const Affine& a) {
  Affine m = a;
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 4; ++col) {
      const bool row_x = row == 0, col_x = col == 0;
      if (row_x != col_x) m.m[row * 4 + col] = -m.m[row * 4 + col];
    }
  }
  return m;
}

// A glTF column-major 4x4 as an Affine (its last row dropped).
Affine from_column_major(const float* c) {
  Affine a;
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 4; ++col) a.m[row * 4 + col] = c[col * 4 + row];
  }
  return a;
}

// An affine with no shear split into TRS: the scale is its columns' lengths, the rotation the
// quaternion of what is left.
Trs decompose(const Affine& a) {
  Trs t;
  float r[9];
  for (int col = 0; col < 3; ++col) {
    const float x = a.m[col], y = a.m[4 + col], z = a.m[8 + col];
    const float len = std::sqrt(x * x + y * y + z * z);
    t.s[col] = len;
    const float inv = len > 0 ? 1.0f / len : 0.0f;
    r[col] = x * inv;
    r[3 + col] = y * inv;
    r[6 + col] = z * inv;
  }
  for (int row = 0; row < 3; ++row) t.t[row] = a.m[row * 4 + 3];
  const float trace = r[0] + r[4] + r[8];
  float* q = t.r;
  if (trace > 0) {
    const float s = 2 * std::sqrt(trace + 1);
    q[3] = s / 4;
    q[0] = (r[7] - r[5]) / s;
    q[1] = (r[2] - r[6]) / s;
    q[2] = (r[3] - r[1]) / s;
  } else if (r[0] > r[4] && r[0] > r[8]) {
    const float s = 2 * std::sqrt(1 + r[0] - r[4] - r[8]);
    q[3] = (r[7] - r[5]) / s;
    q[0] = s / 4;
    q[1] = (r[1] + r[3]) / s;
    q[2] = (r[2] + r[6]) / s;
  } else if (r[4] > r[8]) {
    const float s = 2 * std::sqrt(1 + r[4] - r[0] - r[8]);
    q[3] = (r[2] - r[6]) / s;
    q[0] = (r[1] + r[3]) / s;
    q[1] = s / 4;
    q[2] = (r[5] + r[7]) / s;
  } else {
    const float s = 2 * std::sqrt(1 + r[8] - r[0] - r[4]);
    q[3] = (r[3] - r[1]) / s;
    q[0] = (r[2] + r[6]) / s;
    q[1] = (r[5] + r[7]) / s;
    q[2] = s / 4;
  }
  return t;
}

// ── Sampling an animation channel ──

// The arc between two unit quaternions at f, on the shorter way round.
void slerp(const float a[4], const float b[4], float f, float out[4]) {
  double dot =
      double{a[0]} * b[0] + double{a[1]} * b[1] + double{a[2]} * b[2] + double{a[3]} * b[3];
  const double sb = dot < 0 ? -1.0 : 1.0;
  dot *= sb;
  double wa = 1 - f, wb = f;
  if (dot < 0.9995) {
    const double theta = core::libm::acos(dot);
    const double s = core::libm::sin(theta);
    wa = core::libm::sin((1 - f) * theta) / s;
    wb = core::libm::sin(f * theta) / s;
  }
  double len2 = 0;
  double o[4];
  for (int k = 0; k < 4; ++k) {
    o[k] = wa * a[k] + wb * sb * b[k];
    len2 += o[k] * o[k];
  }
  const double inv = len2 > 0 ? 1.0 / std::sqrt(len2) : 0.0;
  for (int k = 0; k < 4; ++k) out[k] = static_cast<float>(o[k] * inv);
}

// The channel's value at time t into out (n components: 3 or 4), in glTF's frame.
void sample_channel(const cgltf_animation_sampler& s, int n, float t, float out[4]) {
  const cgltf_accessor* in = s.input;
  const cgltf_accessor* val = s.output;
  const auto keys = static_cast<cgltf_size>(in->count);
  const bool cubic = s.interpolation == cgltf_interpolation_type_cubic_spline;
  // Key k's value (cubic: the middle of its in-tangent, value, out-tangent triplet).
  auto value = [&](cgltf_size k, int part, float* v) {
    cgltf_accessor_read_float(val, cubic ? k * 3 + part : k, v, n);
  };
  float t0 = 0, t1 = 0;
  cgltf_accessor_read_float(in, 0, &t0, 1);
  if (keys <= 1 || t <= t0) {
    value(0, 1, out);
    return;
  }
  cgltf_accessor_read_float(in, keys - 1, &t1, 1);
  if (t >= t1) {
    value(keys - 1, 1, out);
    return;
  }
  // The last key at or before t.
  cgltf_size lo = 0, hi = keys - 1;
  while (hi - lo > 1) {
    const cgltf_size mid = (lo + hi) / 2;
    float tm = 0;
    cgltf_accessor_read_float(in, mid, &tm, 1);
    (tm <= t ? lo : hi) = mid;
  }
  float ta = 0, tb = 0;
  cgltf_accessor_read_float(in, lo, &ta, 1);
  cgltf_accessor_read_float(in, hi, &tb, 1);
  const float dt = tb - ta;
  const float f = dt > 0 ? (t - ta) / dt : 0.0f;
  float a[4] = {0, 0, 0, 0}, b[4] = {0, 0, 0, 0};
  value(lo, 1, a);
  value(hi, 1, b);
  if (s.interpolation == cgltf_interpolation_type_step) {
    std::copy(a, a + n, out);
    return;
  }
  if (cubic) {
    float ao[4] = {0, 0, 0, 0}, bi[4] = {0, 0, 0, 0};
    value(lo, 2, ao);
    value(hi, 0, bi);
    const float f2 = f * f, f3 = f2 * f;
    const float h00 = 2 * f3 - 3 * f2 + 1, h10 = f3 - 2 * f2 + f, h01 = -2 * f3 + 3 * f2,
                h11 = f3 - f2;
    float len2 = 0;
    for (int k = 0; k < n; ++k) {
      out[k] = h00 * a[k] + h10 * dt * ao[k] + h01 * b[k] + h11 * dt * bi[k];
      len2 += out[k] * out[k];
    }
    if (n == 4 && len2 > 0) {
      const float inv = 1.0f / std::sqrt(len2);
      for (int k = 0; k < 4; ++k) out[k] *= inv;
    }
    return;
  }
  if (n == 4) {
    slerp(a, b, f, out);
    return;
  }
  for (int k = 0; k < n; ++k) out[k] = a[k] + (b[k] - a[k]) * f;
}

// ── The load ──

struct Loader {
  const cgltf_data* data;
  std::string where;
  LoadResult& result;
  std::vector<int> joint_of_node;  // a node's joint, or -1 outside the scene
  std::vector<bool> bound;         // a joint's inverse bind came from a skin

  bool fail(const std::string& what, const std::string& next) {
    result.ok = false;
    result.error = "assets: " + where + ": " + what + "; next: " + next;
    return false;
  }
  void note(const std::string& line) {
    if (std::find(result.notes.begin(), result.notes.end(), line) == result.notes.end()) {
      result.notes.push_back(line);
    }
  }

  // Every node of the scene, depth first, parents before children.
  bool joints() {
    const cgltf_scene* scene =
        data->scene ? data->scene : (data->scenes_count ? data->scenes : nullptr);
    joint_of_node.assign(data->nodes_count, -1);
    std::vector<const cgltf_node*> stack;
    if (scene) {
      for (cgltf_size i = scene->nodes_count; i-- > 0;) stack.push_back(scene->nodes[i]);
    } else {
      for (cgltf_size i = data->nodes_count; i-- > 0;) {
        if (!data->nodes[i].parent) stack.push_back(&data->nodes[i]);
      }
    }
    Model& m = result.model;
    while (!stack.empty()) {
      const cgltf_node* node = stack.back();
      stack.pop_back();
      const cgltf_size index = cgltf_node_index(data, node);
      if (joint_of_node[index] >= 0) continue;
      if (m.joints.size() >= kMaxJoints) {
        return fail("the scene has more than " + std::to_string(kMaxJoints) + " nodes",
                    "join meshes or delete empty nodes before export (assets skill)");
      }
      joint_of_node[index] = static_cast<int>(m.joints.size());
      Joint j;
      j.name = node->name ? node->name : "node" + std::to_string(index);
      j.parent = node->parent ? joint_of_node[cgltf_node_index(data, node->parent)] : -1;
      if (node->has_matrix) {
        j.rest = decompose(from_column_major(node->matrix));
      } else {
        if (node->has_translation) std::copy(node->translation, node->translation + 3, j.rest.t);
        if (node->has_rotation) std::copy(node->rotation, node->rotation + 4, j.rest.r);
        if (node->has_scale) std::copy(node->scale, node->scale + 3, j.rest.s);
      }
      mirror_point(j.rest.t);
      mirror_rotation(j.rest.r);
      if (node->camera || node->light) note("cameras and lights are skipped");
      m.joints.push_back(j);
      for (cgltf_size c = node->children_count; c-- > 0;) stack.push_back(node->children[c]);
    }
    // The default inverse binds: each joint's rest, inverted.
    std::vector<Affine> global(m.joints.size());
    for (size_t j = 0; j < m.joints.size(); ++j) {
      const Affine l = to_affine(m.joints[j].rest);
      const int32_t p = m.joints[j].parent;
      global[j] = p >= 0 ? compose(global[static_cast<size_t>(p)], l) : l;
      m.joints[j].inverse_bind = invert(global[j]);
    }
    bound.assign(m.joints.size(), false);
    return true;
  }

  bool skins() {
    Model& m = result.model;
    for (cgltf_size s = 0; s < data->skins_count; ++s) {
      const cgltf_skin& skin = data->skins[s];
      for (cgltf_size i = 0; i < skin.joints_count; ++i) {
        const int j = joint_of_node[cgltf_node_index(data, skin.joints[i])];
        if (j < 0 || !skin.inverse_bind_matrices) continue;
        float c[16];
        cgltf_accessor_read_float(skin.inverse_bind_matrices, i, c, 16);
        const Affine ib = mirror(from_column_major(c));
        if (bound[j]) {
          float d = 0;
          for (int e = 0; e < 12; ++e)
            d = std::max(d, std::fabs(ib.m[e] - m.joints[j].inverse_bind.m[e]));
          if (d > 1e-4f) {
            return fail("joint '" + m.joints[j].name + "' has two different inverse binds",
                        "bind every skin in one rest pose before export (assets skill)");
          }
        }
        m.joints[j].inverse_bind = ib;
        bound[j] = true;
      }
    }
    return true;
  }

  // The part a primitive's material is, adding the material the first time.
  std::vector<int> part_of_material;
  int default_part = -1;
  int part(const cgltf_material* mat) {
    Model& m = result.model;
    if (!mat) {
      if (default_part < 0) {
        default_part = static_cast<int>(m.materials.size());
        m.materials.push_back(Material{"default", {1, 1, 1}});
      }
      return default_part;
    }
    const cgltf_size i = cgltf_material_index(data, mat);
    if (part_of_material[i] < 0) {
      part_of_material[i] = static_cast<int>(m.materials.size());
      Material out;
      out.name = mat->name ? mat->name : "material" + std::to_string(i);
      if (mat->has_pbr_metallic_roughness) {
        const auto& pbr = mat->pbr_metallic_roughness;
        std::copy(pbr.base_color_factor, pbr.base_color_factor + 3, out.rgb);
      }
      m.materials.push_back(out);
    }
    return part_of_material[i];
  }

  // ── Base colour textures, baked into parts ──
  //
  // A textured material's every triangle takes the colour of the texel at its middle (the
  // texture's colour, linear, times the base colour factor) as a part of its own, one part a
  // colour: a low-poly kit's colour atlas (flat swatches, a NEAREST sampler) draws exactly, and
  // every pass that draws parts draws it, with no texture on the GPU. A face whose corners read
  // other colours than its middle (a photograph's texture) keeps its middle's, and a note says so.

  // A material's texture: its image, which set of coordinates, and the transform on them
  // (u' = a u + b v + c, v' = d u + e v + f).
  struct Texture {
    const Image* image = nullptr;
    int texcoord = 0;
    float xf[6] = {1, 0, 0, 0, 1, 0};
  };
  std::map<cgltf_size, Image> images;                    // decoded once an image
  std::vector<cgltf_size> unread;                        // images that could not be
  std::map<std::tuple<int, uint32_t>, int> baked_parts;  // (source part, sRGB colour) to part

  const Image* image_of(const cgltf_image* img) {
    const cgltf_size i = cgltf_image_index(data, img);
    if (const auto it = images.find(i); it != images.end()) return &it->second;
    if (std::find(unread.begin(), unread.end(), i) != unread.end()) return nullptr;
    if (!img->buffer_view) {
      note(
          "a texture outside the file's buffers (a URI image) is skipped: its base colour factor "
          "is used");
    } else if (const uint8_t* bytes = cgltf_buffer_view_data(img->buffer_view)) {
      if (std::optional<Image> out = decode_png({bytes, img->buffer_view->size})) {
        return &images.emplace(i, std::move(*out)).first->second;
      }
    }
    if (img->buffer_view) {
      note(
          "a texture that is not an 8-bit PNG (a JPEG, a 16-bit or interlaced PNG) is "
          "skipped: its base colour factor is used");
    }
    unread.push_back(i);
    return nullptr;
  }

  // The texture a primitive's material draws its base colour from, if one can be read.
  std::optional<Texture> texture_of(const cgltf_material* mat) {
    if (!mat || !mat->has_pbr_metallic_roughness) return {};
    const cgltf_texture_view& view = mat->pbr_metallic_roughness.base_color_texture;
    if (!view.texture || !view.texture->image) return {};
    Texture t;
    t.image = image_of(view.texture->image);
    if (!t.image) return {};
    t.texcoord = view.texcoord;
    if (view.has_transform) {
      const cgltf_texture_transform& x = view.transform;
      if (x.has_texcoord) t.texcoord = x.texcoord;
      // KHR_texture_transform: scaled, then rotated, then offset.
      const auto c = static_cast<float>(core::libm::cos(x.rotation)),
                 s = static_cast<float>(core::libm::sin(x.rotation));
      const float xf[6] = {c * x.scale[0],  s * x.scale[1], x.offset[0],
                           -s * x.scale[0], c * x.scale[1], x.offset[1]};
      std::copy(xf, xf + 6, t.xf);
    }
    return t;
  }

  // The part for the colour `px` (sRGB bytes) of a face of source part `source`.
  int baked_part(int source, const uint8_t* px) {
    Model& m = result.model;
    const uint32_t key = (uint32_t{px[0]} << 16) | (uint32_t{px[1]} << 8) | px[2];
    const auto it = baked_parts.find({source, key});
    if (it != baked_parts.end()) return it->second;
    const Material& base = m.materials[static_cast<size_t>(source)];
    float rgb[3];
    for (int k = 0; k < 3; ++k) rgb[k] = srgb_to_linear(px[k]) * base.rgb[k];
    if (m.materials.size() >= size_t{kMaxParts}) {
      // Out of parts: the nearest colour already baked.
      note("a texture has more colours than the model has parts (" + std::to_string(kMaxParts) +
           "): the rest take the nearest");
      int best = source;
      float best_d = 1e30f;
      for (const auto& [k, p] : baked_parts) {
        const float* c = m.materials[static_cast<size_t>(p)].rgb;
        const float d = (c[0] - rgb[0]) * (c[0] - rgb[0]) + (c[1] - rgb[1]) * (c[1] - rgb[1]) +
                        (c[2] - rgb[2]) * (c[2] - rgb[2]);
        if (d < best_d) best_d = d, best = p;
      }
      return best;
    }
    char hex[8];
    std::snprintf(hex, sizeof hex, "#%02x%02x%02x", px[0], px[1], px[2]);
    Material out;
    out.name = base.name + hex;
    std::copy(rgb, rgb + 3, out.rgb);
    const int p = static_cast<int>(m.materials.size());
    m.materials.push_back(out);
    baked_parts.emplace(std::make_tuple(source, key), p);
    return p;
  }

  // Each triangle of the primitive whose vertices start at `base` and whose indices start at
  // `first` takes its texel's part; a vertex two colours share is split, one copy a colour.
  void bake(Mesh& mesh, const Texture& tex, const std::vector<float>& uvs, uint32_t base,
            size_t first, int source) {
    std::map<std::pair<uint32_t, int>, uint32_t> copies;  // (vertex, part) to its copy
    std::vector<int> owner(mesh.vertex_count() - base, -1);
    size_t mixed = 0, faces = 0;
    auto at = [&](uint32_t v, float out[2]) {
      const float u = uvs[size_t{v - base} * 2], w = uvs[size_t{v - base} * 2 + 1];
      out[0] = tex.xf[0] * u + tex.xf[1] * w + tex.xf[2];
      out[1] = tex.xf[3] * u + tex.xf[4] * w + tex.xf[5];
    };
    for (size_t t = first; t + 2 < mesh.indices.size(); t += 3) {
      float c[3][2];
      for (int k = 0; k < 3; ++k) at(mesh.indices[t + static_cast<size_t>(k)], c[k]);
      const float mu = (c[0][0] + c[1][0] + c[2][0]) / 3, mv = (c[0][1] + c[1][1] + c[2][1]) / 3;
      const uint8_t* px = texel(*tex.image, mu, mv);
      for (const auto& k : c) {
        const uint8_t* q = texel(*tex.image, k[0], k[1]);
        if (q[0] != px[0] || q[1] != px[1] || q[2] != px[2]) {
          ++mixed;
          break;
        }
      }
      ++faces;
      const int p = baked_part(source, px);
      for (int k = 0; k < 3; ++k) {
        uint32_t& v = mesh.indices[t + static_cast<size_t>(k)];
        int& own = owner[v - base];
        if (own < 0 || own == p) {
          own = p;
          mesh.parts[v] = static_cast<uint8_t>(p);
          continue;
        }
        const auto key = std::make_pair(v, p);
        auto found = copies.find(key);
        if (found == copies.end()) {
          const uint32_t n = mesh.vertex_count();
          mesh.positions.insert(mesh.positions.end(),
                                {mesh.positions[size_t{v} * 3], mesh.positions[size_t{v} * 3 + 1],
                                 mesh.positions[size_t{v} * 3 + 2]});
          mesh.normals.insert(mesh.normals.end(),
                              {mesh.normals[size_t{v} * 3], mesh.normals[size_t{v} * 3 + 1],
                               mesh.normals[size_t{v} * 3 + 2]});
          for (int j = 0; j < kJointsPerVertex; ++j) {
            mesh.joints.push_back(mesh.joints[size_t{v} * 4 + static_cast<size_t>(j)]);
            mesh.weights.push_back(mesh.weights[size_t{v} * 4 + static_cast<size_t>(j)]);
          }
          mesh.parts.push_back(static_cast<uint8_t>(p));
          found = copies.emplace(key, n).first;
        }
        v = found->second;
      }
    }
    if (mixed * 10 > faces) {
      note("a texture varies across its faces (" + std::to_string(mixed) + " of " +
           std::to_string(faces) + "): each face is drawn in the colour at its middle");
    }
  }

  bool meshes() {
    Model& m = result.model;
    part_of_material.assign(data->materials_count, -1);
    for (cgltf_size n = 0; n < data->nodes_count; ++n) {
      const cgltf_node& node = data->nodes[n];
      const int self = joint_of_node[n];
      if (!node.mesh || self < 0) continue;
      Mesh mesh;
      mesh.name =
          node.name ? node.name : (node.mesh->name ? node.mesh->name : "mesh" + std::to_string(n));
      const cgltf_skin* skin = node.skin;
      // A rigid part's vertices, from its node's space to the space its joint binds them in.
      const Affine rigid = invert(m.joints[self].inverse_bind);
      bool faceted = false;
      for (cgltf_size p = 0; p < node.mesh->primitives_count; ++p) {
        const cgltf_primitive& prim = node.mesh->primitives[p];
        if (prim.type != cgltf_primitive_type_triangles) {
          return fail("mesh '" + mesh.name + "' has a primitive of points, lines or strips",
                      "triangulate it before export (assets skill)");
        }
        if (prim.targets_count) note("morph targets are skipped");
        const std::optional<Texture> tex = texture_of(prim.material);
        const cgltf_accessor *pos = nullptr, *nrm = nullptr, *jnt = nullptr, *wgt = nullptr,
                             *uv = nullptr;
        for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
          const cgltf_attribute& at = prim.attributes[a];
          if (tex && at.type == cgltf_attribute_type_texcoord && at.index == tex->texcoord)
            uv = at.data;
          if (at.type == cgltf_attribute_type_position) pos = at.data;
          if (at.type == cgltf_attribute_type_normal) nrm = at.data;
          if (at.type == cgltf_attribute_type_joints && at.index == 0) jnt = at.data;
          if (at.type == cgltf_attribute_type_weights && at.index == 0) wgt = at.data;
          if ((at.type == cgltf_attribute_type_joints || at.type == cgltf_attribute_type_weights) &&
              at.index > 0) {
            note("influences past the first four are skipped");
          }
        }
        if (!pos) {
          return fail("mesh '" + mesh.name + "' has a primitive with no POSITION",
                      "export the mesh with its positions (assets skill)");
        }
        if (!nrm) faceted = true;
        const bool skinned = skin && jnt && wgt;
        const auto base = static_cast<uint32_t>(mesh.vertex_count());
        const auto material = static_cast<uint8_t>(part(prim.material));
        std::vector<float> uvs;
        if (tex && uv) {
          uvs.resize(pos->count * 2);
          for (cgltf_size v = 0; v < pos->count; ++v)
            cgltf_accessor_read_float(uv, v, &uvs[v * 2], 2);
        } else if (tex) {
          note("a textured primitive has no texture coordinates: its base colour factor is used");
        }
        const size_t first = mesh.indices.size();
        for (cgltf_size v = 0; v < pos->count; ++v) {
          float p3[3] = {0, 0, 0}, n3[3] = {0, 1, 0};
          cgltf_accessor_read_float(pos, v, p3, 3);
          if (nrm) cgltf_accessor_read_float(nrm, v, n3, 3);
          uint8_t js[4] = {static_cast<uint8_t>(self), 0, 0, 0};
          float ws[4] = {1, 0, 0, 0};
          mirror_point(p3);
          mirror_point(n3);
          if (skinned) {
            cgltf_uint ji[4] = {0, 0, 0, 0};
            cgltf_accessor_read_uint(jnt, v, ji, 4);
            cgltf_accessor_read_float(wgt, v, ws, 4);
            float sum = 0;
            for (int k = 0; k < 4; ++k) {
              const int j = ji[k] < skin->joints_count
                                ? joint_of_node[cgltf_node_index(data, skin->joints[ji[k]])]
                                : -1;
              if (j < 0) ws[k] = 0;
              js[k] = static_cast<uint8_t>(std::max(j, 0));
              ws[k] = std::max(ws[k], 0.0f);
              sum += ws[k];
            }
            if (sum > 0) {
              for (float& w : ws) w /= sum;
            } else {
              js[0] = static_cast<uint8_t>(self);
              ws[0] = 1;
            }
          } else {
            float q[3];
            transform_point(rigid, p3, q);
            std::copy(q, q + 3, p3);
            transform_vector(rigid, n3, q);
            const float len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
            for (int k = 0; k < 3; ++k) n3[k] = len > 0 ? q[k] / len : n3[k];
          }
          mesh.positions.insert(mesh.positions.end(), p3, p3 + 3);
          mesh.normals.insert(mesh.normals.end(), n3, n3 + 3);
          mesh.joints.insert(mesh.joints.end(), js, js + 4);
          mesh.weights.insert(mesh.weights.end(), ws, ws + 4);
          mesh.parts.push_back(material);
        }
        const cgltf_size count = prim.indices ? prim.indices->count : pos->count;
        for (cgltf_size i = 0; i + 2 < count; i += 3) {
          uint32_t t[3];
          for (int k = 0; k < 3; ++k) {
            t[k] = prim.indices
                       ? static_cast<uint32_t>(cgltf_accessor_read_index(prim.indices, i + k))
                       : static_cast<uint32_t>(i + k);
          }
          // The mirror turns the winding over: swap the last two to keep the front outside.
          mesh.indices.insert(mesh.indices.end(), {base + t[0], base + t[2], base + t[1]});
        }
        if (!uvs.empty()) bake(mesh, *tex, uvs, base, first, material);
      }
      if (faceted) make_faceted(mesh);
      m.meshes.push_back(std::move(mesh));
    }
    return true;
  }

  // Each triangle its own three vertices and its face's normal.
  static void make_faceted(Mesh& mesh) {
    Mesh f;
    f.name = mesh.name;
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
      const float* a = &mesh.positions[size_t{mesh.indices[t]} * 3];
      const float* b = &mesh.positions[size_t{mesh.indices[t + 1]} * 3];
      const float* c = &mesh.positions[size_t{mesh.indices[t + 2]} * 3];
      const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
      const float v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
      float n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
                    u[0] * v[1] - u[1] * v[0]};
      const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
      for (float& e : n) e = len > 0 ? e / len : 0.0f;
      for (int k = 0; k < 3; ++k) {
        const uint32_t i = mesh.indices[t + k];
        f.indices.push_back(f.vertex_count());
        f.positions.insert(f.positions.end(), &mesh.positions[size_t{i} * 3],
                           &mesh.positions[size_t{i} * 3] + 3);
        f.normals.insert(f.normals.end(), n, n + 3);
        f.joints.insert(f.joints.end(), &mesh.joints[size_t{i} * 4],
                        &mesh.joints[size_t{i} * 4] + 4);
        f.weights.insert(f.weights.end(), &mesh.weights[size_t{i} * 4],
                         &mesh.weights[size_t{i} * 4] + 4);
        f.parts.push_back(mesh.parts[i]);
      }
    }
    mesh = std::move(f);
  }

  bool clips() {
    Model& m = result.model;
    const size_t joints = m.joints.size();
    for (cgltf_size a = 0; a < data->animations_count; ++a) {
      const cgltf_animation& anim = data->animations[a];
      float end = 0;
      for (cgltf_size c = 0; c < anim.channels_count; ++c) {
        const cgltf_animation_sampler* s = anim.channels[c].sampler;
        if (s && s->input && s->input->count) {
          float t = 0;
          cgltf_accessor_read_float(s->input, s->input->count - 1, &t, 1);
          end = std::max(end, t);
        }
      }
      Clip clip;
      clip.name = anim.name ? anim.name : "clip" + std::to_string(a);
      clip.frames = static_cast<uint32_t>(std::lround(end * kClipRateHz)) + 1;
      clip.samples.resize(size_t{clip.frames} * joints);
      for (uint32_t f = 0; f < clip.frames; ++f) {
        Trs* row = &clip.samples[size_t{f} * joints];
        for (size_t j = 0; j < joints; ++j) row[j] = m.joints[j].rest;
        const float t = static_cast<float>(f) / kClipRateHz;
        for (cgltf_size c = 0; c < anim.channels_count; ++c) {
          const cgltf_animation_channel& ch = anim.channels[c];
          if (!ch.target_node || !ch.sampler) continue;
          const int j = joint_of_node[cgltf_node_index(data, ch.target_node)];
          if (j < 0) continue;
          Trs& o = row[j];
          switch (ch.target_path) {
            case cgltf_animation_path_type_translation:
              sample_channel(*ch.sampler, 3, t, o.t);
              mirror_point(o.t);
              break;
            case cgltf_animation_path_type_rotation:
              sample_channel(*ch.sampler, 4, t, o.r);
              mirror_rotation(o.r);
              break;
            case cgltf_animation_path_type_scale:
              sample_channel(*ch.sampler, 3, t, o.s);
              break;
            default:
              note("animations of morph weights are skipped");
              break;
          }
        }
      }
      m.clips.push_back(std::move(clip));
    }
    return true;
  }
};

const char* cgltf_why(cgltf_result r) {
  switch (r) {
    case cgltf_result_data_too_short:
      return "the data ends early";
    case cgltf_result_unknown_format:
      return "it is neither glTF JSON nor GLB";
    case cgltf_result_invalid_json:
      return "its JSON does not parse";
    case cgltf_result_invalid_gltf:
      return "it is not valid glTF 2.0";
    case cgltf_result_invalid_options:
      return "the loader's options are invalid";
    case cgltf_result_file_not_found:
      return "a buffer it names is not found";
    case cgltf_result_io_error:
      return "a buffer could not be read";
    case cgltf_result_out_of_memory:
      return "out of memory";
    case cgltf_result_legacy_gltf:
      return "it is glTF 1.0";
    default:
      return "cgltf could not read it";
  }
}

}  // namespace

LoadResult load_gltf(std::span<const uint8_t> bytes, std::string_view where) {
  LoadResult result;
  const std::string name(where);
  cgltf_options options{};
  cgltf_data* data = nullptr;
  cgltf_result r = cgltf_parse(&options, bytes.data(), bytes.size(), &data);
  if (r == cgltf_result_success) r = cgltf_load_buffers(&options, data, name.c_str());
  if (r == cgltf_result_success) r = cgltf_validate(data);
  if (r != cgltf_result_success) {
    result.error =
        "assets: " + name + ": " + cgltf_why(r) +
        "; next: re-export it as glTF 2.0 (.glb) and check it with a validator (assets skill)";
    cgltf_free(data);
    return result;
  }
  result.ok = true;
  Loader l{data, name, result, {}, {}, {}, -1, {}, {}, {}};
  if (l.joints() && l.skins() && l.meshes()) l.clips();
  if (result.ok && result.model.meshes.empty()) {
    l.fail("it has no mesh in its scene", "export the objects with their meshes; skill: assets");
  }
  cgltf_free(data);
  if (!result.ok) result.model = Model{};
  return result;
}

LoadResult load_gltf_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    LoadResult r;
    r.error =
        "assets: " + path + ": the file cannot be opened; next: check the path (assets skill)";
    return r;
  }
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                                   std::istreambuf_iterator<char>());
  return load_gltf(bytes, path);
}

}  // namespace ge::assets
