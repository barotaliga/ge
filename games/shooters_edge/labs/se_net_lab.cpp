// LAB `se-net`: does the classic server (games/shooters_edge/net/classic_server.hpp) sign scripted
// clients on and keep them playing under each network condition, and what does it cost on the
// wire? For each preset (`--preset`, or every one): a session of `--clients` scripted clients
// (classic_client.hpp, each asking for its extensions a different way) on the test arena over
// engine/net's simulated network for `--seconds` of server time. A table per preset with every
// client's stage, when it connected and began, the bytes a second each way, its frames full and
// delta, the entity frames it could not delta from, its downloads; a table of the presets side by
// side (the server's bytes a second, the network's loss, the moves and their checksums, the
// rejects and drops); the session's digest; and a chart of the server's bytes a second over the
// session, a line a preset.
//
// ge:summary Lab: classic sessions under every condition preset: bytes per second, loss, digest
// ge:keywords lab se-net classic-protocol classic-server preset bytes-per-second loss signon
// ge:keywords downloads delta-frames digest server-kbps
// ge:skill net labs
// ge:test tests/shooters_edge_classic_server_test.cpp
#include <cstdio>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/conditions.hpp"
#include "shooters_edge/net/classic_trial.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace cn = shooters_edge::net;
using ge::core::num::to_fixed;

std::string hex64(uint64_t v) {
  char buf[17];
  std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
  return buf;
}

std::string at(double s) { return s < 0 ? "never" : to_fixed(s, 2); }

void run(gl::Lab& lab) {
  const std::string want = lab.arg("preset");
  cn::TrialOptions base;
  base.clients = static_cast<int>(lab.arg_num("clients"));
  base.seconds = lab.arg_num("seconds");
  base.seed = static_cast<uint32_t>(lab.arg_num("seed"));
  base.antilag = lab.arg_num("antilag") != 0;
  base.capture = lab.arg_num("capture") != 0;
  std::vector<std::string> presets;
  for (const std::string& p : gn::preset_names())
    if (want == "all" || want == p) presets.push_back(p);
  if (presets.empty()) {
    lab.fail("se-net: no preset named " + want, "next: make lab LAB=se-net ARGS=--help");
    return;
  }
  const std::vector<std::pair<std::string, std::string>> in = {{"preset", want},
                                                               {"clients", lab.arg("clients")},
                                                               {"seconds", lab.arg("seconds")},
                                                               {"seed", lab.arg("seed")},
                                                               {"antilag", lab.arg("antilag")}};

  gl::LabTable side{"the presets side by side (bytes a second are the server's, both ways)",
                    {"preset", "out B/s", "in B/s", "datagrams lost %", "clients playing", "moves",
                     "bad checksums", "mod bytes dropped", "rejects", "drops", "digest"},
                    {}};
  gl::LineChart chart;
  chart.spikes = false;
  chart.frame = {"THE SERVER'S BYTES OUT A SECOND",
                 std::to_string(base.clients) + " scripted clients, " + to_fixed(base.seconds, 0) +
                     " s, a line a preset",
                 "server time (s)", "kbit/s"};
  for (const std::string& p : presets) {
    cn::TrialOptions o = base;
    o.preset = p;
    const cn::TrialReport r = cn::run_classic_trial(o);
    if (!r.ok) {
      lab.fail("se-net: " + r.error, "next: make lab LAB=se-net ARGS=--help");
      return;
    }
    gl::LabTable t{"under " + p + ": every client",
                   {"client", "asks", "agreed", "stage", "connected s", "playing s", "in B/s",
                    "out B/s", "full frames", "delta frames", "delta misses", "entities",
                    "downloads", "chunks out of band"},
                   {}};
    int playing = 0;
    for (const cn::TrialClient& c : r.clients) {
      if (c.stage == "active") playing++;
      t.rows.push_back({c.name, c.mode, c.ext.size() > 40 ? c.ext.substr(0, 40) + "..." : c.ext,
                        c.stage, at(c.connect_s), at(c.active_s),
                        to_fixed(static_cast<double>(c.bytes_in) / r.seconds, 0),
                        to_fixed(static_cast<double>(c.bytes_out) / r.seconds, 0),
                        std::to_string(c.full_frames), std::to_string(c.delta_frames),
                        std::to_string(c.delta_misses), std::to_string(c.entities),
                        std::to_string(c.downloads), std::to_string(c.chunks_oob)});
    }
    lab.table(t);
    const double out_bps = static_cast<double>(r.server.bytes_out) / r.seconds;
    const double in_bps = static_cast<double>(r.server.bytes_in) / r.seconds;
    const double lost =
        r.net_sent > 0 ? 100.0 * static_cast<double>(r.net_lost) / static_cast<double>(r.net_sent)
                       : 0;
    side.rows.push_back(
        {p, to_fixed(out_bps, 0), to_fixed(in_bps, 0), to_fixed(lost, 1),
         std::to_string(playing) + "/" + std::to_string(r.clients.size()), std::to_string(r.moves),
         std::to_string(r.moves_bad_checksum), std::to_string(r.server.mod_bytes_dropped),
         std::to_string(r.server.rejects), std::to_string(r.server.drops), hex64(r.digest)});
    lab.metric(p + ".out-bytes-per-s", out_bps, "B/s");
    lab.metric(p + ".in-bytes-per-s", in_bps, "B/s");
    lab.metric(p + ".lost", lost, "%");
    lab.metric(p + ".playing", playing, "clients");
    lab.metric(p + ".bad-checksums", static_cast<double>(r.moves_bad_checksum), "count");
    lab.metric(p + ".mod-bytes-dropped", static_cast<double>(r.server.mod_bytes_dropped), "B");
    if (playing != static_cast<int>(r.clients.size()))
      lab.note("under " + p + " not every client reached play in " + to_fixed(r.seconds, 0) +
               " s: its table says where each stopped");
    if (base.capture) lab.attach(p + ".pcap", r.pcap);
    gl::Trace tr{p, {}, {}};
    for (size_t i = 0; i < r.server_kbps.size(); i++) {
      tr.x.push_back(static_cast<double>(i + 1));
      tr.y.push_back(r.server_kbps[i]);
    }
    chart.traces.push_back(tr);
  }
  lab.table(side);
  const gl::Chart c = gl::line_chart(chart);
  const std::string svg = gl::to_svg(c.plot);
  lab.attach("server-kbps.svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({"server-kbps", c.title,
            "The server's bytes out a second over each session, in kbit/s, a line a preset. The "
            "clients join a fifth of a second apart; the first seconds carry the signon (the "
            "lists, the baselines, the skins downloaded), then the play's steady stream of "
            "player records and entity deltas.",
            "a rise over the first seconds while the clients sign on, then a flat line; each "
            "worse preset's line sits lower and rises later, because the server answers only "
            "the packets it hears and the signon waits on lost reliable messages",
            in},
           gl::to_picture(c.plot));
}

const gl::LabSpec kSpec{
    "se-net",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/net: the classic protocol's server, its channel and codec, driven by "
    "scripted clients over engine/net's simulated network",
    "net",
    "Does the classic server sign scripted clients on and keep them playing under each network "
    "condition, and how many bytes a second does it cost each way?",
    {
        {"preset", "all", "a condition preset (lan, broadband, wifi, mobile, bad, awful) or all"},
        {"clients", "4", "scripted clients, 1..16"},
        {"seconds", "20", "seconds of server time a session"},
        {"seed", "1", "the session's seed (the network's draws, the challenges)"},
        {"antilag", "0", "1: lag compensation for hitscan"},
        {"capture", "0",
         "1: attach each preset's server capture (<preset>.pcap, for ge net inspect)"},
    },
    "--seconds=6 --clients=3",
    run,
};
GE_LAB(kSpec);

}  // namespace
