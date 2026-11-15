// The map commands in a game's player CLI (commands.hpp).
//
// ge:summary The map build and map plan commands, and the brush tool run in-process for map rows
// ge:keywords gamecli map build plan svg compile_map leak textures report run_tool bsp_main
// ge:skill brush agent-interface
// ge:test tests/gamecli_cli_test.cpp
#include "brush/cli/commands.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

#include "brush/compile/compile.hpp"
#include "brush/compile/library.hpp"
#include "brush/compile/plan.hpp"
#include "brush/compile/svg.hpp"
#include "brush/tool.hpp"
#include "core/json_write.hpp"

namespace ge::brush::cli {

namespace {

namespace fs = std::filesystem;
using core::json_bool;
using core::json_num;
using core::json_obj;
using core::json_str;

bool read_text(const std::string& path, std::string* out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  out->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}

bool is_svg(const std::string& path) {
  const std::string e = fs::path(path).extension().string();
  return e == ".svg" || e == ".SVG";
}

int cmd_plan(gamecli::Ctx& ctx) {
  if (ctx.words.size() != 1 || !is_svg(ctx.words[0])) {
    return gamecli::refuse(ctx, 2, "map plan takes one SVG drawing", "run map plan <drawing.svg>");
  }
  std::string text, plan, error;
  if (!read_text(ctx.words[0], &text)) {
    return gamecli::refuse(ctx, 1, "cannot read " + ctx.words[0], "check the path");
  }
  if (!mapc::svg_to_plan(text, &plan, &error)) {
    return gamecli::refuse(ctx, 1, error, "fix the drawing's shapes the lines name");
  }
  return gamecli::answer(ctx, json_obj({{"plan", json_str(plan)}}), plan,
                         "the plan " + ctx.words[0] + " reads as");
}

int cmd_build(gamecli::Ctx& ctx) {
  if (ctx.words.size() != 1) {
    return gamecli::refuse(ctx, 2, "map build takes one plan or SVG drawing",
                           "run map build <level.plan|level.svg> [--out=<level.bsp>]");
  }
  const std::string in = ctx.words[0];
  std::string text, plan, error;
  if (!read_text(in, &text)) return gamecli::refuse(ctx, 1, "cannot read " + in, "check the path");
  if (is_svg(in)) {
    if (!mapc::svg_to_plan(text, &plan, &error)) {
      return gamecli::refuse(ctx, 1, error, "fix the drawing's shapes the lines name");
    }
  } else {
    plan = text;
  }
  mapc::Source src;
  if (!mapc::read_plan(plan, &src, &error)) {
    return gamecli::refuse(ctx, 1, error, "fix the plan's lines it names");
  }
  // The pictures: --textures, else the install.
  mapc::TextureLibrary lib;
  std::string tex_from = ctx.flag("textures");
  if (tex_from.empty() && gamecli::try_places(ctx)) tex_from = ctx.places.install;
  std::string tex_note = "patterns (no folder of maps given)";
  mapc::CompileOptions opt;
  if (!tex_from.empty()) {
    std::string terr;
    if (mapc::load_texture_library(tex_from, &lib, &terr)) {
      opt.textures = mapc::finder_of(lib);
      tex_note = std::to_string(lib.by_name.size()) + " pictures from " + tex_from;
    } else {
      tex_note = "patterns (" + terr + ")";
    }
  }
  opt.light = !ctx.has("no-light");
  std::vector<uint8_t> bytes;
  mapc::CompileReport rep;
  if (!mapc::compile_map(src, opt, &bytes, &rep, &error)) {
    if (rep.leaked && ctx.has("leak")) {
      std::ofstream pts(ctx.flag("leak"));
      for (const mapc::V3& p : rep.leak_path) pts << p[0] << " " << p[1] << " " << p[2] << "\n";
    }
    return gamecli::refuse(ctx, 1, error,
                           rep.leaked ? "seal the gap, then build again (--leak=<file> writes "
                                        "the way out as points)"
                                      : "fix the source");
  }
  std::string out = ctx.flag("out");
  if (out.empty()) out = fs::path(in).replace_extension(".bsp").string();
  {
    std::ofstream f(out, std::ios::binary);
    if (!f) return gamecli::refuse(ctx, 1, "cannot write " + out, "check the folder exists");
    f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
  }
  std::string warn;
  for (const std::string& w : rep.warnings) warn += "  warning: " + w + "\n";
  const std::string text_out =
      out + ": " + std::to_string(bytes.size()) + " bytes\n  " + std::to_string(rep.brushes) +
      " brushes, " + std::to_string(rep.faces) + " faces, " + std::to_string(rep.leaves) +
      " leaves, " + std::to_string(rep.nodes) + " nodes, " + std::to_string(rep.clip_nodes) +
      " clip nodes, " + std::to_string(rep.models) + " models\n  " + std::to_string(rep.lights) +
      " lights over " + std::to_string(rep.luxels) + " luxels\n  " + std::to_string(rep.textures) +
      " textures, " + std::to_string(rep.textures_found) + " found: " + tex_note + "\n" + warn;
  return gamecli::answer(
      ctx,
      json_obj({{"out", json_str(out)},
                {"bytes", json_num(static_cast<long long>(bytes.size()))},
                {"brushes", json_num(static_cast<long long>(rep.brushes))},
                {"faces", json_num(static_cast<long long>(rep.faces))},
                {"leaves", json_num(static_cast<long long>(rep.leaves))},
                {"nodes", json_num(static_cast<long long>(rep.nodes))},
                {"clip_nodes", json_num(static_cast<long long>(rep.clip_nodes))},
                {"models", json_num(static_cast<long long>(rep.models))},
                {"lights", json_num(static_cast<long long>(rep.lights))},
                {"textures", json_num(static_cast<long long>(rep.textures))},
                {"textures_found", json_num(static_cast<long long>(rep.textures_found))},
                {"warnings", json_num(static_cast<long long>(rep.warnings.size()))},
                {"lit", json_bool(opt.light)}}),
      text_out, "built " + out,
      "put it in the game's maps folder and start it: map " + fs::path(out).stem().string());
}

}  // namespace

std::vector<gamecli::Command> commands() {
  return {{"map build", "<plan|svg>", "compile a map plan or an SVG floor plan into a BSP map",
           "map build arena.plan --out=maps/arena.bsp", "brush", cmd_build},
          {"map plan", "<svg>", "print the map plan an SVG floor plan reads as",
           "map plan arena.svg", "brush", cmd_plan}};
}

std::vector<gamecli::CommandFlags> command_flags() {
  return {{"map build",
           {{"out", "<file.bsp>", "where the map is written (default: beside the source, as .bsp)"},
            {"textures", "<folder>",
             "take the pictures from the maps in this folder's packs (default: the install)"},
            {"no-light", "", "skip the light: every face at the world's least light"},
            {"leak", "<file>", "on a leak, write its way out as points, one x y z a line"}}}};
}

std::vector<std::string> tools() { return {"brush"}; }

toolkit::ToolRun run_tool(std::string_view program, std::string_view name,
                          const std::vector<std::string>& args) {
  toolkit::ToolRun r;
  if (name != "brush") {
    r.code = -1;
    r.findings.push_back({toolkit::Severity::error,
                          "no tool called '" + std::string(name) + "' is linked", "",
                          std::string(program) + " help (the commands it has)"});
    return r;
  }
  const std::string name0(program);
  std::vector<const char*> argv{name0.c_str()};
  for (const std::string& a : args) argv.push_back(a.c_str());
  std::ostringstream out;
  r.code = bsp_main(static_cast<int>(argv.size()), argv.data(), out);
  r.output = out.str();
  return r;
}

}  // namespace ge::brush::cli
