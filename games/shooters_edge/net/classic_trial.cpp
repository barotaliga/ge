// One measured classic session (classic_trial.hpp).
//
// ge:summary The classic trial: the mod, the server, the scripted clients and the report
// ge:keywords run_classic_trial trial_files digest server-kbps
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_server_test.cpp
#include "shooters_edge/net/classic_trial.hpp"

#include <cstring>

#include "net/sim_network.hpp"
#include "progs/file.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace shooters_edge::net {

const char* const kTrialSkins[3] = {"base", "blue", "red"};

namespace {

constexpr int64_t kFrameUs = 13000;
constexpr uint16_t kClientPort = kPortClient;

uint64_t fold(uint64_t h, uint64_t v) {
  for (int i = 0; i < 8; i++) {
    h ^= (v >> (8 * i)) & 0xffu;
    h *= 1099511628211ull;
  }
  return h;
}

uint64_t fold_bytes(uint64_t h, const std::vector<uint8_t>& b) {
  for (uint8_t x : b) {
    h ^= x;
    h *= 1099511628211ull;
  }
  return h;
}

const char* mode_name(ClientExtMode m) {
  return m == ClientExtMode::lines ? "lines" : m == ClientExtMode::pext ? "pext" : "none";
}

}  // namespace

std::vector<ServedFile> trial_files() {
  std::vector<ServedFile> files;
  const char* names[2] = {"skins/blue.pcx", "skins/red.pcx"};
  const size_t sizes[2] = {5000, 2900};
  for (int f = 0; f < 2; f++) {
    ServedFile s;
    s.path = names[f];
    s.bytes.resize(sizes[f]);
    for (size_t i = 0; i < sizes[f]; i++)
      s.bytes[i] = static_cast<uint8_t>((i * 131 + static_cast<size_t>(f) * 17 + 7) & 0xff);
    files.push_back(std::move(s));
  }
  return files;
}

TrialReport run_classic_trial(const TrialOptions& opt) {
  TrialReport rep;
  rep.preset = opt.preset;
  gn::LinkConditions cond;
  if (!gn::conditions_preset(opt.preset, cond)) {
    rep.error = "run_classic_trial: no preset named " + opt.preset +
                "; next: one of lan, broadband, wifi, mobile, bad, awful";
    return rep;
  }
  if (opt.clients < 1 || opt.clients > 16) {
    rep.error = "run_classic_trial: clients must be 1..16; next: pass --clients=4";
    return rep;
  }
  ge::progs::Progs progs;
  std::string error;
  if (!sv::build_test_mod(progs, error)) {
    rep.error = "run_classic_trial: " + error;
    return rep;
  }
  sv::QcServerOptions so;
  so.seed = opt.seed;
  so.loop.max_edicts = 1024;
  std::unique_ptr<sv::QcServer> qc = sv::make_qc_server(progs, so, error);
  if (!qc) {
    rep.error = "run_classic_trial: " + error;
    return rep;
  }
  sv::MapSpec map;
  map.name = "arena";
  map.models = sv::arena_models();
  map.entities = sv::kArenaEntities;
  if (!sv::load_map(*qc, map, error)) {
    rep.error = "run_classic_trial: " + error;
    return rep;
  }

  gn::SimNetwork net = gn::make_sim_network(opt.seed, cond);
  const gn::Address server_at = gn::sim_address(0, kPortServer);
  auto st = std::make_unique<gn::Transport>();
  gn::open_sim_transport(*st, net, server_at);
  gn::Capture capture = gn::make_capture();
  if (opt.capture) st->capture = &capture;
  auto server = std::make_unique<ClassicServer>();
  ServerConfig cfg;
  cfg.files = trial_files();
  cfg.antilag = opt.antilag;
  cfg.seed = opt.seed;
  cfg.maxrate = opt.maxrate;
  if (!classic_start(*server, *qc, *st, cfg, error)) {
    rep.error = error;
    return rep;
  }

  const auto n = static_cast<size_t>(opt.clients);
  std::vector<std::unique_ptr<gn::Transport>> ct(n);
  std::vector<std::unique_ptr<ClassicClientEnd>> clients(n);
  for (size_t i = 0; i < n; i++) {
    ct[i] = std::make_unique<gn::Transport>();
    gn::open_sim_transport(*ct[i], net, gn::sim_address(static_cast<uint32_t>(i + 1), kClientPort));
    ClientConfig cc;
    cc.name = "client" + std::to_string(i + 1);
    cc.mode = static_cast<ClientExtMode>(i % 3);
    cc.skin = kTrialSkins[i % 3];
    cc.files = {"skins/base.pcx"};
    cc.seed = static_cast<int>(i) + 1;
    cc.qport = static_cast<uint16_t>(1000 + i);
    cc.topcolor = static_cast<int>(i % 14);
    cc.bottomcolor = static_cast<int>((i * 5) % 14);
    clients[i] = std::make_unique<ClassicClientEnd>();
    client_start(*clients[i], *ct[i], server_at, cc, 0);
  }

  const auto frames = static_cast<int>(opt.seconds * 1e6 / kFrameUs);
  uint64_t h = 1469598103934665603ull;
  int64_t last_bytes = 0;
  for (int f = 0; f < frames; f++) {
    // Clients join a fifth of a second apart, so their signons overlap but do not coincide.
    const int64_t now = static_cast<int64_t>(f) * kFrameUs;
    gn::sim_advance(net, now);
    for (size_t i = 0; i < n; i++)
      if (now >= static_cast<int64_t>(i) * 200000) client_tick(*clients[i], now);
    classic_frame(*server, now);
    h = fold(h, sh::server_hash(qc->sv));
    if ((f + 1) % 77 == 0) {
      rep.server_kbps.push_back(static_cast<double>(server->counts.bytes_out - last_bytes) * 8 /
                                1000.0 / (77 * kFrameUs / 1e6));
      last_bytes = server->counts.bytes_out;
    }
  }
  rep.frames = frames;
  rep.seconds = frames * kFrameUs / 1e6;

  const std::vector<ServedFile> files = trial_files();
  for (size_t i = 0; i < n; i++) {
    const ClassicClientEnd& c = *clients[i];
    TrialClient t;
    t.name = c.cfg.name;
    t.mode = mode_name(c.cfg.mode);
    t.stage = client_stage_name(c.stage);
    t.refused = c.refused;
    t.connect_s = c.connected_at;
    t.active_s = c.active_at;
    t.bytes_in = c.bytes_in;
    t.bytes_out = c.bytes_out;
    t.full_frames = c.full_frames;
    t.delta_frames = c.delta_frames;
    t.delta_misses = c.delta_misses;
    t.entities = c.entities_last;
    for (bool s : c.player_seen) t.players_seen += s ? 1 : 0;
    t.prints = static_cast<int64_t>(c.prints.size());
    t.chunks_oob = c.chunks_oob;
    for (const auto& [path, bytes] : c.downloaded)
      for (const ServedFile& sf : files)
        if (sf.path == path && sf.bytes == bytes) t.downloads++;
    t.files = static_cast<int>(c.downloaded.size());
    t.stats_health = c.stats[0];
    t.ext = describe_ext(c.ext);
    rep.clients.push_back(t);
    h = fold(h, static_cast<uint64_t>(c.stage));
    h = fold(h, static_cast<uint64_t>(c.bytes_in));
    h = fold(h, static_cast<uint64_t>(c.bytes_out));
    h = fold(h, static_cast<uint64_t>(c.full_frames));
    h = fold(h, static_cast<uint64_t>(c.delta_frames));
    h = fold(h, static_cast<uint64_t>(c.entities_last));
    for (int32_t s : c.stats) h = fold(h, static_cast<uint32_t>(s));
    for (const auto& [path, bytes] : c.downloaded) h = fold_bytes(h, bytes);
    for (const PlayerState& p : c.players) {
      uint32_t u[3];
      std::memcpy(u, &p.origin, sizeof u);
      for (uint32_t x : u) h = fold(h, x);
    }
  }
  rep.server = server->counts;
  for (const ClassicClient& c : server->clients) {
    rep.moves += static_cast<int64_t>(c.moves);
    rep.moves_unchecked += static_cast<int64_t>(c.moves_unchecked);
    rep.moves_bad_checksum += static_cast<int64_t>(c.moves_bad_checksum);
  }
  rep.lag_rewinds = server->lag_rewinds;
  rep.net_sent = static_cast<int64_t>(net.stats.sent);
  rep.net_lost = static_cast<int64_t>(net.stats.lost + net.stats.queue_dropped);
  rep.log = server->log;
  h = fold(h, static_cast<uint64_t>(rep.server.bytes_out));
  h = fold(h, static_cast<uint64_t>(rep.server.bytes_in));
  rep.digest = h;
  if (opt.capture) rep.pcap = capture.bytes;
  rep.ok = true;
  return rep;
}

}  // namespace shooters_edge::net
