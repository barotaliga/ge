// The map group's rows and the reading of the brush tool's answer (map.hpp).
//
// ge:summary The map rows' flags and bodies, and the brush tool's JSON answer read into an Outcome
// ge:keywords toolkit map bsp brush map_answer map_rows info ents liquids slice check tool
// ge:skill agent-interface brush
// ge:test tests/toolkit_map_test.cpp
#include "toolkit/map.hpp"

#include <string>
#include <vector>

#include "core/json_read.hpp"

namespace ge::toolkit {

namespace {

using core::FlatJson;

std::string text_at(const FlatJson& m, const std::string& key) {
  const auto it = m.find(key);
  return it == m.end() ? std::string() : it->second.text;
}

int count_of(const FlatJson& m, const std::string& list, const std::string& field) {
  int n = 0;
  for (;; n++) {
    std::string key = list;
    key += '.';
    key += std::to_string(n);
    key += '.';
    key += field;
    if (!m.contains(key)) break;
  }
  return n;
}

// The tool's error, "what; next: what to do", split; the next step is this group's, spelled with
// the calling program's name (the tool's own speaks of its bare commands).
Finding refusal(const Call& call, const std::string& error, const std::string& where,
                std::string_view command = "") {
  const std::string what = error.substr(0, error.find("; next: "));
  // The import and export steps name their own next step (a gap to seal, a line to fix).
  if (const size_t at = error.find("; next: "); command == "export" && at != std::string::npos) {
    return {Severity::error, what, where, error.substr(at + 8)};
  }
  const std::string next =
      "pass a map file, or --root=<dir> --game=<name> and a name on that search path (" +
      call.program + " map check --root=<dir> --game=<name> lists them)";
  return {Severity::error, what, where, next};
}

// The command handed to the tool: its name, the map, every flag as typed.
Outcome run_map(const Call& call, std::string_view command) {
  Outcome o;
  if (!call.tool) {
    o.exit = 2;
    o.findings.push_back({Severity::error, call.program + " runs no brush tool", "",
                          call.program + " help (the commands it has)"});
    return o;
  }
  std::vector<std::string> args{std::string(command)};
  for (const std::string& w : call.words) args.push_back(w);
  for (const auto& [name, value] : call.flags)
    args.push_back("--" + name + (value.empty() ? "" : "=" + value));
  const ToolRun r = call.tool(call.program, "brush", args);
  if (r.code < 0) {
    o.findings = r.findings;
    return o;
  }
  return map_answer(call, r, command);
}

Outcome map_info(const Call& c) { return run_map(c, "info"); }
Outcome map_ents(const Call& c) { return run_map(c, "ents"); }
Outcome map_liquids(const Call& c) { return run_map(c, "liquids"); }
Outcome map_slice(const Call& c) { return run_map(c, "slice"); }
Outcome map_check(const Call& c) { return run_map(c, "check"); }
Outcome map_import(const Call& c) { return run_map(c, "import"); }
Outcome map_export(const Call& c) { return run_map(c, "export"); }

constexpr Flag kRoot{"root", "<dir>",
                     "the install folder: the map is a name on its search path, not a file"};
constexpr Flag kGame{"game", "<name>[,<mod>...]",
                     "the game folder under --root, then mods over it in order (id1, or id1,ctf)"};
constexpr Flag kOpenFlags[] = {kRoot, kGame};
constexpr Flag kEntsFlags[] = {
    kRoot,
    kGame,
    {"class", "<glob>", "only entities whose classname matches (light_torch*)"},
    {"near", "<x,y,z>", "only entities near this point in map units, nearest first"},
    {"radius", "<units>", "how near --near reaches (default 256)"}};
constexpr Flag kLiquidsFlags[] = {
    kRoot, kGame, {"deepest", "", "only the deepest pool at least 32 units across both ways"}};
constexpr Flag kSliceFlags[] = {kRoot,
                                kGame,
                                {"x", "<units>", "the plane x = units"},
                                {"y", "<units>", "the plane y = units"},
                                {"z", "<units>", "the plane z = units (height)"},
                                {"step", "<units>", "one character's size (default: the map's)"}};

constexpr Flag kImportFlags[] = {
    kRoot,
    kGame,
    {"out", "<file.map>", "where the text is written (default: the map's name, .map)"}};
constexpr Flag kExportFlags[] = {
    {"out", "<file.bsp>", "where the map is written (default: beside the source, as .bsp)"},
    {"version", "<29|38>", "the family the map is built in (default 29)"},
    {"textures", "<folder>", "a folder of maps or packs whose pictures the faces take"},
    {"no-light", "", "skip the light: every face at the world's least light"}};

constexpr Row kRows[] = {
    {"map info", "<map>", 1, 1, "see",
     "a BSP map's family (29, BSP2, 38, 46) and counts: vertices, surfaces, textures, "
     "lightmaps, leaves, clusters, models, clip nodes, entities; its texture sequences and BSPX "
     "lumps",
     kOpenFlags, "map info maps/start.bsp", "assets", map_info, "brush"},
    {"map ents", "<map>", 1, 1, "see",
     "a map's entity blocks as key and value (spawn points, items, lights, doors), by classname "
     "glob or nearest a point first, a brush entity with its box",
     kEntsFlags, "map ents dm4.bsp --class=info_player_*", "assets", map_ents, "brush"},
    {"map liquids", "<map>", 1, 1, "see",
     "a map's pools of water, slime and lava, deepest first: each one's kind, box, depth, "
     "narrowest side and leaves",
     kLiquidsFlags, "map liquids e1m1.bsp --deepest", "assets", map_liquids, "brush"},
    {"map slice", "<map>", 1, 1, "see",
     "the map cut by a plane, drawn as rows of characters: . empty, # solid, ~ water, % slime, ! "
     "lava, ^ sky",
     kSliceFlags, "map slice dm4.bsp --z=64 --step=32", "assets", map_slice, "brush"},
    {"map check", "", 0, 0, "see",
     "every map on a search path read: each one's family and counts, and why any is refused",
     kOpenFlags, "map check --root=<install> --game=id1", "assets", map_check, "brush"},
    {"map import", "<map>", 1, 1, "see",
     "a built map of any family (29, BSP2, 38, 46) as the map editors' .map text: its brushes, "
     "their textures and projections, its entities; the form the map SDK (tools/mapsdk) reads",
     kImportFlags, "map import dm4 --root=<install> --game=<folder> --out=dm4.map", "assets",
     map_import, "brush"},
    {"map export", "<file.map|file.plan>", 1, 1, "see",
     "the map editors' .map text (or a plan) built into a map: the tree, the seal, the hulls, the "
     "light",
     kExportFlags, "map export arena.map --out=arena.bsp", "assets", map_export, "brush"},
};

}  // namespace

Outcome map_answer(const Call& call, const ToolRun& run, std::string_view command) {
  Outcome out;
  // The answer is the last line that is a JSON object.
  std::string line;
  for (size_t end = run.output.size(); end > 0 && line.empty();) {
    const size_t start = run.output.rfind('\n', end - 1);
    const size_t from = start == std::string::npos ? 0 : start + 1;
    if (end > from && run.output[from] == '{') line = run.output.substr(from, end - from);
    end = start == std::string::npos ? 0 : start;
  }
  FlatJson m;
  std::string error;
  if (line.empty() || !core::read_flat(line, m, error, true)) {
    out.findings.push_back(
        {Severity::error, "the brush tool gave no answer (exit " + std::to_string(run.code) + ")",
         run.log, "read its output; the brush tool is the brush plugin's (skill: brush)"});
    return out;
  }
  out.result = line;
  const std::string map = text_at(m, "map");
  const auto ok = m.find("ok");
  const bool answered = ok != m.end() && ok->second.boolean;
  if (command == "check") {
    const int maps = count_of(m, "maps", "map");
    for (int i = 0; i < maps; i++) {
      const std::string k = "maps." + std::to_string(i) + ".";
      const std::string why = text_at(m, k + "error");
      const std::string name = text_at(m, k + "map");
      if (!why.empty()) {
        std::string what = name;
        what += " is refused: ";
        what += why;
        out.findings.push_back(
            {Severity::error, what, name, "rebuild the map, or leave it out of the game's folder"});
      } else {
        out.text += "  " + name + "  family " + text_at(m, k + "family") + ", " +
                    text_at(m, k + "surfaces") + " surfaces, " + text_at(m, k + "entities") +
                    " entities\n";
      }
    }
    if (maps == 0 && !answered) {
      out.findings.push_back(refusal(call, text_at(m, "error"), "the command line"));
      if (run.code == 2) out.exit = 2;
      return out;
    }
    out.summary = text_at(m, "read") + " maps read, " + text_at(m, "refused") + " refused";
    if (out.findings.empty())
      out.next = call.program + " map info <map> --root=... --game=... (one map's counts)";
    return out;
  }
  if (!answered) {
    out.findings.push_back(refusal(call, text_at(m, "error"),
                                   call.words.empty() ? "the command line" : call.words[0],
                                   command));
    if (run.code == 2) out.exit = 2;
    return out;
  }
  if (command == "info") {
    out.text = "  " + map + ": family " + text_at(m, "family") + "\n";
    for (const char* k : {"vertices", "triangles", "surfaces", "textures", "lightmaps", "leaves",
                          "clusters", "models", "clip_nodes", "entities", "bspx_lumps"})
      out.text += "  " + std::string(k) + " " + text_at(m, k) + "\n";
    out.summary = map + ", family " + text_at(m, "family") + ": " + text_at(m, "surfaces") +
                  " surfaces, " + text_at(m, "entities") + " entities";
    out.next = call.program + " map ents " + map + " --class=info_player_* (where players start)";
  } else if (command == "ents") {
    const int n = count_of(m, "entities", "index");
    for (int i = 0; i < n; i++) {
      const std::string k = "entities." + std::to_string(i) + ".";
      std::string row = "  #" + text_at(m, k + "index") + " " + text_at(m, k + "classname");
      if (m.contains(k + "origin")) row += " at " + text_at(m, k + "origin");
      if (m.contains(k + "distance")) row += " (" + text_at(m, k + "distance") + " away)";
      out.text += row + "\n";
    }
    out.summary = std::to_string(n) + " entities in " + map;
    out.next = call.program + " map ents " + map + " --near=x,y,z (the nearest to a point)";
  } else if (command == "liquids") {
    const int n = count_of(m, "liquids", "kind");
    for (int i = 0; i < n; i++) {
      const std::string k = "liquids." + std::to_string(i) + ".";
      out.text += "  " + text_at(m, k + "kind") + ", " + text_at(m, k + "depth") +
                  " deep, narrowest " + text_at(m, k + "narrowest") + "\n";
    }
    out.summary = n == 0 ? map + " holds no water, slime or lava"
                         : std::to_string(n) + " pools in " + map + ", deepest first";
    out.next = call.program + " map slice " + map + " --z=<height> (the pools seen from above)";
  } else if (command == "import") {
    for (const char* k : {"family", "entities", "brushes", "sides", "textured_sides", "models"})
      out.text += "  " + std::string(k) + " " + text_at(m, k) + "\n";
    out.summary = map + " written as " + text_at(m, "out");
    out.next = call.program + " map export " + text_at(m, "out") +
               " --out=<file.bsp> (build it again), or edit it with tools/mapsdk";
  } else if (command == "export") {
    for (const char* k : {"version", "bytes", "brushes", "faces", "leaves", "models", "textures",
                          "textures_found", "warnings"})
      out.text += "  " + std::string(k) + " " + text_at(m, k) + "\n";
    out.summary = text_at(m, "source") + " built into " + text_at(m, "out");
    out.next = call.program + " map info " + text_at(m, "out") + " (its counts)";
  } else if (command == "slice") {
    for (int r = 0; m.contains("grid." + std::to_string(r)); r++)
      out.text += text_at(m, "grid." + std::to_string(r)) + "\n";
    out.summary = map + " cut at " + text_at(m, "plane") + ", " + text_at(m, "step") +
                  " units a character (. empty, # solid, ~ water, % slime, ! lava, ^ sky)";
    out.next = call.program + " map liquids " + map + " (where the pools are)";
  }
  return out;
}

std::span<const Row> map_rows() { return kRows; }

}  // namespace ge::toolkit
