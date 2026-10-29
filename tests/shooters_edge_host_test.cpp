// SHOOTER'S EDGE'S SERVER HOST: a server of bots runs, logs and replays to the bit. Eight stand-in
// bots on the test arena (games/shooters_edge/server/host.hpp, stand_in.hpp) run twice agree on
// every frame's hash; the run's input log goes to bytes and back unchanged and replays with no
// frame leaving it; a damaged log is refused with what is wrong; the eight players start on eight
// spawn points apart; a full server refuses a slot; a packet for a slot that is not a client's is
// ignored. tests/golden/shooters_edge_host.txt holds the hash at every second of a 10 s run,
// written by this test under GE_GOLDEN_WRITE=1 (a deliberate change to the loop, the program or the
// stand-in rewrites it; the PR says why).
//
// Golden rows: frame index, the server hash after it (hex words).
//
// ge:summary Tests: the server host's bots, input log and replay, its slots, and golden hashes
// ge:keywords test host server bots stand-in input-log replay golden slots spawn damaged-log
// ge:skill shooter testing
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "shooters_edge/server/host.hpp"
#include "shooters_edge/server/stand_in.hpp"

namespace se = shooters_edge::server;
namespace sh = ge::shooter;

namespace {

constexpr const char* kGolden = "tests/golden/shooters_edge_host.txt";
constexpr int kBots = 8;
constexpr int kFrames = 770;  // 10 s of 13 ms frames

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

// A host with its stand-ins (they live as long as it does).
struct Run {
  se::Host host;
  std::vector<se::StandIn> bots;
  std::vector<uint64_t> hashes;
};

std::unique_ptr<Run> make_run(int bots, uint32_t seed = 1) {
  auto r = std::make_unique<Run>();
  se::HostSettings s;
  s.seed = seed;
  std::string error;
  CHECK_MSG(se::host_create(r->host, s, error), error);
  r->bots.resize(static_cast<size_t>(bots));
  for (int b = 0; b < bots; b++) {
    r->bots[static_cast<size_t>(b)] = se::make_stand_in(seed, b);
    CHECK(se::host_add_bot(r->host, se::stand_in_source(r->bots[static_cast<size_t>(b)])) == b);
  }
  return r;
}

void run_frames(Run& r, int frames) {
  for (int n = 0; n < frames; n++) {
    se::host_frame(r.host, nullptr, 0);
    r.hashes.push_back(sh::server_hash(r.host.sv));
  }
}

const Run& reference() {
  static const std::unique_ptr<Run> run = [] {
    auto r = make_run(kBots);
    run_frames(*r, kFrames);
    return r;
  }();
  return *run;
}

}  // namespace

TEST(a_server_of_bots_runs_twice_to_the_same_bits) {
  const Run& a = reference();
  auto b = make_run(kBots);
  run_frames(*b, kFrames);
  CHECK(a.hashes == b->hashes);
  CHECK(a.host.sv.counts.commands > 0);
  // Another seed is another run.
  auto c = make_run(kBots, 2);
  run_frames(*c, kFrames);
  CHECK(c->hashes.back() != a.hashes.back());
}

TEST(the_input_log_round_trips_through_bytes) {
  const se::InputLog& log = reference().host.log;
  CHECK(log.frames.size() == static_cast<size_t>(kFrames));
  CHECK(log.events.size() == static_cast<size_t>(kBots));  // the bots' connects
  const std::vector<uint8_t> bytes = se::write_input_log(log);
  se::InputLog back;
  std::string error;
  CHECK_MSG(se::read_input_log(bytes, back, error), error);
  CHECK(back.settings.maxclients == log.settings.maxclients);
  CHECK(back.settings.seed == log.settings.seed);
  CHECK(back.settings.map == log.settings.map);
  CHECK(back.frames.size() == log.frames.size());
  CHECK(back.events.size() == log.events.size());
  CHECK(back.packets.size() == log.packets.size());
  CHECK(se::write_input_log(back) == bytes);
}

TEST(the_input_log_replays_every_frame) {
  const Run& r = reference();
  int diverged = 0;
  std::string error;
  std::vector<uint64_t> hashes;
  CHECK_MSG(se::replay_input_log(r.host.log, diverged, error, &hashes), error);
  CHECK(diverged == -1);
  CHECK(hashes == r.hashes);
  // A hash changed in the log is the frame the replay names.
  se::InputLog bent = r.host.log;
  bent.frames[300].hash ^= 1;
  CHECK(se::replay_input_log(bent, diverged, error));
  CHECK(diverged == 300);
}

TEST(a_damaged_input_log_is_refused) {
  const std::vector<uint8_t> bytes = se::write_input_log(reference().host.log);
  se::InputLog out;
  std::string error;
  std::vector<uint8_t> bad = bytes;
  bad[0] = 'X';
  CHECK(!se::read_input_log(bad, out, error));
  CHECK(error.find("magic") != std::string::npos);
  bad = bytes;
  bad[4] = 99;  // the version
  CHECK(!se::read_input_log(bad, out, error));
  CHECK(error.find("version") != std::string::npos);
  bad.assign(bytes.begin(), bytes.begin() + static_cast<long>(bytes.size() / 2));
  CHECK(!se::read_input_log(bad, out, error));
  CHECK(error.find("cut short") != std::string::npos);
}

TEST(eight_players_start_on_eight_spawns_apart) {
  auto r = make_run(kBots);
  se::host_frame(r->host, nullptr, 0);
  for (int a = 1; a <= kBots; a++)
    for (int b = a + 1; b <= kBots; b++) {
      const sh::Vec3f& pa = r->host.sv.edicts[static_cast<size_t>(a)].origin;
      const sh::Vec3f& pb = r->host.sv.edicts[static_cast<size_t>(b)].origin;
      const double dx = pa.x - pb.x, dy = pa.y - pb.y;
      CHECK_MSG(dx * dx + dy * dy > 64.0 * 64.0,
                "players " + std::to_string(a) + " and " + std::to_string(b) + " start together");
    }
}

TEST(a_full_server_refuses_a_slot_and_a_freed_one_is_taken_again) {
  se::Host h;
  se::HostSettings s;
  s.maxclients = 2;
  std::string error;
  CHECK_MSG(se::host_create(h, s, error), error);
  CHECK(se::host_add_remote(h) == 0);
  CHECK(se::host_add_remote(h) == 1);
  CHECK(se::host_add_remote(h) == -1);
  CHECK(se::host_count(h, se::SlotKind::remote) == 2);
  se::host_drop(h, 0);
  CHECK(se::host_count(h, se::SlotKind::remote) == 1);
  CHECK(se::host_add_remote(h) == 0);
  // Settings that cannot run are refused.
  se::Host bad;
  s.maxclients = 33;
  CHECK(!se::host_create(bad, s, error));
  se::Host other;
  s.maxclients = 8;
  s.map = "nowhere";
  CHECK(!se::host_create(other, s, error));
  CHECK(error.find("next:") != std::string::npos);
}

TEST(a_packet_for_a_slot_that_is_not_a_client_is_ignored) {
  se::Host a, b;
  se::HostSettings s;
  std::string error;
  CHECK_MSG(se::host_create(a, s, error), error);
  CHECK_MSG(se::host_create(b, s, error), error);
  se::StandIn sa = se::make_stand_in(1, 0), sb = se::make_stand_in(1, 0);
  CHECK(se::host_add_bot(a, se::stand_in_source(sa)) == 0);
  CHECK(se::host_add_bot(b, se::stand_in_source(sb)) == 0);
  sh::Packet stray[2];
  stray[0].client = 0;  // the bot's slot
  stray[1].client = 5;  // empty
  for (sh::Packet& p : stray) {
    p.sequence = 1;
    for (sh::MoveCmd& c : p.cmds) {
      c.msec = 13;
      c.forward = 400;
    }
  }
  for (int n = 0; n < 100; n++) {
    se::host_frame(a, nullptr, 0);
    se::host_frame(b, stray, 2);
  }
  CHECK(sh::server_hash(a.sv) == sh::server_hash(b.sv));
}

TEST(a_server_of_bots_holds_its_golden_hashes) {
  const Run& r = reference();
  std::vector<std::vector<uint64_t>> got;
  for (size_t i = 0; i < r.hashes.size(); i++)
    if (i % 77 == 76 || i + 1 == r.hashes.size()) got.push_back({i, r.hashes[i]});
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# games/shooters_edge/server/host.hpp's own record: 8 stand-in bots, seed 1, frame, "
           "server_hash after it (tests/shooters_edge_host_test.cpp)\n";
    for (const auto& row : got) {
      char line[64];
      std::snprintf(line, sizeof line, "%llx %016llx\n", static_cast<unsigned long long>(row[0]),
                    static_cast<unsigned long long>(row[1]));
      out << line;
    }
    std::printf("wrote %s (%zu rows)\n", kGolden, got.size());
    return;
  }
  const auto rows = ge::test::golden_rows(kGolden);
  CHECK_MSG(rows.size() == got.size(), std::string(kGolden) + " has the wrong row count");
  for (size_t i = 0; i < rows.size() && i < got.size(); i++)
    CHECK_MSG(rows[i] == got[i], "the server moved off its golden hash at frame " +
                                     std::to_string(got[i][0]) +
                                     ": if the change is meant, rewrite with GE_GOLDEN_WRITE=1 "
                                     "and say why in the PR; else run the se-server lab and "
                                     "replay its log to find the first frame that moved");
}
