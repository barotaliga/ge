// LAB `server-cfg`: a server's settings in layers (engine/server/config.hpp) on one fixture: a
// base game, two mods over it (each with its own server.cfg), a profile in the base and the lower
// mod, the command line and a line typed at the server's console. It answers which layer each
// value comes from, the file and line that set it and what it overrode (the server's `--explain`),
// and draws every cvar across the layers: each cell the value a layer set, the winner marked.
//
// ge:summary Lab: server.cfg in layers: base, mods, a profile, the command line, the console
// ge:keywords lab server-cfg layers base mod profile command-line explain override picture
// ge:skill net labs
// ge:test tests/server_cfg_test.cpp
#include <map>
#include <string>
#include <vector>

#include "console/console.hpp"
#include "lab/lab.hpp"
#include "server/config.hpp"
#include "server/main.hpp"
#include "server/sample.hpp"
#include "ui/draw.hpp"
#include "ui/font.hpp"
#include "ui/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace gs = ge::server;
namespace gu = ge::ui;

using Files = std::map<std::string, std::string>;

// The fixture: what an owner might keep beside the binary.
const Files kFiles = {
    {"base/server.cfg",
     "// the base game's\n"
     "sv_hostname \"Edge Base\"\n"
     "sv_maxclients 16\n"
     "sv_description \"the base game\"\n"
     "sv_motd \"welcome\"\n"
     "sv_admin \"owner on the forum\"\n"},
    {"base/duel.cfg", "sv_maxclients 4\n"},
    {"arena/server.cfg", "sv_hostname \"Arena Night\"\nsv_map dm3\n"},
    {"arena/duel.cfg", "sv_maxclients 2\nsv_description \"one on one\"\n"},
    {"ctf/server.cfg", "sv_maxclients 16\nsv_maprotation \"ctf1 ctf2\"\nsv_map ctf1\n"},
};

// A column of the picture: a layer and, for a file layer, the file's directory.
struct Column {
  std::string head;
  gs::Layer layer;
  std::string dir;  // "" any
};

bool in_column(const Column& col, const gs::ValueSource& v) {
  if (v.layer != col.layer) return false;
  return col.dir.empty() || v.file.compare(0, col.dir.size() + 1, col.dir + "/") == 0;
}

gl::Picture layers_picture(const ge::console::Console& con, const gs::ServerCfg& cfg,
                           const std::vector<Column>& cols, const std::vector<std::string>& names) {
  const int cell_w = 200, name_w = 210, row_h = 46, top = 132;
  const int kW = 48 + name_w + cell_w * static_cast<int>(cols.size());
  const int kH = top + row_h * static_cast<int>(names.size()) + 60;
  gu::Image img = gu::make_image(kW, kH, {0.07f, 0.09f, 0.13f, 1});
  gu::DrawList dl;
  dl.begin(kW, kH);
  const gu::Font& bold = gu::ui_font(gu::Weight::semibold);
  const gu::Font& body = gu::ui_font(gu::Weight::medium);
  const gu::Color ink{0.93f, 0.95f, 0.98f, 1}, dim{0.50f, 0.57f, 0.67f, 1};
  const gu::Color win{0.36f, 0.70f, 1.0f, 1}, lost{0.62f, 0.66f, 0.74f, 1};
  dl.text(bold, 30, 24, 46, "SERVER.CFG IN LAYERS", ink);
  dl.text(body, 18, 24, 74, "left to right, each layer over the one before: the blue value wins",
          dim);
  const float x0 = 24 + static_cast<float>(name_w);
  for (size_t c = 0; c < cols.size(); c++)
    dl.text(bold, 16, x0 + static_cast<float>(c) * cell_w + 8, top - 18, cols[c].head, dim);
  for (size_t r = 0; r < names.size(); r++) {
    const float y = static_cast<float>(top) + static_cast<float>(r) * row_h;
    if (r % 2 == 0)
      dl.rect({16, y, static_cast<float>(kW - 32), static_cast<float>(row_h)},
              {0.11f, 0.14f, 0.19f, 1}, 6);
    dl.text(bold, 18, 28, y + 29, names[r], ink);
    const auto it = cfg.history.find(names[r]);
    const ge::console::Cvar* v = con.cvars().find(names[r]);
    for (size_t c = 0; c < cols.size(); c++) {
      const float x = x0 + static_cast<float>(c) * cell_w + 8;
      std::string value;
      bool set_here = false, winner = false;
      if (cols[c].layer == gs::Layer::fallback) {
        value = v ? v->fallback : "";
        set_here = true;
        winner = it == cfg.history.end();
      } else if (it != cfg.history.end()) {
        for (size_t k = 0; k < it->second.size(); k++) {
          if (!in_column(cols[c], it->second[k])) continue;
          value = it->second[k].value;
          set_here = true;
          winner = k + 1 == it->second.size();
        }
      }
      if (!set_here) {
        dl.text(body, 17, x, y + 29, "-", dim);
        continue;
      }
      if (winner)
        dl.rect({x - 6, y + 6, static_cast<float>(cell_w - 8), static_cast<float>(row_h - 12)},
                {0.13f, 0.27f, 0.45f, 1}, 6);
      std::string shown = "\"" + value + "\"";
      if (shown.size() > 18) shown = shown.substr(0, 16) + "..";
      dl.text(body, 17, x, y + 29, shown, winner ? win : lost);
    }
  }
  gu::render(dl, img);
  gl::Picture pic;
  pic.width = img.width;
  pic.height = img.height;
  pic.rgba = std::move(img.rgba);
  return pic;
}

void run(gl::Lab& lab) {
  ge::console::Console con(true);
  con.set_files(
      [](std::string_view name, std::string& out) {
        const auto it = kFiles.find(std::string(name));
        if (it == kFiles.end()) return false;
        out = it->second;
        return true;
      },
      [](std::string_view, std::string_view) { return false; });
  gs::ServerConfig config;
  gs::ServerCfg cfg;
  std::string error;
  std::vector<std::string> args = {"--mod=" + lab.arg("mods"), "--name=" + lab.arg("name")};
  if (!lab.arg("profile").empty()) args.push_back("--cfg=" + lab.arg("profile"));
  if (!gs::server_configure(con, args, config, cfg, gs::sample_game(), error)) {
    lab.fail(error, "Check the flags against the fixture's files; skill: net.");
    return;
  }
  // The owner changes the message at the server's console once it runs.
  con.execute("sv_motd \"tonight: duels\"");

  gl::LabTable read{"the files read, in order", {"#", "file", "layer"}, {}};
  for (size_t i = 0; i < cfg.read.size(); i++) {
    const std::string& f = cfg.read[i];
    const std::string dir = f.substr(0, f.find('/'));
    const bool server_cfg = f.size() > 10 && f.compare(f.size() - 10, 10, "server.cfg") == 0;
    read.rows.push_back(
        {std::to_string(i + 1), f, !server_cfg ? "profile" : (dir == cfg.base ? "base" : "mod")});
  }
  lab.table(std::move(read));

  gl::LabTable ex{"--explain: each server value, where it was set, what it overrode",
                  {"cvar", "value", "set at", "layer", "overrode"},
                  {}};
  std::vector<std::string> names;
  int from_mods = 0, from_profile = 0, overridden = 0;
  for (const ge::console::Cvar* v : con.cvars().sorted()) {
    if ((v->flags & ge::console::kServer) == 0) continue;
    const auto it = cfg.history.find(v->name);
    if (it == cfg.history.end()) continue;
    names.push_back(v->name);
    const std::vector<gs::ValueSource>& h = it->second;
    const gs::ValueSource& last = h.back();
    std::string over = "-";
    for (size_t k = h.size() - 1; k-- > 0;) {
      if (gs::where_text(h[k]) == gs::where_text(last)) continue;
      over = gs::where_text(h[k]) + " \"" + h[k].value + "\"";
      overridden++;
      break;
    }
    from_mods += last.layer == gs::Layer::mod ? 1 : 0;
    from_profile += last.layer == gs::Layer::profile ? 1 : 0;
    ex.rows.push_back(
        {v->name, "\"" + v->value + "\"", gs::where_text(last), gs::layer_name(last.layer), over});
  }
  lab.table(std::move(ex));
  lab.metric("files_read", static_cast<double>(cfg.read.size()), "files");
  lab.metric("values_set", static_cast<double>(names.size()), "cvars");
  lab.metric("won_by_a_mod", from_mods, "cvars");
  lab.metric("won_by_the_profile", from_profile, "cvars");
  lab.metric("overrode_another", overridden, "cvars");
  const std::string hostname = gs::explain(con, cfg, "sv_hostname");
  lab.note("--explain=sv_hostname: " + hostname.substr(0, hostname.find('\n')));

  std::vector<Column> cols = {{"FALLBACK", gs::Layer::fallback, ""},
                              {cfg.base + "/", gs::Layer::base, ""}};
  for (const std::string& m : cfg.mods) cols.push_back({m + "/", gs::Layer::mod, m});
  if (!cfg.profile.empty()) cols.push_back({cfg.read.back(), gs::Layer::profile, ""});
  cols.push_back({"COMMAND LINE", gs::Layer::command_line, ""});
  cols.push_back({"CONSOLE", gs::Layer::run_time, ""});
  lab.shot({"layers",
            "SERVER.CFG IN LAYERS",
            "Every server cvar the fixture sets, across the layers: the fallback, the base game's "
            "server.cfg, each mod's (lowest first), the profile's winning copy, the command line "
            "and the server's console. A cell is the value that layer set; the blue one wins.",
            "a mod changes only what it sets; the higher mod over the lower; the profile and the "
            "command line over both; the console last.",
            {{"mods", lab.arg("mods")}, {"profile", lab.arg("profile")}}},
           layers_picture(con, cfg, cols, names));
}

const gl::LabSpec kSpec{
    "server-cfg",
    gl::LabKind::sim,
    "engine",
    "engine/server/config (server.cfg in layers, --explain)",
    "net",
    "Does a mod's server.cfg override only what it sets over the base's, a profile and the command "
    "line over both, and does every value name the file and line that set it?",
    {
        {"mods", "arena,ctf", "the mods over the base, lowest first (the fixture has arena, ctf)"},
        {"profile", "duel", "a profile (duel is in base and arena); empty: none"},
        {"name", "Friday", "--name on the command line"},
    },
    "--mods=arena,ctf",
    run,
};
GE_LAB(kSpec);

}  // namespace
