// The window's models (world_models.hpp).
//
// ge:summary The window's models: items, weapons, flames, players, loaded from the packs and drawn
// ge:keywords world-models create_models place_models prepare_models encode_models
// ge:skill render shooter morph
#include "shooters_edge/client/world_models.hpp"

#include <algorithm>
#include <cmath>
#include <set>

#include "brush/read.hpp"
#include "classicenhance/models.hpp"
#include "core/output.hpp"
#include "morph/read.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::client {

namespace gr = ge::render;
namespace mo = ge::morph;
namespace br = ge::brush;

namespace {

constexpr uint32_t kSpin = 8;           // a model file's flag: the item turns
constexpr double kSpinDegPerS = 100;    // how fast
constexpr float kAmbientShare = 0.75f;  // of the floor's light, from every side
constexpr float kShadeShare = 0.6f;     // and from above and ahead
constexpr float kRelightUnits = 1;      // a move that takes the light again

bool ends_with(const std::string& s, const char* tail) {
  const size_t n = std::char_traits<char>::length(tail);
  return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}

uint32_t colour_row(const sv::ClientInfo& c, const char* key, uint32_t fallback) {
  const std::string v = sv::info_value(c.userinfo, key);
  if (v.empty()) return fallback;
  int n = 0;
  for (char ch : v) {
    if (ch < '0' || ch > '9') break;
    n = n * 10 + (ch - '0');
    if (n > 99) break;
  }
  return uint32_t(std::clamp(n, 0, int(models::kColourRows) - 1));
}

void to_world(const sh::Vec3f& p, double out[3]) {
  const float f[3] = {p.x, p.y, p.z};
  br::to_engine(f, out);
}

}  // namespace

void place_models(const Listen& l, std::vector<Placed>& out) {
  out.clear();
  if (!l.server) return;
  const sv::QcServer& s = *l.server;
  const int me = player_state(l).entity;
  const int models = int(s.models.size());
  for (int e = 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& ed = s.sv.edicts[size_t(e)];
    if (ed.free || ed.modelindex <= 0 || ed.modelindex >= models || e == me) continue;
    Placed p;
    p.entity = e;
    p.modelindex = ed.modelindex;
    p.frame = int(field_named(l, e, "frame"));
    p.skin = int(field_named(l, e, "skin"));
    p.effects = int(field_named(l, e, "effects"));
    p.origin = ed.origin;
    p.angles = ed.angles;
    if (e <= sh::kMaxClients) {
      const sv::ClientInfo& c = s.clients[size_t(e - 1)];
      p.top = colour_row(c, "topcolor", 0);
      p.bottom = colour_row(c, "bottomcolor", 0);
    }
    out.push_back(p);
  }
  for (size_t i = 0; i < s.statics.size(); i++) {
    const sv::StaticEntity& st = s.statics[i];
    if (st.modelindex <= 0 || st.modelindex >= models) continue;
    Placed p;
    p.entity = -1 - int(i);
    p.modelindex = st.modelindex;
    p.frame = int(st.frame);
    p.skin = int(st.skin);
    p.origin = st.origin;
    p.angles = st.angles;
    out.push_back(p);
  }
}

void create_models(ge::gpu::Device& d, ge::gpu::PipelineSet& pipelines, WorldModels& m,
                   const wgpu::BindGroupLayout& frame, const SearchPath& files,
                   std::span<const uint8_t> palette, const br::Map& map, const ModelLoad& load) {
  std::vector<gr::MorphModel> morphs;
  std::vector<gr::SpriteFrame> frames;
  std::set<std::string> refused;
  m.kinds.assign(load.names.size(), {});
  for (size_t i = 0; i < load.names.size(); i++) {
    const std::string& name = load.names[i];
    ModelKind& k = m.kinds[i];
    // [1] is the map itself and "*N" its own brush models: the map's to draw, not these.
    if (i == 1 || name.empty() || name[0] == '*') continue;
    std::vector<uint8_t> store;
    const std::span<const uint8_t> bytes = find_file(files, name, store);
    if (bytes.empty()) continue;  // the map itself is not in the packs by this name either
    std::string error;
    if (ends_with(name, ".mdl")) {
      mo::Model model;
      if (!mo::read_model(bytes, name, &model, &error)) {
        if (refused.insert(name).second)
          ge::core::output::status("shooters_edge: model " + name + " not drawn: " + error);
        continue;
      }
      if (load.hide_flames && (ends_with(name, "/flame.mdl") || ends_with(name, "/flame2.mdl")))
        model = ge::classicenhance::drop_glowing(model);
      k.is = ModelKind::Is::alias;
      k.first = uint32_t(morphs.size());
      k.frames = models::frame_groups(model);
      k.skins = models::skin_groups(model);
      k.spin = (model.flags & kSpin) != 0;
      k.synced = model.synced;
      for (size_t s = 0; s < std::max<size_t>(1, model.skins.size()); s++)
        morphs.push_back(models::morph_from_alias(model, palette, int(s)));
    } else if (ends_with(name, ".spr")) {
      mo::Sprite sprite;
      if (!mo::read_sprite(bytes, name, &sprite, &error)) {
        if (refused.insert(name).second)
          ge::core::output::status("shooters_edge: sprite " + name + " not drawn: " + error);
        continue;
      }
      k.is = ModelKind::Is::sprite;
      k.first = uint32_t(frames.size());
      k.frames = models::sprite_groups(sprite);
      k.facing = models::facing_of(sprite.facing);
      for (gr::SpriteFrame& f : models::sprite_frames_of(sprite, palette))
        frames.push_back(std::move(f));
    } else if (ends_with(name, ".bsp")) {
      br::Map item;
      if (!br::read_bsp(bytes, &item, &error)) {
        if (refused.insert(name).second)
          ge::core::output::status("shooters_edge: item " + name + " not drawn: " + error);
        continue;
      }
      gr::MorphModel mm = models::morph_from_brush(item, palette);
      if (mm.indices.empty()) continue;
      k.is = ModelKind::Is::brush;
      k.first = uint32_t(morphs.size());
      morphs.push_back(std::move(mm));
    }
  }
  if (!morphs.empty()) {
    m.has_morph =
        gr::create_morph(d, pipelines, m.morph, frame, morphs, models::palette_ramps(palette));
    if (!m.has_morph)
      ge::core::output::status(
          "shooters_edge: the models' skins do not fit the atlas; models not drawn");
  }
  if (!frames.empty()) {
    m.has_sprites = gr::create_sprites(d, pipelines, m.sprites, frame, frames);
    if (!m.has_sprites)
      ge::core::output::status("shooters_edge: the sprites do not fit the atlas; not drawn");
  }
  if (!map.models.empty()) m.probe = models::make_probe(map);
}

void prepare_models(ge::gpu::Device& d, WorldModels& m, const br::Map& map,
                    std::span<const Placed> placed, const gr::FrameView& view, double brightness,
                    double minlight, bool smooth) {
  m.morph_draws.clear();
  m.sprite_draws.clear();
  m.near.clear();
  for (size_t i = 0; i < placed.size(); i++) {
    const Placed& p = placed[i];
    if (size_t(p.modelindex) >= m.kinds.size()) continue;
    if (m.kinds[size_t(p.modelindex)].is == ModelKind::Is::none) continue;
    double w[3];
    to_world(p.origin, w);
    const double dx = w[0] - view.eye[0], dy = w[1] - view.eye[1], dz = w[2] - view.eye[2];
    m.near.push_back({dx * dx + dy * dy + dz * dz, i});
  }
  // The nearest first, so a crowded map drops the far ones.
  std::sort(m.near.begin(), m.near.end());
  const double t = view.time_s;
  for (const auto& [dist, i] : m.near) {
    const Placed& p = placed[i];
    const ModelKind& k = m.kinds[size_t(p.modelindex)];
    double w[3];
    to_world(p.origin, w);
    // Unsynced copies each start their loop at their own moment.
    const double at = k.synced ? t : t + double(uint32_t(p.entity) * 2654435761u % 1000) * 1e-3;
    if (k.is == ModelKind::Is::sprite) {
      if (!m.has_sprites || m.sprite_draws.size() >= gr::kMaxSprites) continue;
      gr::SpriteDraw s;
      s.frame = k.first + models::frame_in(k.frames, p.frame, at);
      for (int c = 0; c < 3; c++) s.position[c] = s.last[c] = w[c];
      s.facing = k.facing;
      const float ang[3] = {p.angles.x, p.angles.y, p.angles.z};
      models::rotation_of(ang, false, s.rotation);
      s.roll = float(p.angles.z * 3.14159265358979323846 / 180);
      s.scale = float(br::kUnitM);
      s.glow = 1;  // a sprite lights itself
      s.outdoor = 0;
      s.smooth = smooth;
      m.sprite_draws.push_back(s);
      continue;
    }
    if (!m.has_morph || m.morph_draws.size() >= gr::kMaxMorphDraws) continue;
    gr::MorphDraw g;
    g.model = k.first;
    uint32_t frame = 0;
    if (k.is == ModelKind::Is::alias) {
      g.model += models::frame_in(k.skins, p.skin, at);
      frame = models::frame_in(k.frames, p.frame, at);
    }
    for (int c = 0; c < 3; c++) g.now.position[c] = w[c];
    float ang[3] = {p.angles.x, p.angles.y, p.angles.z};
    if (k.spin) ang[1] = float(std::fmod(t * kSpinDegPerS, 360.0));
    models::rotation_of(ang, k.is == ModelKind::Is::alias, g.now.rotation);
    g.now.frame_a = g.now.frame_b = frame;
    const auto last = m.last_pose.find(p.entity);
    g.last = last == m.last_pose.end() ? g.now : last->second;
    m.last_pose[p.entity] = g.now;
    g.tint[0] = p.top;
    g.tint[1] = p.bottom;
    // The light on the floor under it, taken again when it has moved.
    auto [found, fresh] = m.lit.try_emplace(p.entity);
    WorldModels::Lit& lit = found->second;
    const float dm = std::abs(lit.at.x - p.origin.x) + std::abs(lit.at.y - p.origin.y) +
                     std::abs(lit.at.z - p.origin.z);
    if (fresh || dm > kRelightUnits) {
      const float above[3] = {p.origin.x, p.origin.y, p.origin.z + 8};
      if (!models::light_under(m.probe, map, above, lit.rgb))
        lit.rgb[0] = lit.rgb[1] = lit.rgb[2] = 1;
      lit.at = p.origin;
    }
    for (int c = 0; c < 3; c++) {
      // The world's own soft floor (bsp_draw's r_minlight), so a thing in a dark corner reads
      // as the walls round it do.
      const double raw = lit.rgb[c];
      const float l = float(std::sqrt(raw * raw + minlight * minlight) * brightness);
      g.ambient[c] = kAmbientShare * l;
      g.shade[c] = kShadeShare * l;
    }
    // From above and a little toward the eye, so a face turned to the player is never black.
    const double len = std::sqrt(dist) + 1e-9;
    const double sd[3] = {-0.3 * (w[0] - view.eye[0]) / len, 0.9,
                          -0.3 * (w[2] - view.eye[2]) / len};
    const double sl = std::sqrt(sd[0] * sd[0] + sd[1] * sd[1] + sd[2] * sd[2]);
    for (int c = 0; c < 3; c++) g.shade_dir[c] = float(sd[c] / sl);
    g.outdoor = 0;
    g.glow = 1;
    g.smooth = smooth;
    g.light_in_white = true;
    m.morph_draws.push_back(g);
  }
  if (m.has_morph) gr::prepare_morph(d, m.morph, m.morph_draws, view);
  if (m.has_sprites) gr::prepare_sprites(d, m.sprites, m.sprite_draws, view);
}

void encode_models(const WorldModels& m, const ge::gpu::PipelineSet& pipelines,
                   gr::FrameRecorder& rec, wgpu::CommandEncoder& e, const gr::FrameTargets& targets,
                   const wgpu::BindGroup& frame) {
  if (m.has_morph) gr::encode_morph(m.morph, pipelines, rec, e, targets, frame);
}

void encode_model_sprites(const WorldModels& m, const ge::gpu::PipelineSet& pipelines,
                          gr::FrameRecorder& rec, wgpu::CommandEncoder& e,
                          const gr::FrameTargets& targets, const wgpu::BindGroup& frame) {
  if (m.has_sprites) gr::encode_sprites(m.sprites, pipelines, rec, e, targets, frame);
}

}  // namespace shooters_edge::client
