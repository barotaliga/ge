// LAB `hub-api`: a scripted evening as the hub's JSON API answers it (docs/hub.md § The JSON API).
// Thirty game servers on the simulated network beat to one master and answer status asks with
// records a script writes: five modes, players coming and going every twenty seconds, some
// servers filled with bots, matches cycling through lobby, countdown, play and intermission, one
// stuck on its score screen, and a few naming broadcasts on three TV nodes. The engine's hub
// (engine/hub: the poller, the interest rule, the API) reads them. It answers whether the hub's
// view follows the evening (the humans it sees against the script's, each second), whether a
// server with humans is read every few seconds, whether the API's answers are the truth at the end
// (counts, filters, streams, the order of the cards: full human matches first, the stale score
// screen last) and what the answers weigh. The humans seen are charted against the truth, and the
// cards drawn as the Servers page will show them.
//
// ge:summary Lab: a scripted evening through the hub's poller and JSON API; freshness, cards
// ge:keywords lab hub api json poller interest cards freshness filter streams evening digest
// ge:skill net labs
// ge:test tests/hub_api_test.cpp
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "core/json_read.hpp"
#include "hub/api.hpp"
#include "hub/poller.hpp"
#include "hub/spec.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/master.hpp"
#include "net/packet.hpp"
#include "net/status.hpp"
#include "ui/draw.hpp"
#include "ui/font.hpp"
#include "ui/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace gh = ge::hub;
namespace gu = ge::ui;

const gn::Address kMaster = gn::sim_address(1, gn::kMasterPort);
constexpr int64_t kTick = 50000;  // 20 Hz: the nodes, the master and the hub each turn
constexpr int64_t kMatchUs = 150000000, kSlotUs = 20000000;

struct ModeDef {
  const char* name;
  int max;
};
constexpr ModeDef kModes[] = {{"4on4", 8}, {"2on2", 4}, {"1on1", 2}, {"ffa", 12}, {"ctf", 16}};

uint64_t mix(uint64_t x) {
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
  return x ^ (x >> 31);
}

struct Scripted {
  gn::Transport port;
  gn::MasterBeat beat;
  int index = 0;
  uint32_t seed = 0;
  bool tv = false;
};

// The record a scripted server says at `t_us`: the script is a pure function of the seed, the
// server and the time.
gn::Status script(const Scripted& s, uint32_t protocol, int tvs, int64_t t_us) {
  gn::Status st;
  st.protocol = protocol;
  st.version = 1;
  if (s.tv) {
    st.kind = gn::NodeKind::tv;
    st.name = "TV " + std::to_string(s.index + 1);
    return st;
  }
  const int i = s.index;
  const ModeDef& m = kModes[i % 5];
  st.kind = gn::NodeKind::game;
  st.name = "Arena " + std::to_string(i + 1);
  st.mode = m.name;
  st.map = i % 3 == 0 ? "tower" : i % 3 == 1 ? "docks" : "vault";
  st.max_players = m.max;
  st.max_spectators = 4;
  const uint64_t base = mix(uint64_t{s.seed} * 1000003u + static_cast<uint64_t>(i));
  const int64_t slot = t_us / kSlotUs;
  const uint64_t r = mix(base ^ static_cast<uint64_t>(slot) * 0x9e3779b97f4a7c15ull);
  const int popularity = static_cast<int>(base % 101);  // percent
  const int swing = 50 + static_cast<int>(r % 71);      // 50..120 percent
  int humans = m.max * popularity * swing / 10000;
  if (i % 7 == 6) humans = m.max;  // always full
  humans = std::min(humans, m.max);
  const int bots = i % 4 == 3 ? m.max - humans : 0;
  // The match's phase: lobby 15 s, countdown 5 s, play 120 s, intermission 10 s; server 1 sits
  // on its score screen from 10 s on.
  const int64_t local = (t_us + int64_t{i} * 13000000) % kMatchUs;
  const bool stuck = i == 0 && t_us >= 10000000;
  if (!stuck && local < 15000000) {
    st.phase = gn::StatusPhase::lobby;
  } else if (!stuck && local < 20000000) {
    st.phase = gn::StatusPhase::countdown;
  } else if (!stuck && local < 140000000) {
    st.phase = gn::StatusPhase::playing;
    st.time_left_s = static_cast<int32_t>((140000000 - local) / 1000000);
  } else {
    st.phase = gn::StatusPhase::intermission;
  }
  const int64_t played = st.phase == gn::StatusPhase::playing ? (local - 20000000) / 7000000 : 0;
  for (int k = 0; k < humans + bots; k++) {
    gn::StatusPlayer p;
    p.name = (k < humans ? "player" : "bot") + std::to_string(i * 20 + k);
    p.team = m.max >= 4 && i % 5 != 3 ? (k % 2 ? "blue" : "red") : "";
    p.score = static_cast<int32_t>(played * (1 + (k * 7 + i) % 4));
    p.ping_ms = static_cast<uint16_t>(15 + (k * 13 + i) % 60);
    p.minutes = static_cast<uint16_t>(t_us / 60000000);
    p.flags = k < humans ? 0 : gn::kPlayerBot;
    st.players.push_back(p);
  }
  if (i % 6 == 0 && tvs > 0) {
    gn::StatusStream str;
    str.node = gn::sim_address(300 + static_cast<uint32_t>(i / 6 % tvs), 28000);
    str.stream = static_cast<uint32_t>(i + 1);
    str.delay_s = 10;
    str.viewers = static_cast<uint16_t>(humans * 3);
    st.streams.push_back(str);
  }
  return st;
}

int humans_of(const gn::Status& s) { return gh::humans(s); }

// THE CARDS: the first rows of /v1/servers, as the Servers page will draw them.
gl::Picture cards_picture(const ge::core::FlatJson& j, int count, double at_s) {
  constexpr int kW = 1280;
  const int rows = std::min(count, 16);
  const int kH = 200 + 56 * rows;
  gu::Image img = gu::make_image(kW, kH, {0.06f, 0.08f, 0.07f, 1});
  gu::DrawList dl;
  dl.begin(kW, kH);
  const gu::Font& bold = gu::ui_font(gu::Weight::semibold);
  const gu::Font& body = gu::ui_font(gu::Weight::medium);
  const gu::Color ink{0.92f, 0.96f, 0.94f, 1}, dim{0.55f, 0.66f, 0.62f, 1};
  const gu::Color hot{1.0f, 0.70f, 0.30f, 1}, cool{0.45f, 0.60f, 0.70f, 1};
  const float x0 = 32, y0 = 32;
  dl.rect({x0, y0, kW - 64, static_cast<float>(kH) - 64}, {0.10f, 0.13f, 0.12f, 1}, 14);
  char head[160];
  std::snprintf(head, sizeof head, "GET /v1/servers  ·  %d servers by interest  ·  at %.0f s",
                count, at_s);
  dl.text(bold, 30, x0 + 24, y0 + 46, "SERVERS", ink);
  dl.text(body, 20, x0 + 24, y0 + 76, head, dim);
  const float cols[7] = {x0 + 24, x0 + 250, x0 + 360, x0 + 470, x0 + 640, x0 + 830, x0 + 1010};
  const char* heads[7] = {"NAME", "MODE", "MAP", "PLAYERS", "PHASE", "STREAM", "INTEREST"};
  for (int c = 0; c < 7; c++) dl.text(bold, 16, cols[c], y0 + 118, heads[c], dim);
  float y = y0 + 160;
  for (int r = 0; r < rows; r++) {
    const std::string k = "servers." + std::to_string(r) + ".";
    auto v = [&](const std::string& f) {
      auto it = j.find(k + f);
      return it == j.end() ? std::string() : it->second.text;
    };
    if (r % 2 == 0) dl.rect({x0 + 16, y - 32, kW - 96, 52}, {0.13f, 0.17f, 0.15f, 1}, 8);
    const bool humans_on = v("humans") != "0";
    dl.text(bold, 20, cols[0], y, v("name"), humans_on ? ink : dim);
    dl.text(body, 20, cols[1], y, v("mode"), ink);
    dl.text(body, 20, cols[2], y, v("map"), dim);
    std::string seats = v("humans") + " / " + v("max_players");
    if (v("bots") != "0") seats += "  +" + v("bots") + " bots";
    dl.text(body, 20, cols[3], y, seats, humans_on ? hot : cool);
    dl.text(body, 20, cols[4], y, v("phase"), dim);
    dl.text(body, 20, cols[5], y, j.count(k + "streams.0.node") ? "watch" : "-",
            j.count(k + "streams.0.node") ? hot : dim);
    dl.text(body, 20, cols[6], y, v("interest"), dim);
    y += 56;
  }
  gu::render(dl, img);
  gl::Picture pic;
  pic.width = img.width;
  pic.height = img.height;
  pic.rgba = std::move(img.rgba);
  return pic;
}

uint64_t fold(uint64_t h, uint64_t v) { return (h ^ v) * 1099511628211ull; }
uint64_t fold_text(uint64_t h, const std::string& s) {
  for (unsigned char c : s) h = fold(h, c);
  return h;
}

void run(gl::Lab& lab) {
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const int servers = static_cast<int>(lab.arg_num("servers"));
  const int tvs = 3;
  const double seconds = lab.arg_num("seconds");
  gn::LinkConditions link;
  if (!gn::conditions_preset(lab.arg("preset"), link)) {
    lab.fail("unknown preset '" + lab.arg("preset") + "'",
             "Pick from lan, broadband, wifi, mobile, bad, awful (engine/net/conditions.cpp); "
             "skill: net.");
    return;
  }
  gn::SimNetwork net = gn::make_sim_network(seed, link);
  gh::HubSpec spec;
  spec.game = "Lab Arena";
  spec.id = "lab_arena";
  spec.protocol = gn::protocol_id("hub_api_lab", 1);
  spec.modes = {{"4on4", 9}, {"2on2", 8}, {"1on1", 7}, {"ctf", 6}, {"ffa", 3}};
  spec.score = "frags";

  gn::Transport master_port;
  gn::open_sim_transport(master_port, net, kMaster);
  gn::Master master;
  gn::master_init(master, gn::MasterConfig{}, master_port);

  std::vector<std::unique_ptr<Scripted>> nodes;
  for (int i = 0; i < servers + tvs; i++) {
    auto s = std::make_unique<Scripted>();
    s->tv = i >= servers;
    s->index = s->tv ? i - servers : i;
    s->seed = seed;
    const gn::Address at = s->tv ? gn::sim_address(300 + static_cast<uint32_t>(s->index), 28000)
                                 : gn::sim_address(100 + static_cast<uint32_t>(i), 27500);
    gn::open_sim_transport(s->port, net, at);
    gn::MasterBeatConfig bc;
    bc.masters = {kMaster};
    bc.protocol = spec.protocol;
    bc.kind = s->tv ? gn::NodeKind::tv : gn::NodeKind::game;
    bc.version = 1;
    bc.seed = uint64_t{seed} * 7919u + static_cast<uint64_t>(i);
    gn::master_beat_init(s->beat, bc);
    nodes.push_back(std::move(s));
  }

  gn::Transport hub_port;
  gn::open_sim_transport(hub_port, net, gn::sim_address(2, 40000));
  gh::PollerConfig pc;
  pc.masters = {kMaster};
  pc.protocol = spec.protocol;
  pc.list_us = 10000000;
  pc.seed = seed;
  gh::Poller hub;
  gh::poller_init(hub, pc, 0);

  gl::Trace t_truth{"humans playing (the script)", {}, {}}, t_seen{"humans the hub shows", {}, {}};
  gl::Trace t_busy{"servers with humans (hub)", {}, {}};
  uint64_t digest = 1469598103934665603ull;
  const int64_t end_us = static_cast<int64_t>(seconds * 1e6);
  double seen_sum = 0, truth_sum = 0, worst_gap = 0;
  int64_t worst_busy_age_us = 0;
  uint8_t buf[gn::kMaxDatagram], out[gn::kMaxDatagram];
  for (int64_t now = 0; now <= end_us; now += kTick) {
    gn::sim_advance(net, now);
    gn::master_update(master, now);
    for (auto& s : nodes) {
      gn::master_beat_update(s->beat, s->port, now);
      gn::Address from;
      int n = 0;
      while ((n = gn::transport_receive(s->port, buf, sizeof buf, from, now)) >= 0) {
        if (gn::master_beat_read(s->beat, buf, static_cast<size_t>(n), from, now)) continue;
        if (!gn::is_status_ask(buf, static_cast<size_t>(n))) continue;
        const gn::Status st = script(*s, spec.protocol, tvs, now);
        const size_t a =
            gn::status_answer(buf, static_cast<size_t>(n), from, spec.protocol,
                              0x5eed ^ gn::address_hash(s->port.local), now, st, out, sizeof out);
        if (a) gn::transport_send(s->port, from, out, a, now);
      }
    }
    gh::poller_pump(hub, hub_port, now);

    if (now % 1000000 == 0) {
      const double t = static_cast<double>(now) / 1e6;
      int truth = 0, seen = 0, busy = 0;
      for (const auto& s : nodes) {
        if (s->tv) continue;
        truth += humans_of(script(*s, spec.protocol, tvs, now));
      }
      for (const gh::HubNode& n : hub.nodes) {
        if (!n.have || n.kind != gn::NodeKind::game) continue;
        const int h = humans_of(n.status);
        seen += h;
        busy += h > 0 ? 1 : 0;
        // After the first minute, a server with humans on it is never older than its busy period
        // and a retry.
        if (h > 0 && now > 60000000)
          worst_busy_age_us = std::max(worst_busy_age_us, now - n.heard_us);
      }
      t_truth.x.push_back(t);
      t_truth.y.push_back(truth);
      t_seen.x.push_back(t);
      t_seen.y.push_back(seen);
      t_busy.x.push_back(t);
      t_busy.y.push_back(busy);
      if (now > 60000000) {
        seen_sum += seen;
        truth_sum += truth;
        worst_gap =
            std::max(worst_gap, static_cast<double>(truth > seen ? truth - seen : seen - truth));
      }
      digest = fold(fold(digest, static_cast<uint64_t>(seen)), static_cast<uint64_t>(busy));
    }
  }

  // The API at the end, against the script's truth.
  struct Ask {
    std::string path;
    int code = 0;
    size_t bytes = 0;
    std::string count;
  };
  std::vector<Ask> asks;
  ge::core::FlatJson servers_json;
  int api_faults = 0;
  auto ask = [&](const std::string& path, ge::core::FlatJson* keep) {
    const gh::Answer a = gh::api_answer(hub, spec, path, end_us);
    ge::core::FlatJson j;
    std::string err;
    if (!ge::core::read_flat(a.body, j, err, true)) {
      api_faults++;
      lab.fail(path + " answered JSON that does not parse: " + err,
               "engine/hub/api.cpp writes it with core/json_write.hpp; skill: net.");
    }
    asks.push_back({path, a.code, a.body.size(), j.count("count") ? j["count"].text : "-"});
    digest = fold_text(digest, a.body);
    if (keep) *keep = j;
    return j;
  };
  ask("/v1", nullptr);
  ask("/v1/servers", &servers_json);
  int truth_servers = 0, truth_streams = 0, truth_mode[5] = {0, 0, 0, 0, 0};
  for (const auto& s : nodes) {
    if (s->tv) continue;
    const gn::Status st = script(*s, spec.protocol, tvs, end_us);
    truth_servers++;
    truth_streams += static_cast<int>(st.streams.size());
    truth_mode[s->index % 5]++;
  }
  for (int m = 0; m < 5; m++) {
    const ge::core::FlatJson j = ask(std::string("/v1/servers?mode=") + kModes[m].name, nullptr);
    if (j.count("count") && j.at("count").text != std::to_string(truth_mode[m])) {
      lab.fail(std::string("/v1/servers?mode=") + kModes[m].name + " lists " + j.at("count").text +
                   " servers, not " + std::to_string(truth_mode[m]),
               "the mode filter in engine/hub/api.cpp; skill: net.");
    }
  }
  ask("/v1/servers?has_players=1", nullptr);
  const ge::core::FlatJson streams = ask("/v1/streams", nullptr);
  ask("/v1/nodes", nullptr);
  ask("/v1/masters", nullptr);
  ask("/v1/servers/" + gn::to_string(nodes[3]->port.local), nullptr);
  ask("/v1/servers/sim:999:1", nullptr);

  const int listed = servers_json.count("count") ? std::stoi(servers_json["count"].text) : -1;
  const std::string first =
      servers_json.count("servers.0.name") ? servers_json["servers.0.name"].text : "";
  int stale_rank = -1;
  for (int r = 0; r < listed; r++) {
    const std::string k = "servers." + std::to_string(r) + ".";
    if (servers_json[k + "name"].text == "Arena 1") stale_rank = r;
  }

  lab.metric("listed_servers", listed, "servers");
  lab.metric("streams", streams.count("count") ? std::stod(streams.at("count").text) : -1,
             "streams");
  lab.metric("humans_seen_share", truth_sum > 0 ? seen_sum / truth_sum : 0, "of the script's");
  lab.metric("worst_humans_gap", worst_gap, "humans");
  lab.metric("worst_busy_age", static_cast<double>(worst_busy_age_us) / 1e6, "s");
  lab.metric("record_changes", static_cast<double>(hub.stats.changes), "records");
  lab.metric("hub_asks", static_cast<double>(hub.stats.asks), "datagrams");
  lab.metric("hub_records", static_cast<double>(hub.stats.records), "records");
  lab.metric("servers_answer", static_cast<double>(asks[1].bytes), "bytes");
  lab.metric("stale_card_rank", stale_rank, "of " + std::to_string(listed));

  gl::LabTable api{"the API at the end", {"GET", "code", "bytes", "count"}, {}};
  for (const Ask& a : asks) {
    api.rows.push_back({a.path, std::to_string(a.code), std::to_string(a.bytes), a.count});
  }
  lab.table(std::move(api));
  gl::LabTable top{
      "the first cards", {"rank", "name", "mode", "humans", "bots", "phase", "interest"}, {}};
  for (int r = 0; r < std::min(listed, 8); r++) {
    const std::string k = "servers." + std::to_string(r) + ".";
    top.rows.push_back(
        {std::to_string(r + 1), servers_json[k + "name"].text, servers_json[k + "mode"].text,
         servers_json[k + "humans"].text + "/" + servers_json[k + "max_players"].text,
         servers_json[k + "bots"].text, servers_json[k + "phase"].text,
         servers_json[k + "interest"].text});
  }
  lab.table(std::move(top));
  char d[20];
  std::snprintf(d, sizeof d, "%016llx", static_cast<unsigned long long>(digest));
  lab.table({"the evening's digest", {"what", "value"}, {{"trace and answers", d}}});

  if (listed != truth_servers) {
    lab.fail("the API lists " + std::to_string(listed) + " servers, not " +
                 std::to_string(truth_servers),
             "every listed game node should have answered (engine/hub/poller.cpp); skill: net.");
  }
  if (streams.count("count") && streams.at("count").text != std::to_string(truth_streams)) {
    lab.fail("/v1/streams lists " + streams.at("count").text + " streams, not " +
                 std::to_string(truth_streams),
             "streams come from each record (engine/hub/api.cpp); skill: net.");
  }
  if (seconds * 1e6 > 10000000 + gh::kStaleIntermissionUs && stale_rank != listed - 1) {
    // Arena 1 has sat on its score screen since 10 s: past the stale limit it is the last card.
    lab.fail("the stale score screen is card " + std::to_string(stale_rank + 1) + " of " +
                 std::to_string(listed) + ", not the last",
             "the interest rule drops it after kStaleIntermissionUs (engine/hub/spec.cpp); "
             "skill: net.");
  }
  if (worst_busy_age_us > pc.busy_us + pc.retry_us * pc.tries + 1000000) {
    lab.fail("a server with humans went " + std::to_string(worst_busy_age_us / 1000000) +
                 " s without a fresh record",
             "busy_us in engine/hub/poller.hpp; skill: net.");
  }
  if (api_faults == 0 && first.empty()) {
    lab.fail("/v1/servers has no first card", "skill: net.");
  }

  gl::LineChart line;
  line.frame = {"HUMANS THE HUB SHOWS AGAINST THE SCRIPT",
                "seed " + lab.arg("seed") + ", " + std::to_string(servers) + " servers", "t (s)",
                "humans / servers"};
  line.spikes = false;
  line.traces = {t_truth, t_seen, t_busy};
  const gl::Chart chart = gl::line_chart(line);
  lab.shot({"humans",
            chart.title,
            "Each second: the humans the script has on every server, the humans the hub's index "
            "shows (the poller's last record of each), and how many servers the hub shows with "
            "humans. Servers with humans are read every 4 s, the rest every 30 s.",
            "the hub's line following the script's a few seconds behind, never drifting; the "
            "first seconds empty until the master's list and the cookie rounds come.",
            {{"servers", lab.arg("servers")}, {"seed", lab.arg("seed")}}},
           gl::to_picture(chart.plot));
  lab.shot({"cards",
            "THE SERVERS CARDS FROM /v1/servers",
            "The first rows of GET /v1/servers at the end of the evening, as the Servers page will "
            "draw them: the interest rule's order, players over slots with bots apart, the phase, "
            "a watch link where a record names a broadcast.",
            "full human matches on top, then by humans and the mode's weight; bot-filled servers "
            "below the human ones; Arena 1 (stuck on its score screen) not among the first.",
            {{"servers", lab.arg("servers")}, {"seed", lab.arg("seed")}}},
           cards_picture(servers_json, listed, seconds));
}

const gl::LabSpec kSpec{
    "hub-api",
    gl::LabKind::sim,
    "engine",
    "engine/hub (the poller, the interest rule, the JSON API)",
    "net",
    "Does the hub's index follow a scripted evening of servers, and do its JSON API's answers "
    "(counts, filters, streams, the order of the cards) tell the truth?",
    {
        {"servers", "30", "scripted game servers (three TV nodes beside them)"},
        {"seconds", "400", "the evening's length, s"},
        {"preset", "broadband", "the network's conditions (engine/net/conditions.cpp)"},
        {"seed", "1", "the network's and the script's stream"},
    },
    "--servers=12 --seconds=330",
    run,
};
GE_LAB(kSpec);

}  // namespace
