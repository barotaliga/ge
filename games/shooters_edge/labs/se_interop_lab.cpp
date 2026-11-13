// LAB `se-interop`: does our protocol hold against existing programs of the classic protocol?
// With `--server=<host:port>`, the lab's scripted classic client (net/classic_client.hpp) joins
// that server over a real UDP socket and plays `--seconds` of wall time: every stage it reached
// and when, why it was dropped, the server info, the lines printed to it, the frames and bytes,
// and every message kind it read. `--basedir` is the game's folder the client reads the map from,
// for the checksum its first `prespawn` carries. The server is any program that speaks the protocol
// (an existing server built beside the checkout, never in the tree, or our own
// `shooters_edge_server`). Without
// `--server` (the smoke run) it reports "not run" with the commands a session runs, so the lab
// stays deterministic in CI and the gap stays visible.
//
// ge:summary Lab: our scripted classic client against a live server over UDP (else: not run)
// ge:keywords lab se-interop interop classic-protocol existing-server existing-client udp
// ge:keywords live-server signon-stages map-checksum not-run
// ge:skill net labs
// ge:test tests/shooters_edge_classic_server_test.cpp
#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "core/num_text.hpp"
#include "forward/forward.hpp"
#include "lab/lab.hpp"
#include "net/address.hpp"
#include "net/transport.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_forward.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/classic_record.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace fw = ge::forward;
namespace sn = shooters_edge::net;

void not_run(gl::Lab& lab) {
  gl::LabTable steps{"what the lab runs (not run: no --server given)", {"step", "command"}, {}};
  steps.rows.push_back({"1. an existing server beside the checkout",
                        "<server> -port 27500 +map <map> (built outside the tree)"});
  steps.rows.push_back({"2. our scripted client joins it and plays",
                        "make lab LAB=se-interop ARGS=--server=127.0.0.1:27500"});
  steps.rows.push_back({"3. an existing client joins our server",
                        "build/<preset>/shooters_edge_server; <client> +connect 127.0.0.1:27500"});
  lab.table(steps);
  lab.metric("interop.run", 0, "bool");
  lab.note("se-interop: not run. Give --server=<host:port> of a live classic server.");
}

void live(gl::Lab& lab, const std::string& server) {
  gn::Address to;
  std::string text = server;
  if (text.find(':') == std::string::npos) text += ":27500";
  if (!gn::parse_address(text, to)) {
    lab.note("se-interop: '" + server + "' is not host:port; next: --server=127.0.0.1:27500");
    lab.metric("interop.run", 0, "bool");
    return;
  }
  gn::Transport t;
  std::string error;
  if (!gn::open_udp_transport(t, gn::ipv4(0, 0, 0, 0, 0), &error)) {
    lab.note("se-interop: the UDP socket did not open: " + error);
    lab.metric("interop.run", 0, "bool");
    return;
  }
  sn::ClientConfig cfg;
  cfg.name = lab.arg("name");
  cfg.topcolor = 4;
  cfg.bottomcolor = 4;
  const std::string basedir = lab.arg("basedir");
  // The search path is mounted once (its packs are read whole, which takes a while) and kept.
  auto files = std::make_shared<shooters_edge::client::SearchPath>();
  auto mounted = std::make_shared<std::string>();
  if (!basedir.empty())
    // A callback held in a std::function: what it throws (a string out of memory)
    // reaches its caller, as any call's does.
    // NOLINTNEXTLINE(bugprone-exception-escape)
    cfg.read_file = [basedir, files, mounted](const std::string& gamedir, const std::string& path) {
      std::vector<uint8_t> store;
      if (*mounted != gamedir) {
        std::string why;
        *files = {};
        if (!shooters_edge::client::open_game_dir(basedir, gamedir, *files, why)) return store;
        *mounted = gamedir;
      }
      const std::span<const uint8_t> got = shooters_edge::client::find_file(*files, path, store);
      return std::vector<uint8_t>(got.begin(), got.end());
    };
  // With --forward=<port>, our forward opens on that loopback port, allowed only the server, and
  // the client reaches the server through it: its connect carries the route in `prx`.
  const int forward_port = static_cast<int>(lab.arg_num("forward"));
  gn::Transport hop_port;
  fw::Forward fwd;
  sn::ClassicFace face;
  gn::Address first = to;
  if (forward_port > 0) {
    const gn::Address hop = gn::ipv4(127, 0, 0, 1, static_cast<uint16_t>(forward_port));
    if (!gn::open_udp_transport(hop_port, hop, &error)) {
      lab.note("se-interop: the forward's socket did not open on " + gn::to_string(hop) + ": " +
               error + "; next: pick a free port with --forward=<port>");
      lab.metric("interop.run", 0, "bool");
      return;
    }
    fw::ForwardConfig fc;
    fc.name = "se-interop";
    fc.allow = {to};
    fc.seed = 7;
    fw::forward_init(fwd, fc, hop_port, [](gn::Transport& out) {
      return gn::open_udp_transport(out, gn::ipv4(0, 0, 0, 0, 0), nullptr);
    });
    sn::classic_forward_attach(fwd, face);
    cfg.prx = text;
    first = hop;
  }
  sn::ClassicClientEnd c;
  sn::client_start(c, t, first, cfg, 0);
  sn::ClientRecorder recording;
  const std::string record_to = lab.arg("record");
  if (!record_to.empty()) {
    sn::record_start(recording, false);
    c.record = &recording;
  }
  const double seconds = lab.arg_num("seconds");
  const auto start = std::chrono::steady_clock::now();
  gl::LabTable stages{"stages reached (wall time)", {"stage", "at s"}, {}};
  sn::ClientStage said = c.stage;
  stages.rows.push_back({sn::client_stage_name(said), "0.000"});
  int64_t now_us = 0;
  while (now_us < static_cast<int64_t>(seconds * 1e6)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(13));
    now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                 std::chrono::steady_clock::now() - start)
                 .count();
    if (forward_port > 0) fw::forward_update(fwd, now_us);
    sn::client_tick(c, now_us);
    if (c.stage != said) {
      said = c.stage;
      stages.rows.push_back({sn::client_stage_name(said),
                             ge::core::num::to_fixed(static_cast<double>(now_us) / 1e6, 3)});
      if (said == sn::ClientStage::dropped) break;
    }
  }
  lab.table(stages);
  if (!record_to.empty()) {
    sn::record_stop(recording);
    std::ofstream out(record_to, std::ios::binary);
    out.write(reinterpret_cast<const char*>(recording.bytes.data()),
              static_cast<std::streamsize>(recording.bytes.size()));
    lab.note("se-interop: the session recorded as a client demo at " + record_to +
             " (watch it: shooters_edge -basedir <dir> -playdemo <file>)");
  }
  gl::LabTable facts{"the session", {"what", "value"}, {}};
  facts.rows.push_back({"server", text});
  facts.rows.push_back({"refused or dropped", c.refused.empty() ? "-" : c.refused});
  facts.rows.push_back({"read error", c.error.empty() ? "-" : c.error});
  facts.rows.push_back({"gamedir / level", c.gamedir + " / " + c.level});
  facts.rows.push_back({"models / sounds",
                        std::to_string(c.models.size()) + " / " + std::to_string(c.sounds.size())});
  facts.rows.push_back({"frames full / delta / missed", std::to_string(c.full_frames) + " / " +
                                                            std::to_string(c.delta_frames) + " / " +
                                                            std::to_string(c.delta_misses)});
  facts.rows.push_back({"datagrams / bytes in / bytes out", std::to_string(c.datagrams_in) + " / " +
                                                                std::to_string(c.bytes_in) + " / " +
                                                                std::to_string(c.bytes_out)});
  facts.rows.push_back({"stufftexts", std::to_string(c.stufftexts)});
  facts.rows.push_back({"packets sent / send failures",
                        std::to_string(c.ch.sent) + " / " + std::to_string(t.send_failures)});
  facts.rows.push_back({"serverinfo", c.serverinfo});
  lab.table(facts);
  if (forward_port > 0) {
    // The server's copy of our own user info, as it sent it to every client: the forwarder's mark.
    std::string mark = "-";
    for (int i = 0; i < 32; i++)
      if (sn::info_get(c.userinfos[i], "name") == cfg.name) {
        const std::string got = sn::info_get(c.userinfos[i], "*qwfwd");
        if (!got.empty()) mark = got;
      }
    gl::LabTable hop{"through our forward", {"what", "value"}, {}};
    hop.rows.push_back({"route (prx)", cfg.prx});
    hop.rows.push_back({"challenges / connects / accepted / refused",
                        std::to_string(face.counts.challenges) + " / " +
                            std::to_string(face.counts.connects) + " / " +
                            std::to_string(face.counts.accepted) + " / " +
                            std::to_string(face.counts.refused)});
    uint64_t up = 0, down = 0;
    for (const auto& h : fwd.clients) {
      up += h->bytes_up;
      down += h->bytes_down;
    }
    hop.rows.push_back({"bytes up / down", std::to_string(up) + " / " + std::to_string(down)});
    hop.rows.push_back({"the server's *qwfwd for us", mark});
    lab.table(hop);
    lab.metric("interop.forward_accepted", static_cast<double>(face.counts.accepted), "clients");
    lab.metric("interop.forward_mark", mark != "-" ? 1 : 0, "bool");
    fw::forward_close(fwd);
  }
  gl::LabTable printed{"lines printed to the client", {"line"}, {}};
  for (const std::string& p : c.prints) printed.rows.push_back({p});
  for (const std::string& p : c.centerprints) printed.rows.push_back({"[centre] " + p});
  lab.table(printed);
  gl::LabTable stuffs{"lines the server stuffed (the first kept)", {"line"}, {}};
  for (const std::string& l : c.stuffed) stuffs.rows.push_back({l});
  lab.table(stuffs);
  gl::LabTable kinds{"message kinds read", {"svc", "count"}, {}};
  for (int i = 0; i < 256; i++)
    if (c.svc_counts[i] > 0)
      kinds.rows.push_back({std::to_string(i), std::to_string(c.svc_counts[i])});
  lab.table(kinds);
  lab.metric("interop.run", 1, "bool");
  lab.metric("interop.active", c.stage == sn::ClientStage::active ? 1 : 0, "bool");
  lab.metric("interop.frames", static_cast<double>(c.full_frames + c.delta_frames), "frames");
}

void run(gl::Lab& lab) {
  const std::string& server = lab.arg("server");
  if (server.empty())
    not_run(lab);
  else
    live(lab, server);
}

const gl::LabSpec kSpec{
    "se-interop",
    gl::LabKind::drive,
    "game:shooters_edge",
    "games/shooters_edge/net: the classic protocol against existing clients and servers",
    "net",
    "Does our classic client sign on to a live server of the classic protocol and play? "
    "(--server=<host:port>; without it the lab reports not run.)",
    {{"server", "", "a live classic server, as host:port (empty: not run)"},
     {"seconds", "10", "wall seconds to play"},
     {"name", "interop", "the client's name"},
     {"record", "", "write the session as a client demo to this path (empty: none)"},
     {"basedir", "", "the game's base folder (its id1 and mod folders): the map's checksum"},
     {"forward", "0",
      "play through our forward on this loopback port (0: straight to the server)"}},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
