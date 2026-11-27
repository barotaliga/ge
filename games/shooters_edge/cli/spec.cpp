// Shooter's Edge's player CLI's Spec and its own commands (spec.hpp).
//
// ge:summary Shooter's Edge's player CLI: the Spec's functions and the demos commands
// ge:keywords secli locate join_words listen_words demos demos-play demos-pack
// ge:keywords demos-unpack ged replay-v2
// ge:skill shooter agent-interface
// ge:test tests/gamecli_cli_test.cpp
#include "shooters_edge/cli/spec.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>

#include "brush/cli/commands.hpp"
#include "capture/screenshot.hpp"
#include "classicenhance/cli/commands.hpp"
#include "classicenhance/profile.hpp"
#include "console/view.hpp"
#include "core/json_write.hpp"
#include "progs/cli/commands.hpp"
#include "replay/stream.hpp"
#include "server/process.hpp"
#include "shooters_edge/cli/mod_run.hpp"
#include "shooters_edge/client/controls.hpp"
#include "shooters_edge/client/coop_mode.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_convert.hpp"
#include "shooters_edge/client/demo_pack.hpp"
#include "shooters_edge/client/demo_radar.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/game_folder.hpp"
#include "shooters_edge/client/install_search.hpp"
#include "shooters_edge/client/settings.hpp"
#include "shooters_edge/client/take_in.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "shooters_edge/menu/game_menu.hpp"
#include "shooters_edge/net/qw_browse.hpp"
#include "shooters_edge/net/qw_game.hpp"
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

// The flags named for the window's settings: fs_basegame, fs_artifacts_basedir and each
// fs_basedir_<id>.
std::vector<std::string> setting_flags() {
  std::vector<std::string> out = {std::string(cl::kBaseGame), std::string(cl::kArtifactsBasedir)};
  for (const auto& [id, dir] : cl::BaseGame().dirs) out.push_back(cl::basedir_setting(id));
  return out;
}

// The window's data folder (client/take_in.hpp).
std::string data_folder(const gc::Ctx& ctx) {
  return cl::data_root(cl::placed_folder(ctx.self_dir),
                       gc::prefs_dir("gameengine2", "shooters_edge"));
}

// The series config's files the window reads, the program's own last (client/settings.hpp).
std::vector<fs::path> series_files(const gc::Ctx& ctx) {
  return cl::series_settings_files(data_folder(ctx), cl::placed_folder(ctx.self_dir));
}

// The one of them the window writes its settings to.
fs::path series_file(const gc::Ctx& ctx) {
  const std::vector<fs::path> files = series_files(ctx);
  return files.empty() ? fs::path() : files.back();
}

// The window's game and install (client/installs.hpp): its settings as the series config keeps
// them, each flag named for a setting (`--fs_basegame=q2`, `--fs_basedir_q2=<dir>`) over them.
cl::BaseGame base_game(const gc::Ctx& ctx) {
  std::vector<std::string> plus;
  for (const std::string& name : setting_flags())
    if (ctx.has(name)) plus.insert(plus.end(), {"+set", name, ctx.flag(name)});
  cl::BaseGame base = cl::read_base_game(series_files(ctx), plus);
  base.from = cl::placed_folder(ctx.self_dir);  // a relative install: from the programs' folder
  return base;
}

// The window's choice of install and game folder, as it chooses them (`--game` its `-game`). Where
// the window would ask for the install (the folder holds no game it can play), the CLI still reads
// the folder the setting names: its settings, recordings and server lists need no packs.
cl::Args folders(const gc::Ctx& ctx) {
  cl::Args a;
  if (ctx.has("game")) {
    a.game = ctx.flag("game");
    a.game_set = true;
  }
  const cl::BaseGame base = base_game(ctx);
  cl::choose_folder(a, base, cl::own_game_folder(data_folder(ctx)));
  if (const std::string named = base.dir(base.game); a.ask_install && !named.empty()) {
    a.basedir = named;
    if (!a.game_set) a.game = cl::game_of(named, *ge::classicenhance::find_profile(base.game));
  }
  std::error_code ec;
  fs::path abs = fs::absolute(a.basedir, ec).lexically_normal();
  if (!ec && abs.filename().empty()) abs = abs.parent_path();  // "dir/" read as "dir"
  if (!ec) a.basedir = abs.string();
  return a;
}

// The window's words for the same game, install and game folder.
std::vector<std::string> game_words(const gc::Ctx& ctx) {
  const cl::Args a = folders(ctx);
  const cl::BaseGame base = base_game(ctx);
  std::vector<std::string> w = {"-game", a.game, "+set", std::string(cl::kBaseGame), base.game};
  if (!base.game.empty()) w.insert(w.end(), {"+set", cl::basedir_setting(base.game), a.basedir});
  return w;
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
  out.series = series_file(ctx).string();
  out.masters = shooters_edge::net::qw_install_masters(a.basedir);
  return true;
}

void join_words(const gc::Ctx& ctx, const std::string& address, std::vector<std::string>& w) {
  w = game_words(ctx);
  w.insert(w.end(), {"+connect", address});
}

// A hub's Spectate link: the window joins as a spectator (the `spectator` setting it sends).
void spectate_words(const gc::Ctx& ctx, const std::string& address, std::vector<std::string>& w) {
  w = game_words(ctx);
  w.insert(w.end(), {"+set", "spectator", "1", "+connect", address});
}

// A hub's Watch link ("Open in the game"): the window plays the TV node's live broadcast
// (client/tv_live.hpp); a node carries one stream, so `stream` names nothing more.
void tv_words(const gc::Ctx& ctx, const std::string& node, uint32_t /*stream*/,
              std::vector<std::string>& w) {
  w = game_words(ctx);
  w.insert(w.end(), {"-tv", node});
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
  w = game_words(ctx);
  w.insert(w.end(), {"-playdemo", out.string()});
  return true;
}

void listen_words(const gc::Ctx& ctx, const gc::HostAsk& ask, std::vector<std::string>& w) {
  // As HOST GAME's START does it: the server's lines (`sv <name> <value>`), then the map.
  w = game_words(ctx);
  w.insert(w.end(), {"+sv_port", std::to_string(ask.port), "+sv", "deathmatch", "1"});
  if (ask.maxclients > 0) w.insert(w.end(), {"+sv", "maxclients", std::to_string(ask.maxclients)});
  const std::string map =
      ask.map.empty() ? std::string(ge::classicenhance::q1_profile().start_map) : ask.map;
  w.insert(w.end(), {"+map", map});
}

// The recordings in the game folder, its demos/ folder and the player's own folder
// (client/installs.hpp's `own_files`: the install's with fs_artifacts_basedir 1), by name.
std::vector<fs::path> demo_files(const gc::Ctx& ctx, const cl::Args& a) {
  std::vector<fs::path> out;
  const fs::path game = fs::path(a.basedir) / a.game;
  std::vector<fs::path> dirs = {game, game / "demos"};
  if (const fs::path own = base_game(ctx).own_files(a.profile, a.basedir, a.game); own != game)
    dirs.push_back(own);
  for (const fs::path& dir : dirs) {
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
      const std::string ext = e.path().extension().string();
      for (std::string_view known : profiles::kQ1Demos)
        if (ext == known) out.push_back(e.path());
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

// `import <source>`: a game, its mission packs, or the packs alone taken into the data folder
// (classic/content/import.hpp), as the window takes them in.
int cmd_import(gc::Ctx& ctx) {
  if (ctx.words.size() > 1)
    return gc::refuse(ctx, 2, "import takes one folder, disc image or release archive",
                      "secli import <folder, .iso, .cue, .mdf, .zip or .7z>");
  const auto data = ctx.flags.find("data");
  const std::string root = data != ctx.flags.end() ? data->second : data_folder(ctx);
  if (ctx.words.empty()) {  // the packs on this machine not yet taken in, as the window offers
    const auto from = ctx.flags.find("from");
    const std::vector<std::string> roots = from != ctx.flags.end()
                                               ? std::vector<std::string>{from->second}
                                               : ge::classic::content::store_folders();
    std::vector<std::string> rows;
    std::string text;
    for (const ge::classic::content::Offer& o : ge::classic::content::find_offers(roots, root)) {
      rows.push_back(json_obj({{"game", json_str(o.game->id)},
                               {"folder", json_str(o.pack->folder)},
                               {"name", json_str(o.pack->name)},
                               {"source", json_str(o.source)}}));
      text += cl::offer_words(o) + "\n";
    }
    return gc::answer(ctx, json_obj({{"offers", json_arr(rows)}}), text,
                      std::to_string(rows.size()) + " mission packs found and not yet taken in",
                      rows.empty() ? "" : "secli import <source>");
  }
  ge::classic::content::ImportReport r;
  std::string why;
  if (!ge::classic::content::import_game(ctx.words[0], root, &r, &why))
    return gc::refuse(ctx, 1, why, "secli import <your game's folder, disc image or archive>");
  std::vector<std::string> packs;
  packs.reserve(r.packs.size());
  for (const auto* m : r.packs)
    packs.push_back(json_obj({{"folder", json_str(m->folder)}, {"name", json_str(m->name)}}));
  std::vector<std::string> notes;
  notes.reserve(r.notes.size());
  for (const std::string& n : r.notes) notes.push_back(json_str(n));
  const std::string words = cl::take_in_words(r, ctx.words[0]);
  std::string notes_text;  // what was found and left, a line each
  for (const std::string& n : r.notes) notes_text += n + "\n";
  return gc::answer(
      ctx,
      json_obj({{"game", json_str(r.game->id)},
                {"base", ge::core::json_bool(r.base)},
                {"packs", json_arr(packs)},
                {"game_dir", json_str(r.game_dir)},
                {"copied", json_num(static_cast<long long>(r.copied.size()))},
                {"same", json_num(static_cast<long long>(r.same.size()))},
                {"tracks", json_num(static_cast<long long>(r.tracks.size()))},
                {"unchanged", ge::core::json_bool(r.unchanged)},
                {"notes", json_arr(notes)}}),
      notes_text, words,
      "se +set fs_basegame " + std::string(r.game->id) + " +set " +
          cl::basedir_setting(r.game->id) + " " + r.game_dir +
          (r.packs.empty() ? std::string() : " -game " + std::string(r.packs[0]->folder)));
}

int cmd_demos(gc::Ctx& ctx) {
  if (!gc::need_places(ctx)) return 1;
  const cl::Args a = folders(ctx);
  std::vector<std::string> rows;
  std::string text;
  for (const fs::path& f : demo_files(ctx, a)) {
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
                                 : "secli demos play <name> watches one");
}

int cmd_demos_play(gc::Ctx& ctx) {
  if (ctx.words.size() != 1)
    return gc::refuse(ctx, 2, "demos play takes one recording's name", "secli demos (lists them)");
  if (!gc::need_places(ctx)) return 1;
  const cl::Args a = folders(ctx);
  fs::path file;
  for (const fs::path& f : demo_files(ctx, a))
    if (f.stem() == ctx.words[0] || f.filename() == ctx.words[0]) file = f;
  if (file.empty())
    return gc::refuse(ctx, 1, "no recording is called '" + ctx.words[0] + "'", "secli demos");
  std::vector<std::string> argv = {ge::server::program_path(ctx.flag("bin", ctx.self_dir), "se")};
  for (std::string& word : game_words(ctx)) argv.push_back(std::move(word));
  argv.insert(argv.end(), {"-playdemo", file.string()});
  if (!fs::exists(argv[0]))
    return gc::refuse(ctx, 1, argv[0] + " is not there",
                      "build it, or pass --bin=<the folder holding se>");
  const std::string log = (fs::path(ctx.places.logs) / "demo.log").string();
  std::error_code ec;
  fs::create_directories(ctx.places.logs, ec);
  long pid = 0;
  std::string error;
  if (!ge::server::spawn_detached(argv, "", log, pid, error))
    return gc::refuse(ctx, 1, error, "read " + log);
  return gc::answer(ctx, json_obj({{"file", json_str(file.string())}, {"pid", json_num(pid)}}),
                    "playing " + file.string(), "", "secli logs demo says what it did");
}

// The recording a name or a file names: one of `demos`' list, else a path.
fs::path find_demo_file(const gc::Ctx& ctx, const std::string& word) {
  for (const fs::path& f : demo_files(ctx, folders(ctx)))
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
                      "secli demos (lists them)");
  if (!gc::need_places(ctx)) return 1;
  const fs::path file = find_demo_file(ctx, ctx.words[0]);
  if (file.empty())
    return gc::refuse(ctx, 1, "no recording is called '" + ctx.words[0] + "'", "secli demos");
  std::vector<uint8_t> bytes;
  if (!read_bytes(file, bytes))
    return gc::refuse(ctx, 1, "cannot read " + file.string(), "check the file's permissions");
  const size_t on_disk = bytes.size();
  if (ge::vfs::wrapping_of(bytes) != ge::vfs::Wrapping::none) {
    ge::vfs::Unpacked u;
    std::string error;
    if (!ge::vfs::unpack_one(std::move(bytes), u, error))
      return gc::refuse(ctx, 1, file.string() + " does not unpack: " + error,
                        "secli demos (pick another)");
    bytes = std::move(u.bytes);
  }
  if (ge::replay::is_stream(bytes))
    return gc::refuse(ctx, 1, file.string() + " is already a replay file",
                      "secli demos unpack " + ctx.words[0]);
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
      "secli demos play " + out.filename().string());
}

// `demos convert <name> <file>`: the recording converted to the format `<file>`'s name asks for
// (client/demo_convert.hpp), written there (beside the recording when it names no folder); a
// packed name (`.mvdz`) is packed as the recorders pack. What the target could not hold is listed.
int cmd_demos_convert(gc::Ctx& ctx) {
  if (ctx.words.size() != 2)
    return gc::refuse(ctx, 2, "demos convert takes a recording and the file to write",
                      "secli demos convert demo1.dem demo1.mvd");
  if (!gc::need_places(ctx)) return 1;
  const fs::path file = find_demo_file(ctx, ctx.words[0]);
  if (file.empty())
    return gc::refuse(ctx, 1, "no recording is called '" + ctx.words[0] + "'", "secli demos");
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
                      "secli demos convert <a .dem> <name>.mvd");
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
                    "secli demos play " + out.filename().string());
}

// `demos unpack <name>`: a replay file version 2 back to the recording's own bytes beside it.
int cmd_demos_unpack(gc::Ctx& ctx) {
  if (ctx.words.size() != 1)
    return gc::refuse(ctx, 2, "demos unpack takes one replay file's name or file",
                      "secli demos (lists them)");
  if (!gc::need_places(ctx)) return 1;
  fs::path file = find_demo_file(ctx, ctx.words[0] + ".ged");
  if (file.empty()) file = find_demo_file(ctx, ctx.words[0]);
  std::vector<uint8_t> bytes;
  if (file.empty() || !read_bytes(file, bytes) || !ge::replay::is_stream(bytes))
    return gc::refuse(ctx, 1, "no replay file is called '" + ctx.words[0] + "'", "secli demos");
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
      "secli demos play " + out.filename().string());
}

gc::Spec make() {
  gc::Spec s;
  s.game = "shooters_edge";
  s.title = "Shooter's Edge";
  s.cli = "secli";
  s.window = "se";
  s.server = "sesv";
  s.prefs_app = "shooters_edge";
  s.default_port = 27500;
  s.lan_first = shooters_edge::net::kLanFirstPort;
  s.lan_last = shooters_edge::net::kLanLastPort;
  s.master_port = 27000;
  s.query = shooters_edge::net::qw_query();
  s.routes = shooters_edge::net::qw_route_query();
  s.server_game = &shooters_edge::net::qw_game();
  s.rcon = shooters_edge::net::qw_rcon;
  s.read_rcon = shooters_edge::net::qw_read_print;
  s.add_cvars = add_cvars;
  s.locate = locate;
  s.join_words = join_words;
  s.listen_words = listen_words;
  s.spectate_words = spectate_words;
  s.tv_words = tv_words;
  s.demo_words = demo_words;
  s.commands = ge::classicenhance::cli::commands();
  for (const ge::gamecli::Command& c : ge::brush::cli::commands()) s.commands.push_back(c);
  s.command_flags = ge::brush::cli::command_flags();
  // The toolkit's qc and map rows, their tools run in this process, and its mod rows: the game
  // loads C++ mod packages.
  s.offers = ge::progs::cli::tools();
  for (const std::string& t : ge::brush::cli::tools()) s.offers.push_back(t);
  s.offers.emplace_back("mod");
  s.tool = [](std::string_view program, std::string_view name,
              const std::vector<std::string>& args) {
    return name == "brush" ? ge::brush::cli::run_tool(program, name, args)
                           : ge::progs::cli::run_tool(program, name, args);
  };
  s.commands.push_back({"import", "[source]",
                        "a game, its mission packs or a pack alone copied into the data folder "
                        "from a folder, disc image (.iso, .cue, .mdf), .zip or .7z; none: the "
                        "packs found on this machine",
                        "import ~/Downloads/reckoning.cue", "shooters_edge", cmd_import});
  s.command_flags.push_back(
      {"import",
       {{"data", "<dir>", "the data folder to copy into (default: the game's own)"},
        {"from", "<dir>",
         "with no source: the folder to look for packs in (default: the stores' "
         "libraries and the mounted volumes)"}}});
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
  s.commands.push_back(mod_run_command());
  s.command_flags.push_back(mod_run_flags());
  s.flags = {{"fs_basegame", "q1|q2", "the game: Quake or Quake II (default: the window's)"},
             {"fs_basedir_q1", "<dir>", "Quake's install (default: the window's)"},
             {"fs_basedir_q2", "<dir>", "Quake II's install (default: the window's)"},
             {"fs_artifacts_basedir", "0|1",
              "1: the player's saves and recordings in the install (default: the window's)"},
             {"game", "<folder>", "the game folder over it (default: the multiplayer one)"}};
  s.notes =
      "Shooter's Edge plays the player's own classic packs: `--fs_basedir_q1` and "
      "`--fs_basedir_q2` are their installs and `--fs_basegame` the game (the window's settings "
      "by default, so they are rarely needed), and `content` says which game "
      "and release the packs are. Servers speak the classic protocol, so `servers` lists this "
      "game's servers and the classic ones already on the internet alike; a classic server's "
      "address is usually `host:27500`. The player's name is the setting `name`, the colours "
      "`topcolor` and `bottomcolor`; `cl_speedjump 1` asks a server for the easy jump. "
      "`demos` lists recordings and `demos play <name>` watches one; `demos pack <name>` writes "
      "the replay file (.ged), about half the size of a packed recording; `demos convert <name> "
      "<file>` "
      "writes it in another format (`demo1.dem` to `demo1.mvd`). `map build <plan|svg>` makes a "
      "map from a text plan or an SVG floor plan with the install's own textures; put the .bsp in "
      "the game folder's `maps` and start it with `map <name>`. `mod run <package>` plays a C++ "
      "mod "
      "package in the game's mod scenario twice and says whether it replays. `qc build "
      "<progs.src>` compiles a community mod's QC sources, and `qc disasm`, `qc run`, `qc trace` "
      "and `qc profile` look inside a progs; `pak ls` and `pak extract` read the packs. `mod "
      "check` "
      "loads a C++ mod package as the game would, and `mod pack`, `mod key` and `mod sign` ready "
      "one to share.";
  return s;
}

}  // namespace

const gc::Spec& spec() {
  static const gc::Spec s = make();
  return s;
}

}  // namespace shooters_edge::cli
