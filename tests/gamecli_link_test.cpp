// The game CLI's hub links on Shooter's Edge's Spec (engine/gamecli/link.cpp): Join and Spectate
// links start the window on the server (as a spectator for the second), a Watch link is refused
// until the game watches broadcasts, a Play link's demo already kept is unpacked into the
// recording the demo player reads and played, one not kept is said to be fetched, a link from
// another game or carrying words is refused, and registering shows its files and commands.
//
// ge:summary Tests the game CLI's link commands: open join, spectate, tv, demo; register
// ge:keywords test gamecli link open register join spectate demo playdemo dry-run scheme
// ge:skill agent-interface testing
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/json_read.hpp"
#include "gamecli/gamecli.hpp"
#include "mod/sha256.hpp"
#include "shooters_edge/cli/spec.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/demo_write.hpp"

namespace fs = std::filesystem;
namespace gc = ge::gamecli;
namespace se = shooters_edge::client;

namespace {

fs::path fresh(const char* name) {
  const fs::path p = fs::temp_directory_path() / name;
  std::error_code ec;
  fs::remove_all(p, ec);
  fs::create_directories(p, ec);
  return p;
}

int run(std::vector<std::string> args, std::string& out, const fs::path& root) {
  args.insert(args.begin(), {"--basedir=" + root.string(), "--state=" + (root / "state").string(),
                             "--logs=" + (root / "logs").string()});
  return gc::run(shooters_edge::cli::spec(), args, out, (root / "bin").string());
}

std::string command_of(const std::string& out) {
  ge::core::FlatJson j;
  std::string error;
  if (!ge::core::read_flat(out, j, error, true)) return "(not json) " + out;
  return j["result.command"].text;
}

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

}  // namespace

TEST(join_and_spectate_links_start_the_window) {
  const fs::path root = fresh("ge_gamecli_link_join");
  std::string out;
  CHECK(run({"link", "open", "shooters_edge://join/10.0.0.5:27500", "--dry-run", "--json"}, out,
            root) == 0);
  CHECK_MSG(has(command_of(out), "+connect 10.0.0.5:27500"), out);
  CHECK(run({"link", "open", "shooters_edge://spectate/arena.example.org/", "--dry-run", "--json"},
            out, root) == 0);
  const std::string spec = command_of(out);
  CHECK_MSG(has(spec, "+set spectator 1 +connect arena.example.org"), spec);
  fs::remove_all(root);
}

TEST(other_links_are_refused) {
  const fs::path root = fresh("ge_gamecli_link_refuse");
  std::string out;
  CHECK(run({"link", "open", "shooters_edge://tv/tv.example.org:28000/1", "--json"}, out, root) ==
        1);
  CHECK_MSG(has(out, "cannot open links for watching broadcasts yet"), out);
  CHECK(run({"link", "open", "other_game://join/10.0.0.5", "--json"}, out, root) == 2);
  CHECK(run({"link", "open", "shooters_edge://join/10.0.0.5;quit", "--json"}, out, root) == 2);
  CHECK(run({"link", "open", "--json"}, out, root) == 2);
  fs::remove_all(root);
}

TEST(a_kept_demo_is_unpacked_and_played) {
  const fs::path root = fresh("ge_gamecli_link_demo");
  se::MatchScript m;
  m.frames = 300;
  for (int p = 0; p < 2; p++) {
    se::MatchPlayer pl;
    pl.name = "player" + std::to_string(p);
    for (int i = 0; i < m.frames; i++) {
      pl.origins.push_back({static_cast<float>(p * 100 + i), 40.0f, 24});
      pl.health.push_back(100);
      pl.frags.push_back(i / 100);
    }
    pl.weapon = {32};
    pl.armour = {100};
    m.players.push_back(pl);
  }
  const std::vector<uint8_t> recording = se::write_multiview_match(m);
  std::vector<uint8_t> file;
  std::string error;
  CHECK(se::pack_demo(recording, file, error));
  const std::string sha = ge::mod::sha256_hex(file.data(), file.size());
  std::string out;
  const std::string link = "shooters_edge://demo/" + sha + "?hub=127.0.0.1:9";
  // Not kept yet: a dry run says what it would fetch, fetching nothing.
  CHECK(run({"link", "open", link, "--dry-run", "--json"}, out, root) == 0);
  CHECK_MSG(has(out, "would fetch the demo") && has(out, "127.0.0.1:9"), out);
  // Kept (as a fetch leaves it): unpacked beside it and played.
  fs::create_directories(root / "state" / "demos");
  {
    std::ofstream f(root / "state" / "demos" / (sha + ".ged"), std::ios::binary);
    f.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
  }
  CHECK(run({"link", "open", link, "--dry-run", "--json"}, out, root) == 0);
  const fs::path mvd = root / "state" / "demos" / (sha + ".mvd");
  CHECK_MSG(has(command_of(out), "-playdemo " + mvd.string()), out);
  {
    // Closed before the folder is removed: Windows refuses to delete a file still open.
    std::ifstream in(mvd, std::ios::binary);
    const std::vector<uint8_t> back((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
    CHECK(back == recording);
  }
  fs::remove_all(root);
}

TEST(register_shows_its_plan) {
  const fs::path root = fresh("ge_gamecli_link_register");
  std::string out;
  CHECK(run({"link", "register", "--dry-run", "--json"}, out, root) == 0);
  ge::core::FlatJson j;
  std::string error;
  CHECK(ge::core::read_flat(out, j, error, true));
  CHECK(j["result.scheme"].text == "shooters_edge");
  CHECK(!j["result.handler"].text.empty() && j.count("result.commands.0") == 1);
  fs::remove_all(root);
}
