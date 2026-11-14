// The window's models (world_models.hpp).
//
// ge:summary The window's models: items, weapons, flames, players, loaded from the packs and drawn
// ge:keywords world-models create_models place_models prepare_models encode_models
// ge:keywords bubbles ce_bubbles_share
// ge:skill render shooter morph
#include "shooters_edge/client/world_models.hpp"

#include "shooters_edge/client/burning.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "assets/image.hpp"
#include "assets/tga.hpp"
#include "brush/read.hpp"
#include "classicenhance/cached.hpp"
#include "classicenhance/models.hpp"
#include "classicenhance/replace.hpp"
#include "core/output.hpp"
#include "morph/read.hpp"
#include "render/sprite_shapes.hpp"
#include "shooters_edge/app/effects.hpp"
#include "shooters_edge/app/figure_frames.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "vfs/search.hpp"

namespace shooters_edge::client {

namespace gr = ge::render;
namespace mo = ge::morph;
namespace br = ge::brush;
namespace ba = ge::brush::app;

namespace {

// The made frames drawn for the pack's flare and bubble sprites (ce_flares, ce_bubbles): their
// texels, their size in the world, and how far a bubble wobbles off its path as it rises.
constexpr uint32_t kFlarePx = 64, kBubblePx = 32;
constexpr double kFlareM = 1.3, kBubbleM = 0.07, kBubbleWobbleM = 0.015;

// Whether the bubble the server moves as `entity` is one of the `share` (0..1) drawn: a fixed pick
// by its number, so a bubble kept stays kept all the way up and a column thins evenly.
bool bubble_kept(int entity, double share) {
  if (share >= 1) return true;
  return double(uint32_t(entity) * 2654435761u % 1000u) < share * 1000;
}

constexpr uint32_t kSpin = 8;           // a model file's flag: the item turns
constexpr double kSpinDegPerS = 100;    // how fast
constexpr float kAmbientShare = 0.75f;  // of the floor's light, from every side
constexpr float kShadeShare = 0.6f;     // and from above and ahead
constexpr float kRelightUnits = 1;      // a move that takes the light again

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

// The two frames of `k` a placed `p` is drawn between at `at` and the share of the way: its
// animation between its last two frames when one is under way, else its group flowing between its
// own frames (`Placed::lerp`), else the frame alone.
void frames_of(const ModelKind& k, const Placed& p, double at, uint32_t& a, uint32_t& b, float& f) {
  // Another figure's frame of the same name's kind (ModelKind::frame_map).
  const auto mapped = [&k](int n) {
    return n >= 0 && size_t(n) < k.frame_map.size() ? k.frame_map[size_t(n)] : n;
  };
  const int frame = mapped(p.frame);
  if (p.frame_from >= 0 && p.frame_f < 1) {
    a = models::frame_in(k.frames, mapped(p.frame_from), at);
    b = models::frame_in(k.frames, frame, at);
    f = std::clamp(p.frame_f, 0.0f, 1.0f);
    return;
  }
  if (p.lerp) {
    models::frames_between(k.frames, frame, at, a, b, f);
    return;
  }
  a = b = models::frame_in(k.frames, frame, at);
  f = 0;
}

void to_world(const sh::Vec3f& p, double out[3]) {
  const float f[3] = {p.x, p.y, p.z};
  br::to_engine(f, out);
}

// How many times the pack's skin `skin` the enhanced copy `big` is across and down; 0 when it is
// not a whole multiple of 2 or more.
uint32_t scale_of(const gr::MorphSkin& skin, const ge::assets::Image& big) {
  if (skin.width == 0 || skin.height == 0 || big.width <= 0 || big.height <= 0) return 0;
  const uint32_t w = uint32_t(big.width), h = uint32_t(big.height);
  const uint32_t k = w / skin.width;
  if (k < 2 || w != skin.width * k || h != skin.height * k || big.rgba.size() != size_t(w) * h * 4)
    return 0;
  return k;
}

// `skin` (the pack's own) redrawn from its enhanced copy `big` (`from` times its size) at `k`
// times its size, each texel the mean of the copy's under it: the colours the copy's, each
// texel's mark its source texel's.
void enhance_skin(gr::MorphSkin& skin, const ge::assets::Image& big, uint32_t from, uint32_t k) {
  const uint32_t w = skin.width * k, h = skin.height * k, step = from / k;
  if (step == 0) return;
  std::vector<uint8_t> rgba(size_t(w) * h * 4);
  for (uint32_t y = 0; y < h; y++) {
    for (uint32_t x = 0; x < w; x++) {
      uint32_t sum[4] = {0, 0, 0, 0};
      for (uint32_t j = 0; j < step; j++)
        for (uint32_t i = 0; i < step; i++) {
          const size_t at = ((size_t(y) * step + j) * size_t(big.width) + size_t(x) * step + i) * 4;
          for (int c = 0; c < 4; c++) sum[c] += big.rgba[at + size_t(c)];
        }
      for (int c = 0; c < 4; c++)
        rgba[(size_t(y) * w + x) * 4 + size_t(c)] = uint8_t(sum[c] / (step * step));
    }
  }
  std::vector<uint8_t> marks;
  if (!skin.marks.empty()) {
    marks.resize(size_t(w) * h);
    for (uint32_t y = 0; y < h; y++)
      for (uint32_t x = 0; x < w; x++)
        marks[size_t(y) * w + x] = skin.marks[size_t(y / k) * skin.width + x / k];
  }
  skin.width = w;
  skin.height = h;
  skin.rgba = std::move(rgba);
  skin.marks = std::move(marks);
}

// A skin drawn from the game's full-colour picture: its colours, a texel under half opaque a hole
// (nothing drawn), the rest plain (a picture carries no tint rows or glow).
void stand_in_skin(gr::MorphSkin& skin, const ge::assets::Image& img) {
  skin.width = uint32_t(img.width);
  skin.height = uint32_t(img.height);
  skin.rgba = img.rgba;
  skin.marks.assign(size_t(skin.width) * skin.height, gr::kMarkPlain);
  for (size_t i = 0; i < skin.marks.size(); i++)
    if (img.rgba[i * 4 + 3] < 128) skin.marks[i] = gr::kMarkHole;
}

// Says once why a file on the model list is not drawn.
void not_drawn(std::string_view kind, const std::string& name, const std::string& error) {
  std::string line = "shooters_edge: ";
  line += kind;
  line += ' ';
  line += name;
  line += " not drawn: ";
  line += error;
  ge::core::output::status(line);
}

// How much of entity `e` shows: the mod's `alpha` field above 0, else the map's `alpha` key.
float entity_alpha(const Listen& l, int e) {
  const float a = field_named(l, e, "alpha");
  if (a > 0) return a > 1 ? 1.0f : a;
  const std::vector<ge::progs::EntityExtras>& x = l.server->extras;
  return size_t(e) < x.size() && x[size_t(e)].has_alpha ? x[size_t(e)].alpha : 1.0f;
}

// Its size and tint: the mod's `scale` (0: whole) and `colormod` ('0 0 0': none), else the map's
// `colormod` key.
void entity_look(const Listen& l, int e, Placed& p) {
  const float s = field_named(l, e, "scale");
  if (s > 0) p.scale = s;
  float c[3];
  if (vector_field_named(l, e, "colormod", c) && (c[0] != 0 || c[1] != 0 || c[2] != 0)) {
    for (int k = 0; k < 3; k++) p.colormod[k] = c[k] > 0 ? c[k] : 0;
    return;
  }
  const std::vector<ge::progs::EntityExtras>& x = l.server->extras;
  if (size_t(e) < x.size() && x[size_t(e)].has_colormod)
    for (int k = 0; k < 3; k++) p.colormod[k] = x[size_t(e)].colormod[k];
}

// A 24-bit skin as the morph pass draws it: its colours as painted (no shirt or pants tinted),
// alpha under half a hole, halved while a side is over the slot's, which holds it.
gr::MorphSkin true_colour_skin(ge::assets::Image img) {
  while (img.width > int(gr::kMorphSlotSize) || img.height > int(gr::kMorphSlotSize)) {
    ge::assets::Image half;
    half.width = std::max(1, img.width / 2);
    half.height = std::max(1, img.height / 2);
    half.rgba.resize(size_t(half.width) * size_t(half.height) * 4);
    for (int y = 0; y < half.height; y++)
      for (int x = 0; x < half.width; x++)
        for (int c = 0; c < 4; c++) {
          int sum = 0;
          for (int k = 0; k < 4; k++) {
            const int sx = std::min(img.width - 1, 2 * x + (k & 1));
            const int sy = std::min(img.height - 1, 2 * y + (k >> 1));
            sum += img.rgba[(size_t(sy) * size_t(img.width) + size_t(sx)) * 4 + size_t(c)];
          }
          half.rgba[(size_t(y) * size_t(half.width) + size_t(x)) * 4 + size_t(c)] =
              uint8_t((sum + 2) / 4);
        }
    img = std::move(half);
  }
  gr::MorphSkin skin;
  stand_in_skin(skin, img);
  return skin;
}

// Lists a skin picture under `key` (its path's forms gathered: .tga, then .png, then .pcx).
void list_skin(ModelsRead& m, const std::string& key, std::string_view path, bool whole) {
  auto [at, fresh] = m.player_skins.try_emplace(key, uint32_t(m.skin_files.size()));
  if (fresh) m.skin_files.push_back({key, whole, {}});
  std::vector<std::string>& paths = m.skin_files[at->second].paths;
  paths.emplace_back(path);
  const auto rank = [](const std::string& q) {
    return q.ends_with(".tga") ? 0 : q.ends_with(".png") ? 1 : 2;
  };
  std::stable_sort(paths.begin(), paths.end(), [&rank](const std::string& x, const std::string& y) {
    return rank(x) < rank(y);
  });
}

// THE PLAYERS' SKINS: every name under `skins/` with a .tga, .png or .pcx, listed; each is read
// the first time a player wears it (`worn_slot`).
void read_player_skins(const SearchPath& files, ModelsRead& m) {
  for (const ge::vfs::Listed& l : ge::vfs::list(files.vfs)) {
    const std::string_view n = l.name;
    if (!n.starts_with("skins/") ||
        !(n.ends_with(".pcx") || n.ends_with(".png") || n.ends_with(".tga")))
      continue;
    const std::string key = models::skin_key(n.substr(6));
    if (!key.empty() && key.find('/') == std::string::npos) list_skin(m, key, n, false);
  }
}

// One figure's morph model read into `k`, its skins left a texel each: what it wears is another
// model's or a slot's (`MorphDraw::skin`, `slot`), so no picture is held twice; the first skin as
// it was goes to `own` when asked. `keep`: its own skins kept (a first-game figure's, worn in the
// player's colours when no picture of the folder's is).
void add_figure_kind(const mo::Model& model, std::span<const uint8_t> palette, ModelKind& k,
                     std::vector<gr::MorphModel>& morphs, gr::MorphSkin* own = nullptr,
                     bool keep = false) {
  k.is = ModelKind::Is::alias;
  k.first = uint32_t(morphs.size());
  k.frames = models::frame_groups(model);
  k.skins = models::skin_groups(model);
  k.flags = model.flags;
  k.synced = model.synced;
  for (size_t s = 0; s < std::max<size_t>(1, model.skins.size()); s++) {
    morphs.push_back(models::morph_from_alias(model, palette, int(s)));
    if (s == 0 && own != nullptr) *own = morphs.back().skin;
    if (!keep) morphs.back().skin = gr::MorphSkin{1, 1, {128, 128, 128, 255}, {}};
  }
}

// THE FIGURES: every folder under `players/` with a figure (`tris.md2`, else `tris.mdl`; at most
// kMaxFigures; the second game's own, or one a player of the first game names in `model`, its
// frames mapped from the game's figure's by name: app/figure_frames.hpp), the weapons the server
// lists as carried (`#<file>`) in each that has them (their shared picture once, the first one's),
// each folder kept while the atlas holds it; and the pictures in each (`<skin>.tga`, `.png` or
// `.pcx`, not an icon `_i` nor `weapon`) listed, read whole the first time a player wears one, the
// first of each folder kept for a skin it lacks.
void read_figures(const SearchPath& files, std::span<const uint8_t> palette,
                  const GameAdapter& game, ModelsRead& m) {
  std::vector<std::pair<std::string, std::string>> folders;   // folder, its figure's file
  std::vector<std::pair<std::string, std::string>> pictures;  // key, path
  for (const ge::vfs::Listed& l : ge::vfs::list(files.vfs)) {
    const std::string_view n = l.name;
    if (!n.starts_with("players/")) continue;
    const std::string_view rest = n.substr(8);
    const size_t slash = rest.find('/');
    if (slash == std::string_view::npos || slash == 0) continue;
    const std::string folder(rest.substr(0, slash));
    const std::string_view file = rest.substr(slash + 1);
    if (file.find('/') != std::string_view::npos) continue;
    if (file == "tris.md2" || file == "tris.mdl") {
      const auto same = std::find_if(folders.begin(), folders.end(),
                                     [&folder](const auto& f) { return f.first == folder; });
      if (same != folders.end()) {
        if (file == "tris.md2") same->second = file;  // the second game's figure first
      } else if (folders.size() < kMaxFigures) {
        folders.emplace_back(folder, std::string(file));
      }
      continue;
    }
    if (!(file.ends_with(".pcx") || file.ends_with(".png") || file.ends_with(".tga"))) continue;
    const std::string skin = models::skin_key(file);
    if (skin.empty() || skin.ends_with("_i") || skin == "weapon") continue;
    std::string key = folder;
    key.append("/").append(skin);
    pictures.emplace_back(std::move(key), std::string(n));
  }
  std::vector<std::string> carried;
  for (const ModelKind& k : m.kinds)
    if (!k.carried.empty()) carried.push_back(k.carried);
  std::vector<gr::AtlasRect> rects;
  for (const auto& [folder, figure_file] : folders) {
    OpenedModel opened;
    std::string error;
    std::string path = "players/";
    path.append(folder).append("/").append(figure_file);
    const bool painted = figure_file == "tris.md2";
    if (!game.open_model(game, files, path, opened, error) || opened.form != ModelForm::morph) {
      if (!error.empty()) not_drawn("model", path, error);
      continue;
    }
    const size_t morphs_before = m.morphs.size(), kinds_before = m.figure_kinds.size();
    ModelKind k;
    k.figure = painted;  // a first-game figure wears its own skins in the player's colours
    add_figure_kind(opened.morph, palette, k, m.morphs, nullptr, !painted);
    if (!m.player_frames.empty()) {
      std::vector<std::string> names;
      names.reserve(opened.morph.frames.size());
      for (const mo::Frame& fr : opened.morph.frames) names.push_back(fr.name);
      k.frame_map = models::figure_frames(m.player_frames, names);
    }
    std::vector<std::pair<std::string, uint32_t>> carried_here;
    gr::MorphSkin weapon_skin;
    const uint32_t body = uint32_t(m.figure_kinds.size());
    m.figure_kinds.push_back(std::move(k));
    for (const std::string& c : carried) {
      OpenedModel w;
      std::string at = "players/";
      at.append(folder).append("/").append(c);
      std::vector<uint8_t> store;
      if (find_file(files, at, store).empty()) continue;  // the default figure's is drawn
      if (!game.open_model(game, files, at, w, error) || w.form != ModelForm::morph) continue;
      ModelKind wk;
      wk.carried = c;
      add_figure_kind(w.morph, palette, wk, m.morphs,
                      weapon_skin.width == 0 ? &weapon_skin : nullptr);
      std::string key = folder;
      key.append("/").append(c);
      carried_here.emplace_back(std::move(key), uint32_t(m.figure_kinds.size()));
      m.figure_kinds.push_back(std::move(wk));
    }
    // A figure of the second game worn by a first-game player: the folder's weapon in its hand,
    // since the first game's figure has its weapon in its own mesh and the server names none.
    if (painted && !m.player_frames.empty()) {
      std::string at = "players/";
      at.append(folder).append("/weapon.md2");
      OpenedModel w;
      std::vector<uint8_t> store;
      if (!find_file(files, at, store).empty() && game.open_model(game, files, at, w, error) &&
          w.form == ModelForm::morph) {
        ModelKind hk;
        hk.figure = true;  // its own picture's colours
        add_figure_kind(w.morph, palette, hk, m.morphs, nullptr, true);
        std::vector<std::string> names;
        names.reserve(w.morph.frames.size());
        for (const mo::Frame& fr : w.morph.frames) names.push_back(fr.name);
        hk.frame_map = models::figure_frames(m.player_frames, names);
        m.figure_kinds[body].held = uint32_t(m.figure_kinds.size());
        m.figure_kinds.push_back(std::move(hk));
      }
    }
    uint32_t worn = gr::kOwnSkin;
    if (weapon_skin.width > 0) {
      worn = uint32_t(m.morphs.size());
      m.morphs.push_back({});
      m.morphs.back().skin = std::move(weapon_skin);
    }
    if (!gr::pack_morph_atlas(m.morphs, rects)) {  // the atlas is full: the default figure's
      m.morphs.resize(morphs_before);
      m.figure_kinds.resize(kinds_before);
      ge::core::output::status("shooters_edge: players/" + folder +
                               " is drawn as the default figure: the atlas is full");
      continue;
    }
    for (size_t i = body + 1; i < m.figure_kinds.size(); i++)
      if (!m.figure_kinds[i].carried.empty()) m.figure_kinds[i].worn = worn;
    m.figures.emplace(folder, body);
    for (auto& [key, at] : carried_here) m.figure_carried.emplace(key, at);
  }
  std::sort(pictures.begin(), pictures.end());
  for (const auto& [key, path] : pictures) {
    const std::string folder = key.substr(0, key.find('/'));
    if (!m.figures.contains(folder)) continue;
    list_skin(m, key, path, true);
    m.figure_first_skins.try_emplace(folder, m.player_skins.at(key));
  }
}

// A skin listed, read through its forms in order: false when none reads (said once).
bool read_skin(const SearchPath& files, std::span<const uint8_t> palette, const SkinFile& f,
               gr::MorphSkin* out) {
  std::vector<uint8_t> store;
  std::string error;
  for (const std::string& path : f.paths) {
    const std::span<const uint8_t> bytes = find_file(files, path, store);
    if (bytes.empty()) continue;
    error.clear();
    if (path.ends_with(".pcx")) {
      ge::assets::IndexedImage picture;
      if (!ge::assets::decode_pcx(bytes, path, &picture, &error)) continue;
      if (f.whole) {
        *out = models::skin_of_indices(picture.indices, uint32_t(picture.width),
                                       uint32_t(picture.height), palette);
        return true;
      }
      if (models::player_skin(picture, palette, out)) return true;
      error = "larger than " + std::to_string(models::kPlayerPictureWidth) + "x" +
              std::to_string(models::kPlayerPictureHeight);
      continue;
    }
    std::optional<ge::assets::Image> img = path.ends_with(".tga")
                                               ? ge::assets::decode_tga(bytes, &error)
                                               : ge::assets::decode_png(bytes);
    if (img && img->width > 0 && img->height > 0) {
      *out = true_colour_skin(std::move(*img));
      return true;
    }
    if (error.empty()) error = "not a picture this game reads";
  }
  not_drawn("skin", f.paths.empty() ? f.key : f.paths.back(), error);
  return false;
}

}  // namespace

const ModelKind& drawn_kind(const WorldModels& m, const Placed& p, Worn* worn) {
  const ModelKind& k = m.kinds[size_t(p.modelindex)];
  *worn = {};
  if (k.player && p.entity > 0 && p.entity <= sh::kMaxClients) {
    const std::string_view skin = models::forced_skin(m.skin_choice, p.side, p.skin_name);
    const models::Look figure = models::choose_figure(m.skin_choice, p.model_name, skin, m.figures,
                                                      m.player_skins, m.figure_first_skins);
    if (!figure.folder.empty()) {  // the figure they named, in its folder's picture
      if (figure.skin != gr::kOwnSkin) worn->file = figure.skin;
      return m.figure_kinds[m.figures.at(figure.folder)];
    }
    worn->file = models::choose_skin(m.skin_choice, skin, m.player_skins);
    return k;
  }
  if ((!k.figure && k.carried.empty()) || m.figures.empty()) return k;
  const models::Look look =
      models::choose_look(m.skin_choice, p.skin_name, m.figures, m.player_skins);
  if (look.folder.empty()) return k;
  if (k.figure) {
    if (look.skin == gr::kOwnSkin) return k;  // no skin of theirs listed: the server's figure
    worn->file = look.skin;
    return m.figure_kinds[m.figures.at(look.folder)];
  }
  const auto found = m.figure_carried.find(look.folder + "/" + k.carried);
  if (found == m.figure_carried.end()) return k;
  worn->model = m.figure_kinds[found->second].worn;
  return m.figure_kinds[found->second];
}

uint32_t worn_slot(ge::gpu::Device& d, WorldModels& m, uint32_t file) {
  if (file >= m.skin_files.size() || m.files == nullptr || m.morph.slot_count == 0)
    return gr::kNoSlot;
  WorldModels::Slots& s = m.slots;
  s.of_file.resize(m.skin_files.size(), gr::kNoSlot);
  s.failed.resize(m.skin_files.size(), false);
  s.file_in.resize(m.morph.slot_count, ~0u);
  s.used.resize(m.morph.slot_count, 0);
  if (s.of_file[file] != gr::kNoSlot) {
    s.used[s.of_file[file]] = s.frame;
    return s.of_file[file];
  }
  if (s.failed[file]) return gr::kNoSlot;
  gr::MorphSkin skin;
  if (!read_skin(*m.files, m.palette, m.skin_files[file], &skin)) {
    s.failed[file] = true;
    return gr::kNoSlot;
  }
  // The slot worn longest ago (one never used first); never one worn this frame.
  uint32_t slot = 0;
  for (uint32_t i = 1; i < s.used.size(); i++)
    if (s.used[i] < s.used[slot]) slot = i;
  if (s.used[slot] == s.frame && s.frame != 0) return gr::kNoSlot;
  if (s.file_in[slot] != ~0u) s.of_file[s.file_in[slot]] = gr::kNoSlot;
  gr::set_morph_slot(d, m.morph, slot, skin);
  s.file_in[slot] = file;
  s.of_file[file] = slot;
  s.used[slot] = s.frame;
  return slot;
}

void place_models(const Listen& l, std::vector<Placed>& out) {
  out.clear();
  if (!l.server) return;
  const sv::RulesCommon& s = *l.server;
  const int me = player_state(l).entity;
  const ge::console::Cvar* tp = s.cvars.find("teamplay");
  const bool teamplay = tp != nullptr && tp->on();
  const std::string mine = me > 0 && me <= sh::kMaxClients
                               ? sv::info_value(s.clients[size_t(me - 1)].userinfo, "team")
                               : std::string{};
  const int models = int(s.models.size());
  const bool qc = l.rules != nullptr && sv::qc_of(*l.rules) != nullptr;
  for (int e = 1; e < s.sv.num_edicts; e++) {
    const sh::Edict& ed = s.sv.edicts[size_t(e)];
    if (ed.free || ed.modelindex <= 0 || ed.modelindex >= models || e == me) continue;
    if (!model_shown(l, e)) continue;  // a taken item: its model text emptied
    Placed p;
    p.entity = e;
    p.modelindex = ed.modelindex;
    if (qc) {
      p.frame = int(field_named(l, e, "frame"));
      p.skin = int(field_named(l, e, "skin"));
      p.effects = int(field_named(l, e, "effects"));
    } else if (size_t(e) < s.world.entities.size()) {
      // Rules of our own (no program): their world rows carry the frame, the skin and the turn.
      // Their effects are their own game's bits, not these, so none are read.
      const sv::WorldEntity& row = s.world.entities[size_t(e)];
      p.frame = row.frame;
      p.skin = row.skin;
      p.turns = row.turns;
      if (row.carried_model > 0 && row.carried_model < models) p.carried = row.carried_model;
    }
    p.alpha = entity_alpha(l, e);
    entity_look(l, e, p);
    p.origin = ed.origin;
    p.angles = ed.angles;
    if (e <= sh::kMaxClients) {
      const sv::ClientInfo& c = s.clients[size_t(e - 1)];
      p.top = colour_row(c, "topcolor", 0);
      p.bottom = colour_row(c, "bottomcolor", 0);
      p.skin_name = sv::info_value(c.userinfo, "skin");
      p.model_name = sv::info_value(c.userinfo, "model");
      p.side = models::side_of(teamplay, mine, sv::info_value(c.userinfo, "team"));
    }
    out.push_back(p);
    if (p.carried > 0) {  // the weapon it carries: the same place and frame, its own model
      Placed w = p;
      w.entity = e + kCarriedEntity;
      w.modelindex = p.carried;
      w.skin = 0;
      w.carried = 0;
      out.push_back(w);
    }
  }
  for (size_t i = 0; i < s.statics.size(); i++) {
    const sv::StaticEntity& st = s.statics[i];
    if (st.modelindex <= 0 || st.modelindex >= models) continue;
    Placed p;
    p.entity = -1 - int(i);
    p.modelindex = st.modelindex;
    p.frame = int(st.frame);
    p.skin = int(st.skin);
    p.alpha = st.extras.has_alpha ? st.extras.alpha : 1.0f;
    if (st.extras.has_colormod)
      for (int c = 0; c < 3; c++) p.colormod[c] = st.extras.colormod[c];
    p.origin = st.origin;
    p.angles = st.angles;
    out.push_back(p);
  }
}

ModelsRead read_models(const SearchPath& files, std::span<const uint8_t> palette,
                       const br::Map& map, const ModelLoad& load, const GameAdapter& game) {
  ModelsRead m;
  std::vector<gr::MorphModel>& morphs = m.morphs;
  // The skins with enhanced copies: the morph model, its pack's own skin, the copy and its scale.
  struct Enhanced {
    size_t at;
    gr::MorphSkin own;
    ge::assets::Image big;
    uint32_t from;
  };
  std::vector<Enhanced> enhanced;
  // The skins drawn from the game's pictures: the morph model and its pack's own skin.
  std::vector<std::pair<size_t, gr::MorphSkin>> stood_in;
  uint32_t most = 0;
  std::vector<gr::SpriteFrame>& frames = m.frames;
  std::set<std::string> refused;
  // The server's list, then the effects' own models (kEffectModels).
  std::vector<std::string> names(load.names.begin(), load.names.end());
  for (const char* e : kEffectModels) names.emplace_back(e);
  m.kinds.assign(names.size(), {});
  m.brush_of.assign(names.size(), 0);
  m.brushes = uint32_t(map.models.size());
  for (size_t i = 0; i < names.size(); i++) {
    const std::string& name = names[i];
    const bool own = i >= load.names.size();  // an effect's: no word when the pack lacks it
    ModelKind& k = m.kinds[i];
    // [1] is the map itself and "*N" its own brush models: the map's to draw, not these.
    if (name.size() > 1 && name[0] == '*') {
      const int n = std::atoi(name.c_str() + 1);
      if (n > 0 && uint32_t(n) < m.brushes) m.brush_of[i] = uint32_t(n);
      continue;
    }
    if (i == 1 || name.empty()) continue;
    const ge::classicenhance::Replacement* rep = ge::classicenhance::replacement_of(name);
    if (rep != nullptr) k.effect = rep->effect;
    OpenedModel opened;
    std::string error;
    if (!game.open_model(game, files, name, opened, error)) {
      // Not in the files (the map itself is not in the packs by this name either) or refused.
      if (!own && !error.empty() && refused.insert(name).second) not_drawn("model", name, error);
      continue;
    }
    if (opened.form == ModelForm::morph) {
      const mo::Model& model = opened.morph;
      k.is = ModelKind::Is::alias;
      k.player = name == kPlayerModel;
      if (k.player)
        for (const mo::Frame& fr : model.frames) m.player_frames.push_back(fr.name);
      k.figure = name.starts_with("players/") && name.ends_with("/tris.md2");
      if (name.size() > 1 && name[0] == '#') k.carried = name.substr(1);
      k.first = uint32_t(morphs.size());
      k.frames = models::frame_groups(model);
      k.skins = models::skin_groups(model);
      k.spin = (model.flags & kSpin) != 0;
      k.flags = model.flags;
      k.synced = model.synced;
      bool any_stand_in = false;
      for (size_t s = 0; s < std::max<size_t>(1, model.skins.size()); s++) {
        morphs.push_back(models::morph_from_alias(model, palette, int(s)));
        if (s == 0) k.glow_share = models::glow_colour(morphs.back().skin, k.glow_rgb);
        if (s < opened.skins.size() && opened.skins[s].width > 0) {
          stood_in.emplace_back(morphs.size() - 1, morphs.back().skin);
          stand_in_skin(morphs.back().skin, opened.skins[s]);
          any_stand_in = true;
        }
      }
      if (!load.cache.empty() && !model.skins.empty() && !any_stand_in) {
        std::vector<ge::assets::Image> big;
        const std::string stem = std::filesystem::path(name).stem().string();
        if (ge::classicenhance::cached_skins(load.cache, stem, uint32_t(model.skins[0].width),
                                             uint32_t(model.skins[0].height), &big) &&
            big.size() == model.skins.size()) {
          for (size_t s = 0; s < big.size(); s++) {
            const uint32_t from = scale_of(morphs[k.first + s].skin, big[s]);
            if (from == 0) continue;
            most = std::max(most, from);
            enhanced.push_back({k.first + s, morphs[k.first + s].skin, std::move(big[s]), from});
          }
        }
      }
      if (rep != nullptr &&
          !rep->keep_glowing) {  // the engine's fire burns in its painted flame's place
        const mo::Model bare = ge::classicenhance::drop_glowing(model);
        k.bare = uint32_t(morphs.size());
        for (size_t s = 0; s < std::max<size_t>(1, bare.skins.size()); s++)
          morphs.push_back(models::morph_from_alias(bare, palette, int(s)));
      }
    } else if (opened.form == ModelForm::sprite) {
      const mo::Sprite& sprite = opened.sprite;
      k.is = ModelKind::Is::sprite;
      k.first = uint32_t(frames.size());
      k.frames = models::sprite_groups(sprite);
      k.facing = models::facing_of(sprite.facing);
      for (gr::SpriteFrame& f : models::sprite_frames_of(sprite, palette))
        frames.push_back(std::move(f));
      // A flare's or a bubble's made frame, drawn in its place when its switch is on.
      if (k.effect == "flare" || k.effect == "bubbles") {
        k.made = uint32_t(frames.size());
        frames.push_back(k.effect == "flare" ? gr::flare_frame(kFlarePx)
                                             : gr::bubble_frame(kBubblePx));
      }
    } else if (opened.form == ModelForm::brush) {
      gr::MorphModel mm = models::morph_from_brush(opened.brush, palette);
      if (mm.indices.empty()) continue;
      k.is = ModelKind::Is::brush;
      k.first = uint32_t(morphs.size());
      morphs.push_back(std::move(mm));
    }
  }
  if (!stood_in.empty()) {
    // The game's pictures are drawn only when the atlas holds them all; else the packs' own.
    std::vector<gr::AtlasRect> rects;
    if (!gr::pack_morph_atlas(morphs, rects)) {
      for (auto& [at, own] : stood_in) morphs[at].skin = std::move(own);
      ge::core::output::status("shooters_edge: the models' skin pictures do not fit the " +
                               std::to_string(gr::kMorphAtlasSize) +
                               "-texel atlas together; the packs' own skins are drawn (next: "
                               "smaller pictures)");
    }
  }
  if (!enhanced.empty()) {
    // The atlas is one square: every skin raised to twice the pack's first, then each again
    // toward the cache's scale, a step kept only while the atlas still holds them all; the rest
    // stay where they were (the pack's own at worst).
    std::vector<gr::AtlasRect> rects;
    std::vector<uint32_t> at_scale(enhanced.size(), 1);
    for (uint32_t k = 2; k <= most; k *= 2) {
      for (size_t i = 0; i < enhanced.size(); i++) {
        const Enhanced& e = enhanced[i];
        if (at_scale[i] != k / 2 || e.from % k != 0) continue;
        const gr::MorphSkin was = morphs[e.at].skin;
        morphs[e.at].skin = e.own;
        enhance_skin(morphs[e.at].skin, e.big, e.from, k);
        if (gr::pack_morph_atlas(morphs, rects))
          at_scale[i] = k;
        else
          morphs[e.at].skin = was;
      }
    }
    size_t kept = 0;
    for (const uint32_t k : at_scale) kept += k > 1;
    ge::core::output::status("shooters_edge: " + std::to_string(kept) + " of " +
                             std::to_string(enhanced.size()) + " skins enhanced from " +
                             load.cache.string());
  }
  bool any_player = false;
  for (const ModelKind& k : m.kinds) any_player = any_player || k.player;
  if (any_player) read_player_skins(files, m);
  bool any_figure = false;
  for (const ModelKind& k : m.kinds) any_figure = any_figure || k.figure;
  if (any_figure || any_player) read_figures(files, palette, game, m);
  m.ramps = models::palette_ramps(palette);
  if (!map.models.empty()) m.probe = models::make_probe(map);
  if (!m.skin_files.empty()) {
    m.files = &files;
    m.palette.assign(palette.begin(), palette.end());
  }
  return m;
}

void create_models(ge::gpu::Device& d, ge::gpu::PipelineSet& pipelines, WorldModels& m,
                   const wgpu::BindGroupLayout& frame, ModelsRead read) {
  m.kinds = std::move(read.kinds);
  m.brush_of = std::move(read.brush_of);
  m.brushes = read.brushes;
  m.probe = std::move(read.probe);
  m.player_skins = std::move(read.player_skins);
  m.figures = std::move(read.figures);
  m.figure_carried = std::move(read.figure_carried);
  m.figure_first_skins = std::move(read.figure_first_skins);
  m.figure_kinds = std::move(read.figure_kinds);
  m.skin_files = std::move(read.skin_files);
  m.files = read.files;
  m.palette = std::move(read.palette);
  m.slots = {};
  const std::vector<gr::MorphModel>& morphs = read.morphs;
  const std::vector<gr::SpriteFrame>& frames = read.frames;
  if (!morphs.empty()) {
    m.has_morph = gr::create_morph(d, pipelines, m.morph, frame, morphs, read.ramps,
                                   m.skin_files.empty() ? 0 : uint32_t(kSkinSlots));
    if (!m.has_morph)
      ge::core::output::status(
          "shooters_edge: the models' skins do not fit the atlas; models not drawn");
  }
  // The frames and triangles kept for the ash; the skins and coordinates are the GPU's alone.
  m.shapes.clear();
  m.shapes.reserve(morphs.size());
  for (const gr::MorphModel& mm : morphs) {
    gr::MorphModel shape;
    shape.positions = mm.positions;
    shape.indices = mm.indices;
    m.shapes.push_back(std::move(shape));
  }
  if (!frames.empty()) {
    m.has_sprites = gr::create_sprites(d, pipelines, m.sprites, frame, frames);
    if (!m.has_sprites)
      ge::core::output::status("shooters_edge: the sprites do not fit the atlas; not drawn");
  }
}

void place_brushes(const WorldModels& m, std::span<const Placed> placed,
                   std::vector<ba::Mover>& out) {
  out.clear();
  if (m.kinds.empty() || m.brushes < 2) return;
  out.resize(m.brushes - 1);
  for (uint32_t k = 1; k < m.brushes; k++) {
    out[k - 1].model = k;
    out[k - 1].hidden = true;
  }
  for (const Placed& p : placed) {
    if (size_t(p.modelindex) >= m.brush_of.size()) continue;
    const uint32_t n = m.brush_of[size_t(p.modelindex)];
    if (n == 0) continue;
    ba::Mover& mv = out[n - 1];
    mv.hidden = false;
    to_world(p.origin, mv.offset);
  }
}

bool body_triangles(const WorldModels& m, const Placed& p, double time_s, std::vector<float>& out) {
  out.clear();
  if (p.modelindex <= 0 || size_t(p.modelindex) >= m.kinds.size()) return false;
  Worn worn_skin;
  const ModelKind& k = drawn_kind(m, p, &worn_skin);
  if (k.is != ModelKind::Is::alias) return false;
  // The model and frame as prepare_models picks them.
  const double at =
      k.synced ? time_s : time_s + double(uint32_t(p.entity) * 2654435761u % 1000) * 1e-3;
  const uint32_t model = k.first + models::frame_in(k.skins, p.skin, at);
  if (model >= m.shapes.size()) return false;
  const gr::MorphModel& shape = m.shapes[model];
  if (shape.positions.empty()) return false;
  uint32_t a = 0, b = 0;
  float f = 0;
  frames_of(k, p, at, a, b, f);
  const auto last = uint32_t(shape.positions.size() - 1);
  const std::vector<float>& pa = shape.positions[std::min(a, last)];
  const std::vector<float>& pb = shape.positions[std::min(b, last)];
  out.reserve(shape.indices.size() * 3);
  for (const uint32_t i : shape.indices)
    for (size_t c = 0; c < 3; c++) {
      const float va = pa[size_t(i) * 3 + c];
      out.push_back(f > 0 ? va + (pb[size_t(i) * 3 + c] - va) * f : va);
    }
  return !out.empty();
}

bool held_tip(const WorldModels& m, const Placed& p, double time_s, const double forward[3],
              double tip[3]) {
  thread_local std::vector<float> body;
  if (!body_triangles(m, p, time_s, body)) return false;
  double w[3];
  to_world(p.origin, w);
  const float ang[3] = {p.angles.x, p.angles.y, p.angles.z};
  float q[4];
  models::rotation_of(ang, true, q);
  const double x = q[0], y = q[1], z = q[2], s = q[3];
  double best = -1e30;
  for (size_t i = 0; i + 2 < body.size(); i += 3) {
    const double v[3] = {body[i], body[i + 1], body[i + 2]};
    const double t[3] = {2 * (y * v[2] - z * v[1]), 2 * (z * v[0] - x * v[2]),
                         2 * (x * v[1] - y * v[0])};
    const double r[3] = {v[0] + s * t[0] + (y * t[2] - z * t[1]),
                         v[1] + s * t[1] + (z * t[0] - x * t[2]),
                         v[2] + s * t[2] + (x * t[1] - y * t[0])};
    const double d = r[0] * forward[0] + r[1] * forward[1] + r[2] * forward[2];
    if (d <= best) continue;
    best = d;
    for (int c = 0; c < 3; ++c) tip[c] = w[c] + r[c];
  }
  return best > -1e30;
}

void shadow_casters(const WorldModels& m, std::span<const Placed> placed, const Placed* self,
                    const gr::FrameView& view, std::vector<float>& out,
                    std::vector<ge::brush::app::CasterBox>& boxes) {
  out.clear();
  boxes.clear();
  std::vector<float> body;
  const auto cast = [&](const Placed& p) {
    if (size_t(p.modelindex) >= m.kinds.size()) return;
    Worn worn_skin;
    const ModelKind& k = drawn_kind(m, p, &worn_skin);
    if (k.is != ModelKind::Is::alias || k.effect.starts_with("fire")) return;
    if (p.warp.shown < 1) return;  // half ash: its shadow would be whole
    if (p.alpha < 0.5f) return;    // mostly see-through: a whole shadow would give it away
    double w[3];
    to_world(p.origin, w);
    const double dx = w[0] - view.eye[0], dy = w[1] - view.eye[1], dz = w[2] - view.eye[2];
    if (dx * dx + dy * dy + dz * dz > kCasterReachM * kCasterReachM) return;
    if (!body_triangles(m, p, view.time_s, body)) return;
    const float ang[3] = {p.angles.x, p.angles.y, p.angles.z};
    float q[4];
    models::rotation_of(ang, true, q);
    const double x = q[0], y = q[1], z = q[2], s = q[3];
    const size_t first = out.size();
    for (size_t i = 0; i + 2 < body.size(); i += 3) {
      const double v[3] = {body[i], body[i + 1], body[i + 2]};
      const double t[3] = {2 * (y * v[2] - z * v[1]), 2 * (z * v[0] - x * v[2]),
                           2 * (x * v[1] - y * v[0])};
      out.push_back(float(w[0] + v[0] + s * t[0] + (y * t[2] - z * t[1])));
      out.push_back(float(w[1] + v[1] + s * t[1] + (z * t[0] - x * t[2])));
      out.push_back(float(w[2] + v[2] + s * t[2] + (x * t[1] - y * t[0])));
    }
    ge::brush::app::CasterBox box{{1e30f, 1e30f, 1e30f}, {-1e30f, -1e30f, -1e30f}};
    for (size_t i = first; i < out.size(); i += 3)
      for (int a = 0; a < 3; ++a) {
        box.lo[a] = std::min(box.lo[a], out[i + size_t(a)]);
        box.hi[a] = std::max(box.hi[a], out[i + size_t(a)]);
      }
    if (out.size() > first) boxes.push_back(box);
  };
  for (const Placed& p : placed) cast(p);
  if (self != nullptr) cast(*self);
}

void prepare_models(ge::gpu::Device& d, WorldModels& m, const br::Map& map,
                    std::span<const Placed> placed, const gr::FrameView& view, double brightness,
                    double minlight, bool smooth, const InPlace& in_place,
                    std::span<const ge::brush::app::DynamicLight> dynamic, double dynamic_strength,
                    const Burning* burning) {
  const bool bare_flames = in_place.flames;
  m.morph_draws.clear();
  m.sprite_draws.clear();
  m.near.clear();
  m.slots.frame++;
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
    Worn worn;
    const ModelKind& k = drawn_kind(m, p, &worn);
    double w[3];
    to_world(p.origin, w);
    // Unsynced copies each start their loop at their own moment.
    const double at = k.synced ? t : t + double(uint32_t(p.entity) * 2654435761u % 1000) * 1e-3;
    if (k.is == ModelKind::Is::sprite) {
      if (!m.has_sprites || m.sprite_draws.size() >= gr::kMaxSprites) continue;
      // Only a share of the bubbles the game blows is drawn, in either look (ce_bubbles_share).
      if (k.effect == "bubbles" && !bubble_kept(p.entity, in_place.bubbles_share)) continue;
      if (k.effect == "explosion" && in_place.explosions) continue;
      const bool flare = k.made != ModelKind::kNone && k.effect == "flare" && in_place.flares;
      const bool bubble = k.made != ModelKind::kNone && k.effect == "bubbles" && in_place.bubbles;
      if (flare || bubble) {
        gr::SpriteDraw s;
        s.frame = k.made;
        s.facing = gr::SpriteFacing::parallel;
        s.outdoor = 0;
        s.smooth = true;
        if (flare) {
          s.blend = gr::SpriteBlend::add;
          s.scale = float(kFlareM / kFlarePx);
          s.glow = 2.5f;  // a lamp outshines the wall it hangs on
          s.colour[0] = 1.0f, s.colour[1] = 0.74f, s.colour[2] = 0.42f;  // the globe's warm flame
          for (int a = 0; a < 3; a++) s.position[a] = s.last[a] = w[a];
          m.sprite_draws.push_back(s);
          continue;
        }
        s.blend = gr::SpriteBlend::over;
        s.glow = 0.7f;  // its rim catches the room's light
        // Each its own phase, so a column of bubbles never wobbles as one.
        const double ph = double(uint32_t(p.entity) * 2654435761u % 6283) * 1e-3;
        double c[3] = {w[0], w[1], w[2]};
        c[0] += kBubbleWobbleM * std::sin(t * 2 * 3.14159265358979 * 2.7 + ph);
        c[2] += kBubbleWobbleM * std::cos(t * 2 * 3.14159265358979 * 2.3 + ph * 1.3);
        s.scale = float(kBubbleM / kBubblePx);
        for (int a = 0; a < 3; a++) s.position[a] = s.last[a] = c[a];
        m.sprite_draws.push_back(s);
        continue;
      }
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
    g.model = bare_flames && k.bare != ModelKind::kNone ? k.bare : k.first;
    uint32_t frame_a = 0, frame_b = 0;
    float blend = 0;
    if (k.is == ModelKind::Is::alias) {
      g.model += models::frame_in(k.skins, p.skin, at);
      frames_of(k, p, at, frame_a, frame_b, blend);
    }
    for (int c = 0; c < 3; c++) g.now.position[c] = w[c];
    float ang[3] = {p.angles.x, p.angles.y, p.angles.z};
    if (k.spin || p.turns) ang[1] = float(std::fmod(t * kSpinDegPerS, 360.0));
    if (p.turns) ang[0] = ang[2] = 0;  // yaw only
    models::rotation_of(ang, k.is == ModelKind::Is::alias, g.now.rotation);
    g.now.frame_a = frame_a;
    g.now.frame_b = frame_b;
    g.now.blend = blend;
    g.now.scale = p.scale;
    const auto last = m.last_pose.find(p.entity);
    g.last = last == m.last_pose.end() || p.warp.shown < 1 ? g.now : last->second;
    g.warp = p.warp;
    g.alpha = p.alpha;
    if (burning != nullptr) g.burn = burn_skin_of(*burning, p.entity);
    m.last_pose[p.entity] = g.now;
    g.tint[0] = p.top;
    g.tint[1] = p.bottom;
    models::forced_colours(m.skin_choice, p.side, &g.tint[0], &g.tint[1]);
    g.skin = worn.model;
    if (worn.file != Worn::kNoFile) g.slot = worn_slot(d, m, worn.file);
    if (k.figure || !k.carried.empty()) {  // the second game's skins wear their own colours
      g.tint[0] = models::kShirtRow;
      g.tint[1] = models::kPantsRow;
    }
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
    // The frame's dynamic lights at its middle (a figure's chest, an item's top), added as they
    // are to the floor under it.
    double dyn[3], toward[3];
    const double mid[3] = {w[0], w[1] + 16 * br::kUnitM, w[2]};
    effects::dynamic_light_at(dynamic, mid, dynamic_strength, dyn, toward, p.entity);
    double floor_lum = 0, dyn_lum = 0;
    for (int c = 0; c < 3; c++) {
      // The world's own soft floor (bsp_draw's r_minlight), so a thing in a dark corner reads
      // as the walls round it do.
      const double raw = lit.rgb[c];
      const double floor = std::sqrt(raw * raw + minlight * minlight);
      const float l = float((floor + dyn[c]) * brightness);
      g.ambient[c] = kAmbientShare * l * p.colormod[c];  // the tint on its light, not its glow
      g.shade[c] = kShadeShare * l * p.colormod[c];
      floor_lum += floor;
      dyn_lum += dyn[c];
    }
    // From above and a little toward the eye, so a face turned to the player is never black;
    // turned toward the dynamic lights by their share of the light.
    const double len = std::sqrt(dist) + 1e-9;
    double sd[3] = {-0.3 * (w[0] - view.eye[0]) / len, 0.9, -0.3 * (w[2] - view.eye[2]) / len};
    const double sd_len = std::sqrt(sd[0] * sd[0] + sd[1] * sd[1] + sd[2] * sd[2]);
    const double share = dyn_lum / (dyn_lum + floor_lum + 1e-9);
    for (int c = 0; c < 3; c++) sd[c] = (1 - share) * sd[c] / sd_len + share * toward[c];
    const double sl = std::sqrt(sd[0] * sd[0] + sd[1] * sd[1] + sd[2] * sd[2]) + 1e-9;
    for (int c = 0; c < 3; c++) g.shade_dir[c] = float(sd[c] / sl);
    g.outdoor = 0;
    g.glow = 1;
    g.smooth = smooth;
    g.light_in_white = true;
    m.morph_draws.push_back(g);
    // The weapon in a copied-in figure's hand: the same place, light and frame, its own picture.
    if (k.held != ModelKind::kNone && k.held < m.figure_kinds.size() &&
        m.morph_draws.size() < gr::kMaxMorphDraws) {
      const ModelKind& h = m.figure_kinds[k.held];
      gr::MorphDraw hw = g;
      hw.model = h.first;
      frames_of(h, p, at, hw.now.frame_a, hw.now.frame_b, hw.now.blend);
      hw.skin = gr::kOwnSkin;
      hw.slot = gr::kNoSlot;
      const int held_entity = p.entity + kCarriedEntity;
      const auto held_last = m.last_pose.find(held_entity);
      hw.last = held_last == m.last_pose.end() || p.warp.shown < 1 ? hw.now : held_last->second;
      m.last_pose[held_entity] = hw.now;
      m.morph_draws.push_back(hw);
    }
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
