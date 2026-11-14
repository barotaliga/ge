// The game CLI's `account` group against a real account service on the loopback: sign up (the
// recovery code), sign in (the ticket kept and shown), a login code, the key learned at the first
// contact and a service answering with another key refused, a wrong password refused in words.
//
// ge:summary Tests the game CLI's account group: signup, signin, code, show, a changed key refused
// ge:keywords test gamecli account signup signin code show key trust-on-first-use
// ge:skill agent-interface testing
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>

#include "account/matchmaker_service.hpp"
#include "account/service.hpp"
#include "check.hpp"
#include "core/json_read.hpp"
#include "gamecli/gamecli.hpp"
#include "net/udp_socket.hpp"
#include "shooters_edge/cli/spec.hpp"

namespace fs = std::filesystem;
namespace ga = ge::account;
namespace gc = ge::gamecli;
namespace gn = ge::net;

namespace {

fs::path fresh(const char* name) {
  const fs::path p = fs::temp_directory_path() / name;
  std::error_code ec;
  fs::remove_all(p, ec);
  fs::create_directories(p, ec);
  return p;
}

void counter_fresh(void* ctx, uint8_t* out, size_t size) {
  auto* n = static_cast<uint64_t*>(ctx);
  for (size_t i = 0; i < size; i++) out[i] = static_cast<uint8_t>((*n * 131 + i * 17) >> 1);
  (*n)++;
}

// An account service on the loopback, answering on its own thread while the test runs.
struct Running {
  gn::Transport port;
  ga::Service svc;
  uint64_t fresh_n = 1;
  std::atomic<bool> stop{false};
  std::thread thread;

  // `offer`: a server's seats for Shooter's Edge's 1on1s (its ladder's game: the folder), offered
  // before the service starts.
  explicit Running(uint8_t seed_byte, bool offer = false) {
    gn::Address bind;
    gn::parse_address("127.0.0.1:0", bind);
    std::string error;
    gn::open_udp_transport(port, bind, &error);
    ga::Store store;
    ga::store_init(store, ga::HashCost{8, 1});
    ga::ServiceConfig c;
    c.seed.fill(seed_byte);
    ga::service_init(svc, c, std::move(store), port, counter_fresh, &fresh_n);
    if (offer) {
      const auto now = std::chrono::system_clock::now().time_since_epoch();
      ga::service_offer(svc, gn::Key{}, "id1", "1on1", "192.0.2.7:27500", 8,
                        std::chrono::duration_cast<std::chrono::seconds>(now).count());
    }
    thread = std::thread([this] {
      while (!stop) {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        ga::service_update(svc, std::chrono::duration_cast<std::chrono::microseconds>(now).count());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    });
  }
  ~Running() {
    stop = true;
    thread.join();
    gn::close_transport(port);
  }
  std::string at() const { return "127.0.0.1:" + std::to_string(port.local.port); }
};

int run(std::vector<std::string> args, std::string& out, const fs::path& root) {
  args.insert(args.begin(), {"--basedir=" + root.string(), "--state=" + (root / "state").string(),
                             "--logs=" + (root / "logs").string(), "--json"});
  return gc::run(shooters_edge::cli::spec(), args, out, (root / "bin").string());
}

ge::core::FlatJson parse(const std::string& out) {
  ge::core::FlatJson j;
  std::string error;
  if (!ge::core::read_flat(out, j, error, true)) ge::test::fail(__FILE__, __LINE__, error + out);
  return j;
}

}  // namespace

TEST(gamecli_account_signs_up_signs_in_and_makes_a_code) {
  const fs::path root = fresh("ge_gamecli_account");
  Running service(7);
  std::string out;
  CHECK_MSG(
      run({"account", "signup", "Ranger", "--master=" + service.at(), "--password=long enough"},
          out, root) == 0,
      out);
  ge::core::FlatJson j = parse(out);
  CHECK_MSG(j["result.recovery_code"].text.size() == 19, out);
  // The service is kept: no --master from here on.
  CHECK_MSG(run({"account", "signin", "ranger", "--password=long enough"}, out, root) == 0, out);
  j = parse(out);
  CHECK_MSG(j["result.name"].text == "Ranger", out);
  CHECK_MSG(run({"account", "show"}, out, root) == 0, out);
  j = parse(out);
  CHECK_MSG(j["result.ticket.name"].text == "Ranger", out);
  CHECK(j["result.key"].text == gn::key_to_hex(ga::service_key(service.svc)));
  CHECK_MSG(run({"account", "code", "Ranger", "--password=long enough"}, out, root) == 0, out);
  j = parse(out);
  CHECK_MSG(j["result.code"].text.size() == 9, out);
  // Signed in: the ticket buys the code, no password asked.
  CHECK_MSG(run({"account", "code"}, out, root) == 0, out);
  j = parse(out);
  CHECK_MSG(j["result.code"].text.size() == 9 && j["result.by"].text == "ticket", out);
  CHECK_MSG(run({"account", "code", "ranger"}, out, root) == 0, out);
  CHECK_MSG(parse(out)["result.by"].text == "ticket", out);
  CHECK_MSG(run({"account", "code", "Ranger", "--password=not it at all"}, out, root) == 1, out);
  j = parse(out);
  CHECK_MSG(j["error"].text.find("password") != std::string::npos, out);
  fs::remove_all(root);
}

TEST(gamecli_account_refuses_a_service_whose_key_changed) {
  const fs::path root = fresh("ge_gamecli_account_key");
  std::string out;
  std::string first_at;
  {
    Running service(7);
    first_at = service.at();
    CHECK_MSG(run({"account", "signup", "Ranger", "--master=" + first_at, "--password=long enough"},
                  out, root) == 0,
              out);
  }
  // Another service now answers at the kept address (here: the kept file pointed at it).
  Running other(9);
  {
    const fs::path file = root / "state" / "account.txt";
    std::ifstream in(file);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    text.replace(text.find(first_at), first_at.size(), other.at());
    std::ofstream(file, std::ios::trunc) << text;
  }
  CHECK_MSG(run({"account", "signin", "Ranger", "--password=long enough"}, out, root) == 1, out);
  ge::core::FlatJson j = parse(out);
  CHECK_MSG(j["error"].text.find("another key") != std::string::npos, out);
  fs::remove_all(root);
}

TEST(gamecli_friends_add_see_each_other_and_invite) {
  const fs::path ann = fresh("ge_gamecli_friends_ann"), bob = fresh("ge_gamecli_friends_bob");
  Running service(9);
  std::string out;
  for (const auto& [root, name] : {std::pair{ann, "Ann"}, std::pair{bob, "Bob"}}) {
    CHECK_MSG(run({"account", "signup", name, "--master=" + service.at(), "--password=long enough"},
                  out, root) == 0,
              out);
    CHECK_MSG(run({"account", "signin", name, "--password=long enough"}, out, root) == 0, out);
  }
  CHECK_MSG(run({"friends", "add", "Bob"}, out, ann) == 0, out);
  CHECK_MSG(parse(out)["result.answer"].text == "asked", out);
  CHECK_MSG(run({"friends", "list"}, out, bob) == 0, out);
  CHECK_MSG(parse(out)["result.friends.0.kind"].text == "asks_you", out);
  CHECK_MSG(run({"friends", "add", "ann"}, out, bob) == 0, out);
  CHECK_MSG(parse(out)["result.words"].text == "You are now friends with Ann.", out);
  CHECK_MSG(run({"friends", "status", "playing", "--where=192.0.2.8:27500", "--words=dm4 duel"},
                out, bob) == 0,
            out);
  CHECK_MSG(run({"friends", "list"}, out, ann) == 0, out);
  ge::core::FlatJson j = parse(out);
  CHECK_MSG(
      j["result.friends.0.name"].text == "Bob" && j["result.friends.0.state"].text == "playing",
      out);
  CHECK_MSG(j["result.friends.0.where"].text == "192.0.2.8:27500", out);
  CHECK_MSG(
      run({"friends", "invite", "Bob", "--where=192.0.2.9:27500", "--note=2on2?"}, out, ann) == 0,
      out);
  CHECK_MSG(run({"friends", "invites"}, out, bob) == 0, out);
  j = parse(out);
  CHECK_MSG(j["result.invites.0.from"].text == "Ann" && j["result.invites.0.note"].text == "2on2?",
            out);
  // Not signed in: told to sign in.
  const fs::path cid = fresh("ge_gamecli_friends_cid");
  CHECK_MSG(run({"friends", "list", "--master=" + service.at()}, out, cid) == 2, out);
  CHECK_MSG(parse(out)["error"].text == "not signed in", out);
  for (const fs::path& p : {ann, bob, cid}) fs::remove_all(p);
}

TEST(gamecli_ranked_find_pairs_two_players_on_an_offered_server) {
  const fs::path ann = fresh("ge_gamecli_ranked_ann"), bob = fresh("ge_gamecli_ranked_bob");
  Running service(10, true);
  std::string out;
  for (const auto& [root, name] : {std::pair{ann, "Ann"}, std::pair{bob, "Bob"}}) {
    CHECK_MSG(run({"account", "signup", name, "--master=" + service.at(), "--password=long enough"},
                  out, root) == 0,
              out);
    CHECK_MSG(run({"account", "signin", name, "--password=long enough"}, out, root) == 0, out);
  }
  CHECK_MSG(run({"ranked", "find", "1on1"}, out, ann) == 0, out);
  CHECK_MSG(parse(out)["result.state"].text == "waiting", out);
  CHECK_MSG(run({"ranked", "find", "1on1"}, out, bob) == 0, out);
  ge::core::FlatJson j = parse(out);
  CHECK_MSG(j["result.state"].text == "matched", out);
  CHECK_MSG(j["result.address"].text == "192.0.2.7:27500", out);
  CHECK_MSG(j["result.theirs.0"].text == "Ann", out);
  CHECK_MSG(run({"ranked", "status"}, out, ann) == 0, out);
  CHECK_MSG(parse(out)["result.theirs.0"].text == "Bob", out);
  CHECK_MSG(run({"ranked", "find", "2on2"}, out, ann) == 1, out);  // no duo named
  for (const fs::path& p : {ann, bob}) fs::remove_all(p);
}
