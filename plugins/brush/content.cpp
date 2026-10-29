// The content scan and the asset map (content.hpp).
//
// ge:summary The content scan: shipped files, the maps' entity keys, numbered sets; the asset map
// ge:keywords content scan noise model sounds worldtype assetmap parse compare pin
// ge:skill brush free-assets
// ge:test tests/brush_content_test.cpp
#include "brush/content.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "vfs/pak.hpp"

namespace ge::brush {

namespace {

bool ends_with(std::string_view s, std::string_view tail) {
  return s.size() >= tail.size() && s.substr(s.size() - tail.size()) == tail;
}

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  }
  return out;
}

bool is_model_file(std::string_view path) {
  const std::string p = lower(path);
  return ends_with(p, ".mdl") || ends_with(p, ".md2") || ends_with(p, ".md3") ||
         ends_with(p, ".spr") || ends_with(p, ".sp2");
}

bool is_sound_key(std::string_view key) {
  return key == "noise" || key == "noise1" || key == "noise2" || key == "noise3" || key == "noise4";
}

bool is_number(std::string_view s) {
  if (s.empty()) return false;
  for (char c : s) {
    if (c < '0' || c > '9') return false;
  }
  return true;
}

// Numbers by value ("2" before "10"), anything else as text after them.
bool value_less(const std::string& a, const std::string& b) {
  const bool na = is_number(a), nb = is_number(b);
  if (na && nb) return a.size() != b.size() ? a.size() < b.size() : a < b;
  if (na != nb) return na;
  return a < b;
}

}  // namespace

ContentScan scan_content(const std::vector<ContentFile>& files) {
  std::vector<const ContentFile*> sorted;
  sorted.reserve(files.size());
  for (const ContentFile& f : files) sorted.push_back(&f);
  std::stable_sort(sorted.begin(), sorted.end(),
                   [](const ContentFile* a, const ContentFile* b) { return a->path < b->path; });

  // Assets by kind and name; uses appended in map order.
  std::map<std::pair<AssetKind, std::string>, ScannedAsset> assets;
  auto asset = [&](AssetKind kind, const std::string& name) -> ScannedAsset& {
    ScannedAsset& a = assets[{kind, name}];
    a.kind = kind;
    a.name = name;
    return a;
  };
  std::map<std::tuple<std::string, std::string, std::string>, SetUse> sets;

  ContentScan scan;
  std::set<std::string> stored;
  for (const ContentFile* f : sorted) {
    if (!stored.insert(f->path).second) continue;  // the first file stored under a path wins
    const std::string p = lower(f->path);
    if (ends_with(p, ".wav")) {
      const std::string_view name = std::string_view(f->path).starts_with("sound/")
                                        ? std::string_view(f->path).substr(6)
                                        : std::string_view(f->path);
      asset(AssetKind::sound, std::string(name)).shipped = true;
      continue;
    }
    if (is_model_file(f->path)) {
      asset(AssetKind::model, f->path).shipped = true;
      continue;
    }
    if (!ends_with(p, ".bsp")) continue;

    Map map;
    std::string error;
    if (!read_bsp(f->bytes, &map, &error)) {
      scan.unread.emplace_back(f->path, error);
      continue;
    }
    scan.maps.push_back(f->path);
    for (const Entity& e : map.entities) {
      const std::string& classname = e.get("classname");
      for (const auto& [key, value] : e.pairs) {
        if (value.empty()) continue;
        if (is_sound_key(key)) {
          asset(AssetKind::sound, value).uses.push_back({f->path, classname, key});
        } else if (key == "model" && value[0] != '*') {
          asset(AssetKind::model, value).uses.push_back({f->path, classname, key});
        } else if ((key == "sounds" && classname != "worldspawn") ||
                   (key == "worldtype" && classname == "worldspawn")) {
          SetUse& s = sets[{classname, key, value}];
          s.classname = classname;
          s.key = key;
          s.value = value;
          if (s.maps.empty() || s.maps.back() != f->path) s.maps.push_back(f->path);
        }
      }
    }
  }
  // A .bsp a map names as a model is shipped when the set holds it.
  for (auto& [k, a] : assets) {
    if (a.kind != AssetKind::model || a.shipped || !ends_with(lower(a.name), ".bsp")) continue;
    a.shipped = stored.count(a.name) != 0;
  }

  for (auto& [k, a] : assets) scan.assets.push_back(std::move(a));
  for (auto& [k, s] : sets) scan.sets.push_back(std::move(s));
  std::stable_sort(scan.sets.begin(), scan.sets.end(), [](const SetUse& a, const SetUse& b) {
    if (a.classname != b.classname) return a.classname < b.classname;
    if (a.key != b.key) return a.key < b.key;
    return value_less(a.value, b.value);
  });
  return scan;
}

bool parse_asset_map(std::string_view text, AssetMap* out, std::string* error) {
  out->lines.clear();
  int number = 0;
  size_t at = 0;
  while (at <= text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    std::string_view line = text.substr(at, end - at);
    at = end + 1;
    ++number;
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

    // The comment: a '#' at the start or after whitespace.
    bool pinned = false;
    for (size_t i = 0; i < line.size(); ++i) {
      if (line[i] == '#' && (i == 0 || line[i - 1] == ' ' || line[i - 1] == '\t')) {
        std::string_view comment = line.substr(i + 1);
        while (!comment.empty() && (comment.front() == ' ' || comment.front() == '\t')) {
          comment.remove_prefix(1);
        }
        pinned = comment.starts_with("pin");
        line = line.substr(0, i);
        break;
      }
    }
    std::vector<std::string_view> words;
    for (size_t i = 0; i < line.size();) {
      while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
      const size_t from = i;
      while (i < line.size() && line[i] != ' ' && line[i] != '\t') ++i;
      if (i > from) words.push_back(line.substr(from, i - from));
    }
    if (words.empty()) continue;

    AssetMapLine l;
    l.line = number;
    l.pinned = pinned;
    auto fail = [&](const std::string& why) {
      *error = "line " + std::to_string(number) + ": " + why;
      return false;
    };
    if (words[0] == "sound" || words[0] == "model") {
      if (words.size() != 3) {
        return fail("a `" + std::string(words[0]) + "` line is `" + std::string(words[0]) +
                    " <classic path> <our name>`");
      }
      l.kind = words[0] == "sound" ? AssetKind::sound : AssetKind::model;
      l.classic = words[1];
      l.ours = words[2];
    } else if (words[0] == "set") {
      const size_t eq = words.size() == 5 ? words[2].find('=') : std::string_view::npos;
      if (eq == std::string_view::npos || eq == 0 || eq + 1 == words[2].size()) {
        return fail("a `set` line is `set <classname> <key>=<value> <field> <our name>`");
      }
      l.set = true;
      l.classname = words[1];
      l.key = words[2].substr(0, eq);
      l.value = words[2].substr(eq + 1);
      l.field = words[3];
      l.ours = words[4];
    } else {
      return fail("`" + std::string(words[0]) + "` is not `sound`, `model` or `set`");
    }
    out->lines.push_back(std::move(l));
  }
  return true;
}

ContentMatch compare_content(const ContentScan& scan, const AssetMap& map) {
  std::map<std::pair<AssetKind, std::string>, std::string> named;
  std::map<std::tuple<std::string, std::string, std::string>, std::string> fields;
  // The world's type is read by the classes that pick by it (locks, keys): a world set is mapped
  // by every class's row for that type, each named `<class>.<field>`.
  std::map<std::string, std::string> world_types;
  for (const AssetMapLine& l : map.lines) {
    if (!l.set) {
      named.emplace(std::pair{l.kind, l.classic}, l.ours);
      continue;
    }
    std::string& f = fields[{l.classname, l.key, l.value}];
    f += (f.empty() ? "" : " ") + l.field + "=" + l.ours;
    if (l.key == "worldtype") {
      std::string& w = world_types[l.value];
      w += (w.empty() ? "" : " ") + l.classname + "." + l.field + "=" + l.ours;
    }
  }
  ContentMatch m;
  m.asset_ours.reserve(scan.assets.size());
  for (const ScannedAsset& a : scan.assets) {
    const auto it = named.find({a.kind, a.name});
    m.asset_ours.push_back(it == named.end() ? "" : it->second);
  }
  m.set_ours.reserve(scan.sets.size());
  for (const SetUse& s : scan.sets) {
    if (s.classname == "worldspawn" && s.key == "worldtype") {
      const auto w = world_types.find(s.value);
      m.set_ours.push_back(w == world_types.end() ? "" : w->second);
      continue;
    }
    const auto it = fields.find({s.classname, s.key, s.value});
    m.set_ours.push_back(it == fields.end() ? "" : it->second);
  }
  return m;
}

bool content_fixture(ContentFixture* out, std::string* error) {
  const auto entity = [](std::vector<std::pair<std::string, std::string>> pairs) {
    Entity e;
    e.pairs = std::move(pairs);
    return e;
  };
  Fixture hall = sample_fixture();
  hall.world_keys = {{"worldtype", "1"}, {"sounds", "4"}};
  hall.entities = {
      entity({{"classname", "func_door"}, {"sounds", "2"}, {"model", "*1"}}),
      entity({{"classname", "func_door"}, {"sounds", "2"}, {"model", "*2"}}),
      entity({{"classname", "func_button"}, {"sounds", "1"}, {"model", "*3"}}),
      entity({{"classname", "ambient_sound"}, {"noise", "misc/hum.wav"}}),
      entity({{"classname", "trigger_once"}, {"noise", "misc/gong.wav"}}),
      entity({{"classname", "prop_crate"}, {"model", "progs/crate.mdl"}}),
      entity({{"classname", "item_box"}, {"model", "maps/b_box.bsp"}}),
  };
  Fixture side = sample_fixture();
  side.message = "The fixture side halls";
  side.entities = {
      entity({{"classname", "func_door"}, {"sounds", "2"}, {"noise1", "doors/slide.wav"}}),
      entity({{"classname", "func_plat"}, {"sounds", "1"}}),
  };
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  files.push_back({"sound/doors/slide.wav", std::vector<uint8_t>(64, 1)});
  files.push_back({"sound/misc/hum.wav", std::vector<uint8_t>(64, 2)});
  files.push_back({"progs/crate.mdl", std::vector<uint8_t>(64, 3)});
  files.push_back({"maps/hall.bsp", {}});
  files.push_back({"maps/side.bsp", {}});
  if (!write_bsp(hall, Family::v29, &files[3].second, error)) return false;
  if (!write_bsp(side, Family::v38, &files[4].second, error)) return false;
  out->asset_map =
      "# The fixture's asset map.\n"
      "sound doors/slide.wav door/slide\n"
      "sound misc/hum.wav ambient/hum  # pin\n"
      "model progs/crate.mdl prop/crate\n"
      "set func_door sounds=2 noise1 door/slide_stop\n"
      "set func_door sounds=2 noise2 door/slide\n"
      "set item_box worldtype=1 noise pickup/box\n";
  return vfs::write_pak(files, &out->pak, error);
}

}  // namespace ge::brush
