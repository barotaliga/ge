// The brush tool (tool.hpp): info, ents and check as JSON.
//
// ge:summary The brush tool's commands: counts, entities near a point, liquids, a slice, check
// ge:keywords brush bsp tool info ents check json search-path near liquids slice deepest
// ge:skill brush agent-interface
// ge:test tests/brush_tool_test.cpp
#include "brush/tool.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "brush/anim.hpp"
#include "brush/read.hpp"
#include "brush/survey.hpp"
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
  bool near = false, deepest = false;
  float at[3] = {};
  double radius = 256, step = 0, plane = 0;
  int axis = -1;
};

bool number(const std::string& s, double* v) {
  char* end = nullptr;
  *v = std::strtod(s.c_str(), &end);
  return !s.empty() && end && *end == '\0' && std::isfinite(*v);
}

std::string num(double v) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.6g", v);
  return buf;
}

std::string vec(const float v[3]) {
  return "[" + num(v[0]) + "," + num(v[1]) + "," + num(v[2]) + "]";
}

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
    } else if (s.rfind("--near=", 0) == 0) {
      double v[3];
      if (std::sscanf(s.c_str() + 7, "%lf,%lf,%lf", &v[0], &v[1], &v[2]) != 3) {
        *bad = "--near takes x,y,z in map units, as " + s + " does not";
        return false;
      }
      for (int k = 0; k < 3; ++k) a->at[k] = float(v[k]);
      a->near = true;
    } else if (s.rfind("--radius=", 0) == 0) {
      if (!number(s.substr(9), &a->radius) || a->radius < 0) {
        *bad = "--radius takes a distance in map units, as " + s + " does not";
        return false;
      }
    } else if (s.rfind("--step=", 0) == 0) {
      if (!number(s.substr(7), &a->step) || a->step <= 0) {
        *bad = "--step takes a cell size in map units above 0, as " + s + " does not";
        return false;
      }
    } else if (s.size() > 4 && s[0] == '-' && s[1] == '-' && s[2] >= 'x' && s[2] <= 'z' &&
               s[3] == '=') {
      if (a->axis >= 0 || !number(s.substr(4), &a->plane)) {
        *bad = "slice takes one plane, --x=, --y= or --z= a coordinate in map units";
        return false;
      }
      a->axis = s[2] - 'x';
    } else if (s == "--deepest") {
      a->deepest = true;
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

// The map named by the command's one word, read; false with the answer printed.
bool open_map(const Args& a, const char* command, Map* m, std::ostream& out, int* code) {
  if (a.words.size() != 1) {
    *code = fail(out, 2, std::string(command) + " takes one map");
    return false;
  }
  std::vector<uint8_t> bytes;
  std::string error;
  if (!load(a, a.words[0], &bytes, &error)) {
    *code = fail(out, 1, error);
    return false;
  }
  if (!read_bsp(bytes, m, &error)) {
    *code = fail(out, 1, a.words[0] + ": " + error);
    return false;
  }
  return true;
}

int ents(const Args& a, std::ostream& out) {
  Map m;
  int code = 0;
  if (!open_map(a, "ents", &m, out, &code)) return code;
  struct Hit {
    size_t index;
    double distance;
    float mins[3], maxs[3];
    bool brush;
  };
  std::vector<Hit> hits;
  for (size_t i = 0; i < m.entities.size(); ++i) {
    const Entity& e = m.entities[i];
    if (!a.klass.empty() && !glob_match(a.klass, e.get("classname"))) continue;
    Hit h{i, 0, {}, {}, false};
    if (entity_place(m, e, h.mins, h.maxs)) {
      h.brush = h.mins[0] != h.maxs[0] || h.mins[1] != h.maxs[1] || h.mins[2] != h.maxs[2];
      h.distance = box_distance(h.mins, h.maxs, a.at);
    } else if (a.near) {
      continue;
    }
    if (a.near && h.distance > a.radius) continue;
    hits.push_back(h);
  }
  if (a.near) {
    std::stable_sort(hits.begin(), hits.end(),
                     [](const Hit& x, const Hit& y) { return x.distance < y.distance; });
  }
  out << "{\"ok\":true,\"map\":" << esc(a.words[0]);
  if (a.near) out << ",\"near\":" << vec(a.at) << ",\"radius\":" << num(a.radius);
  out << ",\"entities\":[";
  for (size_t j = 0; j < hits.size(); ++j) {
    const Hit& h = hits[j];
    out << (j ? "," : "") << "{\"index\":" << h.index;
    if (a.near) out << ",\"distance\":" << num(h.distance);
    if (h.brush) out << ",\"mins\":" << vec(h.mins) << ",\"maxs\":" << vec(h.maxs);
    for (const auto& [k, v] : m.entities[h.index].pairs) out << "," << esc(k) << ":" << esc(v);
    out << "}";
  }
  out << "]}\n";
  return 0;
}

// The narrowest pool `liquids --deepest` takes: a standing figure's width, in map units.
constexpr float kEyeRoom = 32;

int liquids(const Args& a, std::ostream& out) {
  Map m;
  int code = 0;
  if (!open_map(a, "liquids", &m, out, &code)) return code;
  std::vector<LiquidBody> bodies = liquid_bodies(m);
  if (a.deepest && bodies.size() > 1) {
    // The deepest an eye fits in: a slab narrower than a figure is a window, not a pool.
    size_t pick = 0;
    while (pick < bodies.size() && bodies[pick].narrowest() < kEyeRoom) ++pick;
    bodies = {bodies[pick < bodies.size() ? pick : 0]};
  }
  out << "{\"ok\":true,\"map\":" << esc(a.words[0]) << ",\"liquids\":[";
  for (size_t i = 0; i < bodies.size(); ++i) {
    const LiquidBody& b = bodies[i];
    out << (i ? "," : "") << "{\"kind\":\"" << cell_name(b.kind) << "\",\"mins\":" << vec(b.mins)
        << ",\"maxs\":" << vec(b.maxs) << ",\"depth\":" << num(b.depth())
        << ",\"narrowest\":" << num(b.narrowest()) << ",\"leaves\":" << b.leaves << "}";
  }
  out << "]";
  if (bodies.empty()) out << ",\"notes\":[\"no leaf holds water, slime or lava\"]";
  out << "}\n";
  return 0;
}

int slice(const Args& a, std::ostream& out) {
  if (a.axis < 0) return fail(out, 2, "slice needs a plane: --x=, --y= or --z= in map units");
  Map m;
  int code = 0;
  if (!open_map(a, "slice", &m, out, &code)) return code;
  Slice s;
  std::string error;
  if (!slice_map(m, a.axis, a.plane, a.step, &s, &error)) return fail(out, 1, error);
  const char* axes = "xyz";
  out << "{\"ok\":true,\"map\":" << esc(a.words[0]) << ",\"plane\":\"" << axes[s.axis] << "="
      << num(s.at) << "\",\"step\":" << num(s.step) << ",\"columns\":\"" << axes[s.u_axis]
      << "\",\"rows\":\"" << axes[s.v_axis] << "\",\"first_column\":" << num(s.u0)
      << ",\"first_row\":" << num(s.v0)
      << ",\"legend\":{\".\":\"empty\",\"#\":\"solid\",\"~\":\"water\",\"%\":\"slime\","
         "\"!\":\"lava\",\"^\":\"sky\"},\"grid\":[";
  for (size_t r = 0; r < s.rows.size(); ++r) out << (r ? "," : "") << esc(s.rows[r]);
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
                    "; next: info <map> | ents <map> [--class=<glob>] [--near=x,y,z] | liquids "
                    "<map> | slice <map> --x=<u> | check, with --root=<dir> --game=<name> for a "
                    "search path");
  if (a.command == "help") {
    out << "{\"ok\":true,\"commands\":[\"info <map>\","
           "\"ents <map> [--class=<glob>] [--near=x,y,z [--radius=<units>]]\","
           "\"liquids <map> [--deepest]\",\"slice <map> --x=|--y=|--z=<units> [--step=<units>]\","
           "\"check\"],\"flags\":[\"--root=<dir>\",\"--game=<name>[,<mod>...]\"]}\n";
    return 0;
  }
  if (a.command == "info") return info(a, out);
  if (a.command == "ents") return ents(a, out);
  if (a.command == "liquids") return liquids(a, out);
  if (a.command == "slice") return slice(a, out);
  if (a.command == "check") return check(a, out);
  return fail(out, 2,
              "unknown command " + a.command + "; next: info, ents, liquids, slice, check or help");
}

}  // namespace ge::brush
