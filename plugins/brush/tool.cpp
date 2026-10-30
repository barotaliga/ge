// The brush tool (tool.hpp): info, ents and check as JSON.
//
// ge:summary The brush tool's commands: a map's counts and entities, every map on a path checked
// ge:keywords brush bsp tool info ents check json search-path
// ge:skill brush agent-interface
// ge:test tests/brush_tool_test.cpp
#include "brush/tool.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "brush/anim.hpp"
#include "brush/read.hpp"
#include "vfs/search.hpp"

namespace ge::brush {
namespace {

std::string esc(const std::string& s) {
  std::string out = "\"";
  for (char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out + "\"";
}

struct Args {
  std::string command;
  std::vector<std::string> words;
  std::string root, game, klass;
};

bool parse(int argc, const char* const* argv, Args* a, std::string* bad) {
  if (argc < 2) {
    *bad = "no command";
    return false;
  }
  a->command = argv[1];
  for (int i = 2; i < argc; ++i) {
    const std::string s = argv[i];
    if (s.rfind("--root=", 0) == 0) {
      a->root = s.substr(7);
    } else if (s.rfind("--game=", 0) == 0) {
      a->game = s.substr(7);
    } else if (s.rfind("--class=", 0) == 0) {
      a->klass = s.substr(8);
    } else if (s.rfind("--", 0) == 0) {
      *bad = "unknown flag " + s;
      return false;
    } else {
      a->words.push_back(s);
    }
  }
  return true;
}

int fail(std::ostream& out, int code, const std::string& error) {
  out << "{\"ok\":false,\"error\":" << esc(error) << "}\n";
  return code;
}

bool mount(const Args& a, vfs::SearchPath* sp, std::string* error) {
  size_t at = 0;
  while (at <= a.game.size()) {
    size_t end = a.game.find(',', at);
    if (end == std::string::npos) end = a.game.size();
    const std::string g = a.game.substr(at, end - at);
    if (!g.empty() && !vfs::add_game(sp, a.root, g, error)) return false;
    at = end + 1;
  }
  return true;
}

// The map's bytes: a name on the search path when --root is given, else a file.
bool load(const Args& a, const std::string& name, std::vector<uint8_t>* bytes, std::string* error) {
  if (!a.root.empty()) {
    vfs::SearchPath sp;
    if (!mount(a, &sp, error)) return false;
    const vfs::FileRef* f = vfs::find(sp, name);
    if (!f) {
      *error = name + " is not on the search path; next: `check` lists the maps it holds";
      return false;
    }
    return vfs::read(sp, *f, bytes, error);
  }
  std::ifstream in(name, std::ios::binary);
  if (!in) {
    *error = "cannot open " + name + "; next: pass a map file, or --root and --game";
    return false;
  }
  bytes->assign(std::istreambuf_iterator<char>(in), {});
  return true;
}

std::string counts(const Map& m) {
  size_t clusters = size_t(m.clusters);
  std::string s = "\"family\":" + esc(family_name(m.family));
  const auto n = [&](const char* k, size_t v) {
    s += ",\"" + std::string(k) + "\":" + std::to_string(v);
  };
  n("vertices", m.vertices.size());
  n("triangles", m.indices.size() / 3);
  n("surfaces", m.surfaces.size());
  n("textures", m.textures.size());
  n("lightmaps", m.lightmaps.size());
  n("leaves", m.leaves.size());
  n("clusters", clusters);
  n("models", m.models.size());
  n("clip_nodes", m.clip_nodes.size());
  n("entities", m.entities.size());
  n("bspx_lumps", m.bspx.size());
  return s;
}

int info(const Args& a, std::ostream& out) {
  if (a.words.size() != 1) return fail(out, 2, "info takes one map");
  std::vector<uint8_t> bytes;
  std::string error;
  if (!load(a, a.words[0], &bytes, &error)) return fail(out, 1, error);
  Map m;
  if (!read_bsp(bytes, &m, &error)) return fail(out, 1, a.words[0] + ": " + error);
  out << "{\"ok\":true,\"map\":" << esc(a.words[0]) << "," << counts(m);
  const TextureAnims anims = build_texture_anims(m);
  out << ",\"texture_sequences\":[";
  for (size_t i = 0; i < anims.sets.size(); ++i) {
    const TextureSet& t = anims.sets[i];
    out << (i ? "," : "") << "{\"name\":" << esc(t.name) << ",\"kind\":\""
        << (t.random ? "random" : "cycle") << "\",\"frames\":" << t.frames.size()
        << ",\"alternate\":" << t.alt.size() << "}";
  }
  out << "],\"notes\":[";
  for (size_t i = 0; i < anims.notes.size(); ++i) out << (i ? "," : "") << esc(anims.notes[i]);
  out << "],\"bspx\":[";
  for (size_t i = 0; i < m.bspx.size(); ++i) {
    out << (i ? "," : "") << "{\"name\":" << esc(m.bspx[i].name)
        << ",\"bytes\":" << m.bspx[i].bytes.size() << "}";
  }
  out << "]}\n";
  return 0;
}

int ents(const Args& a, std::ostream& out) {
  if (a.words.size() != 1) return fail(out, 2, "ents takes one map");
  std::vector<uint8_t> bytes;
  std::string error;
  if (!load(a, a.words[0], &bytes, &error)) return fail(out, 1, error);
  Map m;
  if (!read_bsp(bytes, &m, &error)) return fail(out, 1, a.words[0] + ": " + error);
  out << "{\"ok\":true,\"map\":" << esc(a.words[0]) << ",\"entities\":[";
  bool first = true;
  for (size_t i = 0; i < m.entities.size(); ++i) {
    const Entity& e = m.entities[i];
    if (!a.klass.empty() && e.get("classname") != a.klass) continue;
    out << (first ? "" : ",") << "{\"index\":" << i;
    for (const auto& [k, v] : e.pairs) out << "," << esc(k) << ":" << esc(v);
    out << "}";
    first = false;
  }
  out << "]}\n";
  return 0;
}

int check(const Args& a, std::ostream& out) {
  if (a.root.empty() || a.game.empty())
    return fail(out, 2, "check needs --root=<dir> and --game=<name>");
  vfs::SearchPath sp;
  std::string error;
  if (!mount(a, &sp, &error)) return fail(out, 1, error);
  size_t read = 0, refused = 0;
  std::string maps;
  for (const vfs::Listed& l : vfs::list(sp)) {
    const std::string name(l.name);
    if (name.size() < 4 || name.compare(name.size() - 4, 4, ".bsp") != 0) continue;
    std::vector<uint8_t> bytes;
    Map m;
    const bool ok = vfs::read(sp, l.ref, &bytes, &error) && read_bsp(bytes, &m, &error);
    maps += std::string(maps.empty() ? "" : ",") + "{\"map\":" + esc(name) +
            (ok ? "," + counts(m) : ",\"error\":" + esc(error)) + "}";
    ok ? ++read : ++refused;
  }
  out << "{\"ok\":" << (refused ? "false" : "true") << ",\"read\":" << read
      << ",\"refused\":" << refused << ",\"maps\":[" << maps << "]";
  if (refused)
    out << ",\"error\":" << esc(std::to_string(refused) + " maps were refused; each names why");
  out << "}\n";
  return refused ? 1 : 0;
}

}  // namespace

int bsp_main(int argc, const char* const* argv, std::ostream& out) {
  Args a;
  std::string bad;
  if (!parse(argc, argv, &a, &bad))
    return fail(out, 2,
                bad +
                    "; next: info <map> | ents <map> [--class=<name>] | check, with "
                    "--root=<dir> --game=<name> for a search path");
  if (a.command == "help") {
    out << "{\"ok\":true,\"commands\":[\"info <map>\",\"ents <map> [--class=<classname>]\","
           "\"check\"],\"flags\":[\"--root=<dir>\",\"--game=<name>[,<mod>...]\"]}\n";
    return 0;
  }
  if (a.command == "info") return info(a, out);
  if (a.command == "ents") return ents(a, out);
  if (a.command == "check") return check(a, out);
  return fail(out, 2, "unknown command " + a.command + "; next: info, ents, check or help");
}

}  // namespace ge::brush
