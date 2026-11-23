// What a Quake II client fetches on joining, stage by stage (q2_fetch.hpp).
//
// ge:summary Quake II's files fetched on joining, stage by stage: the lists from a level's names
// ge:keywords download-order fetch-stages q2_fetch_path_ok q2_fetch_look
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_net_test.cpp
#include "shooters_edge/net/q2_fetch.hpp"

#include <algorithm>
#include <array>
#include <cctype>

#include "brush/read.hpp"
#include "morph/read.hpp"

namespace shooters_edge::net {

namespace {

bool name_char(char ch) {
  const auto u = static_cast<unsigned char>(ch);
  return std::isalnum(u) != 0 || ch == '_' || ch == '-';
}

bool plain_name(std::string_view s) {
  return !s.empty() && std::all_of(s.begin(), s.end(), name_char);
}

bool ends_with(std::string_view s, std::string_view tail) {
  return s.size() >= tail.size() && s.substr(s.size() - tail.size()) == tail;
}

// The level's names in a list, from slot 1 up to the first empty one (slot 255 of the models,
// the player's own model, stepped over when empty).
template <typename Fn>
void walk(const std::vector<std::string>& list, bool models, Fn&& fn) {
  for (size_t i = 1; i < list.size(); i++) {
    if (list[i].empty()) {
      if (models && i == 255) continue;
      break;
    }
    fn(list[i]);
  }
}

struct Lister {
  std::vector<std::string> out;
  void add(std::string_view path) {
    std::string p = q2_fetch_path(path);
    if (!q2_fetch_path_ok(p)) return;
    if (std::find(out.begin(), out.end(), p) == out.end()) out.push_back(std::move(p));
  }
};

bool plain_model(const std::string& name) { return name[0] != '*' && name[0] != '#'; }

}  // namespace

std::string q2_fetch_path(std::string_view path) {
  std::string p(path);
  std::replace(p.begin(), p.end(), '\\', '/');
  for (size_t at; (at = p.find("/./")) != std::string::npos;) p.erase(at, 2);
  while (p.rfind("./", 0) == 0) p.erase(0, 2);
  for (char& ch : p) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return p;
}

bool q2_fetch_path_ok(std::string_view path) {
  if (path.empty() || path.size() >= 64) return false;
  if (!name_char(path.front()) || !name_char(path.back())) return false;
  if (path.find("..") != std::string_view::npos || path.find('/') == std::string_view::npos)
    return false;
  for (const char ch : path)
    if (static_cast<unsigned char>(ch) < 32 || static_cast<unsigned char>(ch) > 126) return false;
  for (const std::string_view skip : {"save/", "music/", "video/"})
    if (path.substr(0, skip.size()) == skip) return false;
  const size_t dot = path.rfind('.');
  if (dot == std::string_view::npos || dot < path.rfind('/')) return false;
  static constexpr std::array<std::string_view, 15> kKinds = {"bsp", "dm2", "ent", "jpg", "loc",
                                                              "md2", "md3", "ogg", "pcx", "png",
                                                              "sp2", "tga", "txt", "wal", "wav"};
  std::string ext(path.substr(dot + 1));
  for (char& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return std::find(kKinds.begin(), kKinds.end(), ext) != kKinds.end();
}

std::string q2_fetch_look(std::string_view look) {
  const size_t slash = look.find_first_of("/\\");
  if (slash == std::string_view::npos) return "male/grunt";
  std::string figure(look.substr(0, slash));
  const std::string skin(look.substr(slash + 1));
  if (figure.empty()) figure = "male";
  if (!plain_name(skin)) return figure == "female" ? "female/athena" : "male/grunt";
  if (!plain_name(figure)) return "male/grunt";
  return figure + "/" + skin;
}

std::vector<std::string> q2_fetch_models(const client::DemoState& s) {
  Lister l;
  if (s.models.size() > 1 && !s.models[1].empty()) l.add(s.models[1]);
  walk(s.models, true, [&l](const std::string& m) {
    if (plain_model(m) && m.rfind("maps/", 0) != 0) l.add(m);
  });
  return std::move(l.out);
}

std::vector<std::string> q2_fetch_level(const client::DemoState& s, const Q2FetchRead& read,
                                        bool vweps) {
  Lister l;
  // The skins and frames the models name, read from the files now held.
  walk(s.models, true, [&](const std::string& m) {
    if (!plain_model(m) || !read) return;
    const std::string path = q2_fetch_path(m);
    if (!ends_with(path, ".md2") && !ends_with(path, ".sp2")) return;
    const std::vector<uint8_t> bytes = read(path);
    if (bytes.empty()) return;
    std::string error;
    if (ends_with(path, ".md2")) {
      ge::morph::Model model;
      if (ge::morph::read_model(bytes, path, &model, &error))
        for (const std::string& skin : model.skin_paths) l.add(skin);
    } else {
      ge::morph::Sprite sprite;
      if (ge::morph::read_sprite(bytes, path, &sprite, &error))
        for (const ge::morph::SpriteFrame& f : sprite.frames) l.add(f.picture);
    }
  });
  walk(s.sounds, false, [&l](const std::string& n) {
    if (n[0] == '*') return;
    l.add(n[0] == '#' ? n.substr(1) : "sound/" + n);
  });
  walk(s.images, false, [&l](const std::string& n) {
    l.add(n[0] == '/' || n[0] == '\\' ? n.substr(1) : "pics/" + n + ".pcx");
  });
  std::vector<std::string> weapons = {"weapon.md2"};
  if (vweps)
    walk(s.models, true, [&weapons](const std::string& m) {
      if (m[0] == '#') weapons.push_back(m.substr(1));
    });
  for (const std::string& raw : s.looks) {
    if (raw.empty()) continue;
    const std::string look = q2_fetch_look(raw);
    const size_t slash = look.find('/');
    const std::string dir = "players/" + look.substr(0, slash) + "/";
    const std::string skin = look.substr(slash + 1);
    l.add(dir + "tris.md2");
    for (const std::string& w : weapons) l.add(dir + w);
    l.add(dir + "weapon.pcx");
    l.add(dir + skin + ".pcx");
    l.add(dir + skin + "_i.pcx");
    walk(s.sounds, false, [&](const std::string& n) {
      if (n[0] == '*') l.add(dir + n.substr(1));
    });
  }
  for (const char* side : {"rt", "lf", "bk", "ft", "up", "dn"})
    l.add("env/" + s.sky + side + ".tga");
  return std::move(l.out);
}

std::vector<std::string> q2_fetch_textures(std::span<const uint8_t> map) {
  ge::brush::Map m;
  std::string error;
  if (map.empty() || !ge::brush::read_bsp(map, &m, &error) || m.family != ge::brush::Family::v38)
    return {};
  Lister l;
  for (const ge::brush::Texture& t : m.textures)
    if (!t.name.empty()) l.add("textures/" + t.name + ".wal");
  return std::move(l.out);
}

}  // namespace shooters_edge::net
