// The window's place names (place_names.hpp).
//
// ge:summary The window's place names: read at map start, added, removed and saved in the game
// ge:keywords place-names map_started add_place_commands addloc saveloc
// ge:skill shooter console
// ge:test tests/shooters_edge_place_names_test.cpp
#include "shooters_edge/client/place_names.hpp"

#include <cstdio>
#include <fstream>
#include <system_error>
#include <utility>

namespace shooters_edge::client {

namespace {

namespace fs = std::filesystem;
namespace con = ge::console;
namespace sh = ge::shooter;

using CArgs = con::Args;

// A file's name as a player gives it: `.loc` added when it has no dot; "" when it would leave
// the locs folder.
std::string file_name(const std::string& name) {
  if (name.empty() || name.find("..") != std::string::npos ||
      name.find_first_of("/\\:") != std::string::npos)
    return {};
  return name.find('.') == std::string::npos ? name + ".loc" : name;
}

// The text of `locs/<file>`: the player's saved one under the game folder, else the game's own.
bool read_file(const PlaceNames& p, const SearchPath& files, const std::string& file,
               std::string& text) {
  if (!p.game_dir.empty()) {
    std::ifstream f(p.game_dir / "locs" / file, std::ios::binary);
    if (f) {
      text.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
      return true;
    }
  }
  std::vector<uint8_t> store;
  const std::span<const uint8_t> bytes = find_file(files, "locs/" + file, store);
  if (bytes.empty()) return false;
  text.assign(bytes.begin(), bytes.end());
  return true;
}

float range_of(const PlaceNames& p) {
  return p.range != nullptr ? static_cast<float>(p.range->number) : 0;
}

std::string lower(std::string s) {
  for (char& ch : s) ch = ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch;
  return s;
}

std::string at_text(sh::Vec3f v) {
  char s[64];
  std::snprintf(s, sizeof s, "(%4.0f, %4.0f, %4.0f)", static_cast<double>(v.x),
                static_cast<double>(v.y), static_cast<double>(v.z));
  return s;
}

}  // namespace

void map_started(PlaceNames& p, const GameAdapter& adapter, const SearchPath& files,
                 const std::string& map) {
  if (map == p.map) return;  // the same map again: the points as they are, edits and all
  p.map = map;
  p.locs = {};
  if (map.empty() || (p.load != nullptr && !p.load->on())) return;
  std::string text;
  if (read_file(p, files, map + ".loc", text) && sh::parse_locs(text, p.locs) > 0) {
    p.locs.from = "locs/" + map + ".loc";
    return;
  }
  p.locs = {};
  if (adapter.open_locations != nullptr) adapter.open_locations(adapter, files, map, p.locs);
}

void add_place_commands(con::Console& con, PlaceNames& p, const GameAdapter& adapter,
                        const SearchPath& files, const WhereAmI& where) {
  con::Cvars& v = con.cvars();
  p.load = &v.add({.name = "tp_loadlocs",
                   .fallback = "1",
                   .flags = con::kArchive,
                   .help = "1: each map's place names are read when it starts",
                   .kind = con::CvarKind::boolean});
  p.someplace = &v.add({.name = "tp_name_someplace",
                        .fallback = "someplace",
                        .flags = con::kArchive,
                        .help = "where you are when the map has no place names",
                        .kind = con::CvarKind::text});
  p.range = &v.add({.name = "loc_dist",
                    .fallback = adapter.game != nullptr ? adapter.game->place_range : "0",
                    .flags = con::kArchive,
                    .help = "How far a place name reaches from its point, in units.",
                    .kind = con::CvarKind::number,
                    .min = 0,
                    .values = "0: no limit; NUMBER: a point farther than this is no place"});
  // The sequel's place macros: where the player is and where the view looks, `unknown` when no
  // place is within reach or no game is playing.
  if (adapter.game != nullptr && adapter.game->place_macros) {
    con.add_macro("loc_here", [&p, where] {
      sh::Vec3f at;
      if (!where || !where(at)) return std::string("unknown");
      const std::string name = sh::loc_name_at(p.locs, at, range_of(p));
      return name.empty() ? std::string("unknown") : name;
    });
    con.add_macro("loc_there", [&p] {
      sh::Vec3f at;
      if (!p.looked_at || !p.looked_at(at)) return std::string("unknown");
      const std::string name = sh::loc_name_at(p.locs, at, range_of(p));
      return name.empty() ? std::string("unknown") : name;
    });
  }
  // Each command under its name and its older short name, the same command either way.
  const auto both = [&con](const char* name, const char* old_name, const std::string& help,
                           const con::CommandFn& fn) {
    con.add_command(name, help, fn);
    con.add_command(old_name, help, fn);
  };
  const auto add = [&p, where](con::Console& c, const CArgs& a) {
    if (a.size() != 2) {
      c.print("Usage: addloc <name>");
      return;
    }
    sh::Vec3f at;
    if (!where || !where(at)) {
      c.print("You need to be in a game to add a place.");
      return;
    }
    sh::add_loc(p.locs, at, a[1]);
    c.print("Added place \"" + a[1] + "\" at " + at_text(at));
  };
  both("locations_add", "addloc", "locations_add <name>: name the place where you stand", add);
  const auto remove = [&p, where](con::Console& c, const CArgs&) {
    sh::Vec3f at{0, 0, 0};
    if (where) where(at);
    sh::Loc gone;
    if (!sh::remove_nearest_loc(p.locs, at, &gone, range_of(p))) {
      c.print("There are no places left to remove.");
      return;
    }
    c.print("Removed place \"" + (gone.raw.empty() ? gone.name : gone.raw) + "\" at " +
            at_text(gone.at));
  };
  both("locations_remove", "removeloc", "locations_remove: forget the place nearest you", remove);
  both("locations_clearall", "clearlocs", "locations_clearall: forget every place on this map",
       [&p](con::Console& c, const CArgs&) {
         const size_t n = p.locs.points.size();
         p.locs.points.clear();
         c.print("Cleared " + std::to_string(n) + " places.");
       });
  const auto save = [&p](con::Console& c, const CArgs& a) {
    if (a.size() != 2) {
      c.print("Usage: saveloc <name>");
      return;
    }
    const std::string file = file_name(a[1]);
    if (file.empty() || file.size() > 64) {
      c.print("That is not a name a place file can have.");
      return;
    }
    if (p.locs.points.empty()) {
      c.print("There are no places to save.");
      return;
    }
    std::error_code ec;
    const fs::path dir = (p.game_dir.empty() ? fs::path(".") : p.game_dir) / "locs";
    fs::create_directories(dir, ec);
    std::ofstream f(dir / file, std::ios::binary | std::ios::trunc);
    const std::string text = sh::locs_text(p.locs);
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!f) {
      c.print("Couldn't save locs/" + file + ".");
      return;
    }
    c.print("Saved " + std::to_string(p.locs.points.size()) + " places in locs/" + file + ".");
  };
  both("locations_savefile", "saveloc",
       "locations_savefile <name>: save this map's places (saveloc $mapname)", save);
  both("locations_loadfile", "loadloc", "locations_loadfile <name>: read a place file",
       [&p, &files](con::Console& c, const CArgs& a) {
         if (a.size() != 2) {
           c.print("Usage: loadloc <name>");
           return;
         }
         const std::string file = file_name(a[1]);
         std::string text;
         if (file.empty() || !read_file(p, files, file, text)) {
           c.print("Couldn't load locs/" + (file.empty() ? a[1] : file) + ".");
           return;
         }
         sh::Locs read;
         const int n = sh::parse_locs(text, read);
         read.from = "locs/" + file;
         p.locs = std::move(read);
         c.print(n == 0 ? "locs/" + file + " has no places."
                        : "Loaded locs/" + file + " (" + std::to_string(n) + " places).");
       });
  // `loc` alone says where you are; `loc add|del|set|list|save` edits the places as the sequel's
  // client spells it.
  const auto in_game = [where](con::Console& c, sh::Vec3f& at) {
    if (where && where(at)) return true;
    c.print("You need to be in a game.");
    return false;
  };
  con.add_command(
      "loc", "loc: the place you are in; loc add|del|set|list|save edits the places",
      [&p, in_game, add, remove, save](con::Console& c, const CArgs& a) {
        const std::string sub = a.size() > 1 ? lower(a[1]) : "";
        const auto shifted = [&a](const char* name, bool joined) {
          CArgs b = a;
          b.words = {name};
          std::string rest;
          for (size_t i = 2; i < a.size(); ++i) rest += (rest.empty() ? "" : " ") + a[i];
          if (joined && !rest.empty()) b.words.push_back(rest);
          if (!joined && a.size() > 2) b.words.push_back(a[2]);
          return b;
        };
        sh::Vec3f at;
        if (sub.empty()) {
          if (!in_game(c, at)) return;
          const std::string name = sh::loc_name_at(p.locs, at, range_of(p));
          c.print(name.empty() ? (p.someplace != nullptr ? p.someplace->value : "someplace")
                               : name);
        } else if (sub == "add") {
          add(c, shifted("addloc", true));
        } else if (sub == "del") {
          remove(c, shifted("removeloc", false));
        } else if (sub == "set") {
          if (a.size() < 3) {
            c.print("Usage: loc set <name>");
            return;
          }
          if (!in_game(c, at)) return;
          const sh::Loc* near = sh::nearest_loc(p.locs, at, range_of(p));
          if (near == nullptr) {
            c.print("There is no place near you to rename.");
            return;
          }
          sh::Loc& l = p.locs.points[static_cast<size_t>(near - p.locs.points.data())];
          l.raw = shifted("", true).words[1];
          l.name = sh::expand_loc_name(l.raw);
          c.print("Renamed the place at " + at_text(l.at) + " to \"" + l.raw + "\"");
        } else if (sub == "list") {
          for (const sh::Loc& l : p.locs.points)
            c.print((l.raw.empty() ? l.name : l.raw) + " at " + at_text(l.at));
          c.print(p.locs.points.empty() ? "There are no places on this map."
                                        : std::to_string(p.locs.points.size()) + " places.");
        } else if (sub == "save") {
          CArgs b = shifted("saveloc", false);
          if (b.size() < 2) b.words.push_back(p.map);
          save(c, b);
        } else {
          c.print("Usage: loc, or loc add|del|set|list|save");
        }
      });
}

}  // namespace shooters_edge::client
