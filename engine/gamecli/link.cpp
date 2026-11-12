// The game CLI's hub links (docs/hub.md § Open in the game, engine/hub/link.hpp): `link open`
// takes a link a hub's page hands the system (Join, Spectate, Watch, Play) and starts the window
// on it, fetching a demo into `<state>/demos/<sha256>.ger` first (kept: a second Play starts at
// once); `link register` makes the game's scheme open in this CLI, per system.
//
// ge:summary The game CLI's link commands: a hub link opened in the window, the scheme registered
// ge:keywords gamecli link open register hub join spectate watch play demo fetch scheme
// ge:skill agent-interface
// ge:test tests/gamecli_link_test.cpp
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "core/json_write.hpp"
#include "gamecli/commands.hpp"
#include "gamecli/process.hpp"
#include "hub/link.hpp"
#include "mod/sha256.hpp"

namespace ge::gamecli {

namespace fs = std::filesystem;
using ge::core::json_obj;
using ge::core::json_str;
using ge::core::json_strs;

namespace {

bool read_file(const fs::path& p, std::vector<uint8_t>& out) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}

std::string home_dir() {
#ifdef _WIN32
  const char* h = std::getenv("USERPROFILE");
#else
  const char* h = std::getenv("HOME");
#endif
  return h ? h : "";
}

}  // namespace

int cmd_link_open(Ctx& ctx) {
  const std::string usage =
      std::string(ctx.spec.cli) + " link open " + ctx.spec.game + "://join/192.168.1.20:27500";
  if (ctx.words.size() != 1) return refuse(ctx, 2, "link open takes one link", usage);
  hub::HubLink link;
  std::string error;
  if (!hub::parse_link(ctx.words[0], ctx.spec.game, link, error)) {
    return refuse(ctx, 2, error, usage);
  }
  if (ctx.spec.window[0] == '\0') {
    return refuse(ctx, 1, std::string(ctx.spec.title) + " has no window to open a link in",
                  std::string(ctx.spec.cli) + " help");
  }
  if (!need_places(ctx)) return 1;
  std::vector<std::string> argv = {window_path(ctx)};
  switch (link.kind) {
    case hub::LinkKind::join:
      if (ctx.spec.join_words == nullptr) break;
      ctx.spec.join_words(ctx, link.address, argv);
      return start_window(ctx, argv, "join", "on " + link.address, link.address);
    case hub::LinkKind::spectate:
      if (ctx.spec.spectate_words == nullptr) break;
      ctx.spec.spectate_words(ctx, link.address, argv);
      return start_window(ctx, argv, "join", "watching " + link.address, link.address);
    case hub::LinkKind::tv:
      if (ctx.spec.tv_words == nullptr) break;
      ctx.spec.tv_words(ctx, link.address, link.stream, argv);
      return start_window(ctx, argv, "join", "watching the broadcast on " + link.address,
                          link.address);
    case hub::LinkKind::demo: {
      if (ctx.spec.demo_words == nullptr) break;
      const fs::path file = fs::path(ctx.places.state) / "demos" / (link.sha + ".ger");
      std::vector<uint8_t> bytes;
      const bool kept =
          read_file(file, bytes) && mod::sha256_hex(bytes.data(), bytes.size()) == link.sha;
      if (!kept && ctx.has("dry-run")) {
        return answer(ctx,
                      json_obj({{"sha256", json_str(link.sha)},
                                {"hub", json_str(link.hub)},
                                {"file", json_str(file.string())}}),
                      "fetch " + link.sha + " from " + link.hub + " into " + file.string() +
                          ", then start the game on it",
                      "would fetch the demo and play it (--dry-run: nothing fetched)");
      }
      if (!kept) {
        if (!hub::fetch_demo(link.hub, link.sha, bytes, error)) {
          return refuse(ctx, 1, error, "open the hub's page again, or try later");
        }
        std::error_code ec;
        fs::create_directories(file.parent_path(), ec);
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        if (!out) {
          return refuse(ctx, 1, "the demo cannot be kept at " + file.string(),
                        "check the disk, or pass --state=<a writable folder>");
        }
      }
      if (!ctx.spec.demo_words(ctx, file.string(), argv, error)) {
        return refuse(ctx, 1, error, "download it from the hub's page and open it in the game");
      }
      return start_window(ctx, argv, "demo", kept ? "on the demo" : "on the demo just fetched",
                          link.hub);
    }
  }
  const char* asked[] = {"joining a server", "spectating", "watching broadcasts", "playing demos"};
  return refuse(ctx, 1,
                std::string(ctx.spec.title) + " cannot open links for " +
                    asked[static_cast<int>(link.kind)] + " yet",
                std::string(ctx.spec.cli) + " help lists what it can do");
}

int cmd_link_register(Ctx& ctx) {
  if (!ctx.words.empty()) {
    return refuse(ctx, 2, "link register takes no words",
                  std::string(ctx.spec.cli) + " link register --dry-run");
  }
  const std::string cli = program_path(ctx.self_dir, ctx.spec.cli);
  const hub::LinkOs os = hub::this_os();
  const std::string home = home_dir();
  if (home.empty() && os != hub::LinkOs::windows) {
    return refuse(ctx, 1, "the home folder is not known (HOME is unset)", "set HOME and run again");
  }
  const hub::LinkPlan plan = hub::link_plan(ctx.spec.game, ctx.spec.title, cli, os, home);
  std::vector<std::string> files, commands;
  std::string text;
  for (const hub::LinkFile& f : plan.files) {
    files.push_back(f.path);
    text += "write " + f.path + "\n";
  }
  for (const std::vector<std::string>& c : plan.commands) {
    std::string line;
    for (const std::string& w : c) line += (line.empty() ? "" : " ") + w;
    commands.push_back(line);
    text += "run " + line + "\n";
  }
  const std::string result = json_obj({{"scheme", json_str(ctx.spec.game)},
                                       {"handler", json_str(plan.handler)},
                                       {"files", json_strs(files)},
                                       {"commands", json_strs(commands)}});
  if (ctx.has("dry-run")) {
    return answer(
        ctx, result, text,
        "would register " + std::string(ctx.spec.game) + ":// links (--dry-run: nothing changed)");
  }
  for (const hub::LinkFile& f : plan.files) {
    std::error_code ec;
    fs::create_directories(fs::path(f.path).parent_path(), ec);
    std::ofstream out(f.path, std::ios::binary);
    out << f.text;
    if (!out) {
      return refuse(ctx, 1, f.path + " cannot be written", "check the folder's permissions");
    }
  }
  if (!need_places(ctx)) return 1;
  std::error_code ec;
  fs::create_directories(ctx.places.logs, ec);
  const std::string log = (fs::path(ctx.places.logs) / "link-register.log").string();
  for (const std::vector<std::string>& c : plan.commands) {
    int code = 0;
    std::string error;
    if (!run_wait(c, log, code, error) || code != 0) {
      return refuse(
          ctx, 1,
          c[0] + (error.empty() ? " failed (exit " + std::to_string(code) + ")" : ": " + error),
          "read " + log + "; the links still open with " + std::string(ctx.spec.cli) +
              " link open <link>");
    }
  }
  return answer(ctx, result, text + "registered: " + plan.handler + "\n",
                std::string(ctx.spec.game) + ":// links now open in the game", "");
}

}  // namespace ge::gamecli
