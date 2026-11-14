// Shooter's Edge's player CLI's Spec and its own commands (spec.hpp).
//
// ge:summary Shooter's Edge's player CLI: the Spec's functions and the demos commands
// ge:keywords shooters_edge_cli locate join_words listen_words demos demos-play demos-pack
// ge:keywords demos-unpack ged replay-v2
// ge:skill shooter agent-interface
// ge:test tests/gamecli_cli_test.cpp
#include "shooters_edge/cli/spec.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>

#include "capture/screenshot.hpp"
#include "classicenhance/cli/commands.hpp"
#include "classicenhance/profile.hpp"
#include "console/view.hpp"
#include "core/json_write.hpp"
#include "gamecli/process.hpp"
#include "replay/stream.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/coop_mode.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_convert.hpp"
#include "shooters_edge/client/demo_pack.hpp"
#include "shooters_edge/client/demo_radar.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/game_folder.hpp"
#include "shooters_edge/client/settings.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/net/classic_browse.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/view/hand.hpp"
#include "vfs/unpack.hpp"
#include "vfs/zstd.hpp"

namespace shooters_edge::cli {

namespace {

namespace fs = std::filesystem;
namespace gc = ge::gamecli;
namespace cl = shooters_edge::client;
using ge::core::json_arr;
using ge::core::json_num;
using ge::core::json_obj;
using ge::core::json_str;

// The window's choice of install and game folder, from the same flags and the same kept game.
cl::Args folders(const gc::Ctx& ctx) {
  cl::Args a;
  if (ctx.has("basedir")) {
    a.basedir = cl::expand_home(ctx.flag("basedir"));
    a.basedir_set = true;
  }
  if (ctx.has("game")) {
    a.game = ctx.flag("game");
    a.game_set = true;
  }
  cl::choose_folder(a, gc::prefs_dir("gameengine2", "shooters_edge"));
  std::error_code ec;
  fs::path abs = fs::absolute(a.basedir, ec).lexically_normal();
  if (!ec && abs.filename().empty()) abs = abs.parent_path();  // "dir/" read as "dir"
  if (!ec) a.basedir = abs.string();
  return a;
}

// Every player setting the libraries without a window register, as the window registers them
// (client/main.cpp): the console's (its look, the player's name), the controls, the user info, the
// co-op options, the HUD, the held weapons, the menus (the browser's sb_ settings among them) and
// the screenshots. The window's own (the picture, the window, the pacing) come from its catalog.
struct Registered {
  cl::ControlSettings controls;
  cl::UserInfo user_info;
  cl::CoopMode coop;
  shooters_edge::hud::Hud hud = shooters_edge::hud::make_hud();
  shooters_edge::view::ViewSettings view;
  shooters_edge::menu::GameMenu menu;
  ge::capture::ScreenshotSettings shots;
};

void add_cvars(ge::console::Console& con) {
  con.keep(std::make_shared<ge::console::ConsoleView>(con));  // con_ and the player's name
  auto r = std::make_shared<Registered>();
  cl::add_coop_cvars(con, r->coop);
  cl::add_cvars(con, r->controls);
  shooters_edge::view::add_view_cvars(con.cvars(), r->view);
  cl::add_user_info_cvars(con, r->user_info);
  shooters_edge::hud::add_cvars(con, r->hud);
  r->menu.add_cvars(con);
  ge::capture::add_screenshot_cvars(con.cvars(), r->shots);
  con.keep(r);
}

bool locate(const gc::Ctx& ctx, gc::Places& out, std::string& error) {
  const cl::Args a = folders(ctx);
  if (!fs::is_directory(a.basedir)) {
    error = "the install " + a.basedir + " is not a folder";
    return false;
  }
  out.install = a.basedir;
  out.settings = cl::settings_dir(a.basedir, a.game).string();
  out.masters = shooters_edge::net::classic_install_masters(a.basedir);
  return true;
}

void join_words(const gc::Ctx& ctx, const std::string& address, std::vector<std::string>& w) {
  const cl::Args a = folders(ctx);
  w.insert(w.end(), {"-basedir", a.basedir, "-game", a.game, "+connect", address});
}

// A hub's Spectate link: the window joins as a spectator (the `spectator` setting it sends).
void spectate_words(const gc::Ctx& ctx, const std::string& address, std::vector<std::string>& w) {
  const cl::Args a = folders(ctx);
  w.insert(w.end(),
           {"-basedir", a.basedir, "-game", a.game, "+set", "spectator", "1", "+connect", address});
}

// A hub's Play link: the replay file (version 2) the CLI fetched, unpacked beside it into the
// recording the demo player reads (client/demo_v2.hpp), then played.
bool demo_words(const gc::Ctx& ctx, const std::string& path, std::vector<std::string>& w,
                std::string& error) {
  std::vector<uint8_t> file;
  {
    std::ifstream f(path, std::ios::binary);
    file.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  }
  std::vector<uint8_t> recording;
  if (!cl::unpack_demo(file, recording, error)) {
    error = "the demo is not one this game plays: " + error;
    return false;
  }
  cl::Demo demo;
  if (!cl::open_demo(recording, demo, error)) {
    error = "the demo does not open: " + error;
    return false;
  }
  fs::path out = path;
  out.replace_extension(cl::plain_extension(demo.state.format));
  std::ofstream f(out, std::ios::binary);
  f.write(reinterpret_cast<const char*>(recording.data()),
          static_cast<std::streamsize>(recording.size()));
  if (!f) {
    error = "the demo cannot be written to " + out.string();
    return false;
  }
  const cl::Args a = folders(ctx);
  w.insert(w.end(), {"-basedir", a.basedir, "-game", a.game, "-playdemo", out.string()});
  return true;
}

void listen_words(const gc::Ctx& ctx, const gc::HostAsk& ask, std::vector<std::string>& w) {
  const cl::Args a = folders(ctx);
  // As HOST GAME's START does it: the server's lines (`sv <name> <value>`), then the map.
  w.insert(w.end(), {"-basedir", a.basedir, "-game", a.game, "+sv_port", std::to_string(ask.port),
                     "+sv", "deathmatch", "1"});
  if (ask.maxclients > 0) w.insert(w.end(), {"+sv", "maxclients", std::to_string(ask.maxclients)});
  const std::string map =
      ask.map.empty() ? std::string(ge::classicenhance::classic_profile().start_map) : ask.map;
  w.insert(w.end(), {"+map", map});
}

// The recordings in the game folder and its demos/ folder, by name.
std::vector<fs::path> demo_files(const cl::Args& a) {
  std::vector<fs::path> out;
  const fs::path game = fs::path(a.basedir) / a.game;
  for (const fs::path& dir : {game, game / "demos"}) {
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
      const std::string ext = e.path().extension().string();
      for (std::string_view known : profiles::kClassicDemos)
        if (ext == known) out.push_back(e.path());
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

int cmd_demos(gc::Ctx& ctx) {
  if (!gc::need_places(ctx)) return 1;
  const cl::Args a = folders(ctx);
  std::vector<std::string> rows;
  std::string text;
  for (const fs::path& f : demo_files(a)) {
    std::error_code ec;
    const auto bytes = fs::file_size(f, ec);
    rows.push_back(json_obj({{"name", json_str(f.stem().string())},
                             {"file", json_str(f.string())},
                             {"bytes", json_num(static_cast<long long>(bytes))}}));
    text += f.stem().string() + "  " + std::to_string(bytes / 1024) + " KiB  " + f.string() + "\n";
  }
  return gc::answer(ctx, json_obj({{"demos", json_arr(rows)}}), text,
                    std::to_string(rows.size()) + " recordings in " + a.basedir + "/" + a.game,
                    rows.empty() ? "record one in the game's console: record <name>"
                                 : "shooters_edge_cli demos play <name> watches one");
}

int cmd_demos_play(gc::Ctx& ctx) {
  if (ctx.words.size() != 1)
    return gc::refuse(ctx, 2, "demos play takes one recording's name",
                      "shooters_edge_cli demos (lists them)");
  if (!gc::need_places(ctx)) return 1;
  const cl::Args a = folders(ctx);
  fs::path file;
  for (const fs::path& f : demo_files(a))
    if (f.stem() == ctx.words[0] || f.filename() == ctx.words[0]) file = f;
  if (file.empty())
    return gc::refuse(ctx, 1, "no recording is called '" + ctx.words[0] + "'",
                      "shooters_edge_cli demos");
  const std::vector<std::string> argv = {
      gc::program_path(ctx.flag("bin", ctx.self_dir), "shooters_edge"),
      "-basedir",
      a.basedir,
      "-game",
      a.game,
      "-playdemo",
      file.string()};
  if (!fs::exists(argv[0]))
    return gc::refuse(ctx, 1, argv[0] + " is not there",
                      "build it, or pass --bin=<the folder holding shooters_edge>");
  const std::string log = (fs::path(ctx.places.logs) / "demo.log").string();
  std::error_code ec;
  fs::create_directories(ctx.places.logs, ec);
  long pid = 0;
  std::string error;
  if (!gc::spawn_detached(argv, "", log, pid, error))
    return gc::refuse(ctx, 1, error, "read " + log);
  return gc::answer(ctx, json_obj({{"file", json_str(file.string())}, {"pid", json_num(pid)}}),
                    "playing " + file.string(), "", "shooters_edge_cli logs demo says what it did");
}

// The recording a name or a file names: one of `demos`' list, else a path.
fs::path find_demo_file(const gc::Ctx& ctx, const std::string& word) {
  for (const fs::path& f : demo_files(folders(ctx)))
    if (f.stem() == word || f.filename() == word) return f;
  std::error_code ec;
  return fs::is_regular_file(word, ec) ? fs::path(word) : fs::path();
}

bool read_bytes(const fs::path& f, std::vector<uint8_t>& out) {
  std::ifstream in(f, std::ios::binary);
  if (!in) return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}

bool write_bytes(const fs::path& f, std::span<const uint8_t> bytes) {
  std::ofstream o(f, std::ios::binary);
  o.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return static_cast<bool>(o);
}

std::string kib(size_t bytes) { return std::to_string((bytes + 512) / 1024) + " KiB"; }

// `demos pack <name>`: the recording (unpacked first if it is packed) as a replay file version 2
// beside it, `<name>.ged`, read back to check it gives the same bytes.
int cmd_demos_pack(gc::Ctx& ctx) {
  if (ctx.words.size() != 1)
    return gc::refuse(ctx, 2, "demos pack takes one recording's name or file",
                      "shooters_edge_cli demos (lists them)");
  if (!gc::need_places(ctx)) return 1;
  const fs::path file = find_demo_file(ctx, ctx.words[0]);
  if (file.empty())
    return gc::refuse(ctx, 1, "no recording is called '" + ctx.words[0] + "'",
                      "shooters_edge_cli demos");
  std::vector<uint8_t> bytes;
  if (!read_bytes(file, bytes))
    return gc::refuse(ctx, 1, "cannot read " + file.string(), "check the file's permissions");
  const size_t on_disk = bytes.size();
  if (ge::vfs::wrapping_of(bytes) != ge::vfs::Wrapping::none) {
    ge::vfs::Unpacked u;
    std::string error;
    if (!ge::vfs::unpack_one(std::move(bytes), u, error))
      return gc::refuse(ctx, 1, file.string() + " does not unpack: " + error,
                        "shooters_edge_cli demos (pick another)");
    bytes = std::move(u.bytes);
  }
  if (ge::replay::is_stream(bytes))
    return gc::refuse(ctx, 1, file.string() + " is already a replay file",
                      "shooters_edge_cli demos unpack " + ctx.words[0]);
  const fs::path out = fs::path(file).replace_extension(".ged");
  if (fs::exists(out))
    return gc::refuse(ctx, 1, out.string() + " is already there",
                      "delete or rename it, then pack again");
  std::vector<uint8_t> packed, back;
  std::string error;
  client::DemoPackReport rep;
  if (!client::pack_demo(bytes, packed, error, {}, &rep))
    return gc::refuse(ctx, 1, "the recording does not pack: " + error, "report it with the file");
  if (!client::unpack_demo(packed, back, error) || back != bytes)
    return gc::refuse(ctx, 1, "the packed file does not give back the recording's bytes",
                      "report it with the file; nothing was written");
  // A multiview recording also carries its radar, so a hub can draw the match as a map.
  bool radar = false;
  if (rep.format == "multiview") {
    std::vector<uint8_t> with;
    if (client::add_radar(bytes, packed, with, error)) {
      packed = std::move(with);
      radar = true;
    }
  }
  if (!write_bytes(out, packed))
    return gc::refuse(ctx, 1, "cannot write " + out.string(), "check the folder's permissions");
  const double share =
      on_disk > 0 ? static_cast<double>(packed.size()) / static_cast<double>(on_disk) : 0;
  return gc::answer(
      ctx,
      json_obj({{"file", json_str(out.string())},
                {"from", json_str(file.string())},
                {"format", json_str(rep.format)},
                {"plain_bytes", json_num(static_cast<long long>(bytes.size()))},
                {"from_bytes", json_num(static_cast<long long>(on_disk))},
                {"bytes", json_num(static_cast<long long>(packed.size()))},
                {"blocks", json_num(static_cast<long long>(rep.blocks))},
                {"keyframes", json_num(static_cast<long long>(rep.keyframes))},
                {"radar", ge::core::json_bool(radar)}}),
      out.string() + "  " + kib(packed.size()) + " (" + file.filename().string() + " " +
          kib(on_disk) + ", plain " + kib(bytes.size()) + ")\n",
      "packed " + std::to_string(std::lround(share * 100)) + "% of " + file.filename().string(),
      "shooters_edge_cli demos play " + out.filename().string());
}

// `demos convert <name> <file>`: the recording converted to the format `<file>`'s name asks for
// (client/demo_convert.hpp), written there (beside the recording when it names no folder); a
// packed name (`.mvdz`) is packed as the recorders pack. What the target could not hold is listed.
int cmd_demos_convert(gc::Ctx& ctx) {
  if (ctx.words.size() != 2)
    return gc::refuse(ctx, 2, "demos convert takes a recording and the file to write",
                      "shooters_edge_cli demos convert demo1.dem demo1.mvd");
  if (!gc::need_places(ctx)) return 1;
  const fs::path file = find_demo_file(ctx, ctx.words[0]);
  if (file.empty())
    return gc::refuse(ctx, 1, "no recording is called '" + ctx.words[0] + "'",
                      "shooters_edge_cli demos");
  fs::path out = ctx.words[1];
  if (!out.has_parent_path()) out = file.parent_path() / out;
  client::DemoFormat to{};
  if (!client::format_of_name(out.string(), to))
    return gc::refuse(ctx, 2, out.filename().string() + " names no recording format",
                      "end the name in .dem, " +
                          client::plain_extension(client::DemoFormat::client) +
                          ", .mvd or .dm2 (a z after it packs it)");
  if (fs::exists(out))
    return gc::refuse(ctx, 1, out.string() + " is already there",
                      "delete or rename it, then convert again");
  std::vector<uint8_t> bytes;
  if (!read_bytes(file, bytes))
    return gc::refuse(ctx, 1, "cannot read " + file.string(), "check the file's permissions");
  std::vector<uint8_t> converted;
  client::ConvertNotes notes;
  std::string error;
  if (!client::convert_demo(std::move(bytes), to, {}, converted, notes, error))
    return gc::refuse(ctx, 1, file.filename().string() + " does not convert: " + error,
                      "shooters_edge_cli demos convert <a .dem> <name>.mvd");
  const std::string ext = out.extension().string();
  const bool packed = ext.size() == 5 && (ext.back() == 'z' || ext.back() == 'Z');
  if (packed) converted = ge::vfs::zstd_pack(converted);
  if (!write_bytes(out, converted))
    return gc::refuse(ctx, 1, "cannot write " + out.string(), "check the folder's permissions");
  std::vector<std::string> dropped, rewritten;
  std::string text = out.string() + "  " + kib(converted.size()) + ", " +
                     std::to_string(notes.frames) + " frames\n";
  for (const auto& [what, n] : notes.dropped) {
    dropped.push_back(
        json_obj({{"what", json_str(what)}, {"count", json_num(static_cast<long long>(n))}}));
    text += "  dropped " + std::to_string(n) + "x " + what + "\n";
  }
  for (const auto& [what, n] : notes.rewritten) {
    rewritten.push_back(
        json_obj({{"what", json_str(what)}, {"count", json_num(static_cast<long long>(n))}}));
    text += "  rewritten " + std::to_string(n) + "x " + what + "\n";
  }
  return gc::answer(ctx,
                    json_obj({{"file", json_str(out.string())},
                              {"from", json_str(file.string())},
                              {"format", json_str(client::format_words(to))},
                              {"bytes", json_num(static_cast<long long>(converted.size()))},
                              {"frames", json_num(static_cast<long long>(notes.frames))},
                              {"dropped", json_arr(dropped)},
                              {"rewritten", json_arr(rewritten)}}),
                    text, "converted to a " + client::format_words(to),
                    "shooters_edge_cli demos play " + out.filename().string());
}

// `demos unpack <name>`: a replay file version 2 back to the recording's own bytes beside it.
int cmd_demos_unpack(gc::Ctx& ctx) {
  if (ctx.words.size() != 1)
    return gc::refuse(ctx, 2, "demos unpack takes one replay file's name or file",
                      "shooters_edge_cli demos (lists them)");
  if (!gc::need_places(ctx)) return 1;
  fs::path file = find_demo_file(ctx, ctx.words[0] + ".ged");
  if (file.empty()) file = find_demo_file(ctx, ctx.words[0]);
  std::vector<uint8_t> bytes;
  if (file.empty() || !read_bytes(file, bytes) || !ge::replay::is_stream(bytes))
    return gc::refuse(ctx, 1, "no replay file is called '" + ctx.words[0] + "'",
                      "shooters_edge_cli demos");
  std::vector<uint8_t> plain;
  std::string cut;
  if (!client::unpack_demo(bytes, plain, cut))
    return gc::refuse(ctx, 1, file.string() + " does not open: " + cut, "pack the recording again");
  const char* ext = client::is_multiview(plain) ? ".mvd" : ".dem";
  const fs::path out = fs::path(file).replace_extension(ext);
  if (fs::exists(out))
    return gc::refuse(ctx, 1, out.string() + " is already there",
                      "delete or rename it, then unpack again");
  if (!write_bytes(out, plain))
    return gc::refuse(ctx, 1, "cannot write " + out.string(), "check the folder's permissions");
  return gc::answer(
      ctx,
      json_obj({{"file", json_str(out.string())},
                {"bytes", json_num(static_cast<long long>(plain.size()))},
                {"cut", json_str(cut)}}),
      out.string() + "  " + kib(plain.size()) + "\n",
      cut.empty() ? "unpacked " + file.filename().string() : "unpacked what was whole: " + cut,
      "shooters_edge_cli demos play " + out.filename().string());
}

gc::Spec make() {
  gc::Spec s;
  s.game = "shooters_edge";
  s.title = "Shooter's Edge";
  s.cli = "shooters_edge_cli";
  s.window = "shooters_edge";
  s.server = "shooters_edge_server";
  s.prefs_app = "shooters_edge";
  s.default_port = 27500;
  s.lan_first = shooters_edge::net::kLanFirstPort;
  s.lan_last = shooters_edge::net::kLanLastPort;
  s.master_port = 27000;
  s.query = shooters_edge::net::classic_query();
  s.routes = shooters_edge::net::classic_route_query();
  s.server_game = &shooters_edge::net::classic_game();
  s.rcon = shooters_edge::net::classic_rcon;
  s.read_rcon = shooters_edge::net::classic_read_print;
  s.add_cvars = add_cvars;
  s.locate = locate;
  s.join_words = join_words;
  s.listen_words = listen_words;
  s.spectate_words = spectate_words;
  s.demo_words = demo_words;
  s.commands = ge::classicenhance::cli::commands();
  s.commands.push_back({"demos", "", "the recordings in the game folder: name, size, file",
                        "demos --json", "shooters_edge", cmd_demos});
  s.commands.push_back({"demos play", "<name>", "watch a recording in the window",
                        "demos play final", "shooters_edge", cmd_demos_play});
  s.commands.push_back(
      {"demos pack", "<name>",
       "a recording packed as a replay file (.ged) beside it, checked to come back",
       "demos pack final", "shooters_edge", cmd_demos_pack});
  s.commands.push_back({"demos convert", "<name> <file>",
                        "a recording converted to the format the file's name asks for",
                        "demos convert demo1.dem demo1.mvd", "shooters_edge", cmd_demos_convert});
  s.commands.push_back({"demos unpack", "<name>", "a replay file (.ged) back to the recording",
                        "demos unpack final.ged", "shooters_edge", cmd_demos_unpack});
  s.flags = {{"basedir", "<dir>", "the classic game's install (default: the one loaded last)"},
             {"game", "<folder>", "the game folder over it (default: the multiplayer one)"}};
  s.notes =
      "Shooter's Edge plays the player's own classic packs: `--basedir` is their install (the "
      "game remembers the last one loaded, so it is rarely needed), and `content` says which game "
      "and release the packs are. Servers speak the classic protocol, so `servers` lists this "
      "game's servers and the classic ones already on the internet alike; a classic server's "
      "address is usually `host:27500`. The player's name is the setting `name`, the colours "
      "`topcolor` and `bottomcolor`; `cl_speedjump 1` asks a server for the easy jump. "
      "`demos` lists recordings and `demos play <name>` watches one; `demos pack <name>` writes "
      "the replay file (.ged), about half the size of a packed recording; `demos convert <name> "
      "<file>` "
      "writes it in another format (`demo1.dem` to `demo1.mvd`).";
  return s;
}

}  // namespace

const gc::Spec& spec() {
  static const gc::Spec s = make();
  return s;
}

}  // namespace shooters_edge::cli
