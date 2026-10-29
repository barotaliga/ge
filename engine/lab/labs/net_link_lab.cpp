// LAB `net-link`: a host and its clients trading fake traffic (net/traffic.hpp) over the simulated
// network, once under each condition preset, from a cable across a room to a phone on a train.
// It answers whether the link layer keeps its word everywhere (every reliable message once and in
// order, sequenced inputs never going back) and what each condition costs: the round trip and the
// loss the links measured beside the configured ones, how late each kind of message arrived, what
// the host sent and received a second, and how many messages were sent again. The round trip and
// the loss are charted over the run, the reliable latencies drawn as boxes, and one preset's whole
// session is attached as a pcap (`<preset>.pcap`) with its first packets decoded in the notes.
// `--transport=tcp` makes every link a stream (nothing lost, a drop a resend's wait late, the
// packets behind it waiting); a mix with a bulk (`--mix=download`) adds a table of the big
// messages, each sent in pieces and checked whole; `--budget` caps what every connection sends.
// `--secure=1` runs each preset again with connect tokens and sealed packets (M17i), the host's
// flood guards on, and attackers at it: forged and stolen tokens, replayed and forged payloads, a
// flood of requests from 64 addresses and a client sending fifty times its share; the
// security table says what each attack sent and what the host refused, and holds the honest
// clients' round trip, loss and deliveries to the same preset's run without them.
// Everything runs on a virtual clock off one seed, so the lab is a determinism test too.
//
// ge:summary Lab: fake traffic over each network preset: delivery, RTT, loss, latency, bandwidth
// ge:keywords lab net network link latency rtt ping loss packet-loss jitter bandwidth resend
// ge:keywords fake-traffic preset pcap capture chart reliable traffic-mix tcp stream big-message
// ge:keywords fragmentation bulk download budget security secure token attack replay flood
// ge:skill net labs
// ge:test tests/net_traffic_test.cpp
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/stats.hpp"
#include "net/capture.hpp"
#include "net/packet.hpp"
#include "net/traffic.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

std::string share(uint64_t part, uint64_t whole) {
  return whole ? fmt(100.0 * static_cast<double>(part) / static_cast<double>(whole), 1) + "%" : "-";
}

std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= s.size()) {
    const size_t comma = s.find(',', at);
    const std::string part =
        s.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    if (!part.empty()) out.push_back(part);
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return out;
}

void shoot(gl::Lab& lab, const std::string& id, const gl::Chart& chart, const std::string& caption,
           const std::string& look_for) {
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id,
            chart.title,
            caption,
            look_for,
            {{"presets", lab.arg("presets")},
             {"clients", lab.arg("clients")},
             {"seconds", lab.arg("seconds")},
             {"seed", lab.arg("seed")}}},
           gl::to_picture(chart.plot));
}

void run(gl::Lab& lab) {
  const std::vector<std::string> presets = split(lab.arg("presets"));
  const std::string capture_preset = lab.arg("capture");
  gl::LabTable table{
      "fake traffic, " + lab.arg("clients") + " clients for " + lab.arg("seconds") +
          " s, each preset; the " + lab.arg("mix") + " mix",
      {"preset", "conditions", "joined", "rtt ms", "want", "jitter", "loss", "want", "events",
       "order", "event p50", "p95", "snaps", "inputs", "host out kbps", "in", "resends"},
      {}};
  gl::LineChart rtt_chart{
      {"ROUND TRIP THE HOST MEASURED, EACH PRESET", "mean over its peers", "t (s)", "rtt (ms)"},
      {},
      {},
      {},
      {},
      true,
      false,
      false};
  gl::LineChart loss_chart{
      {"PACKET LOSS THE HOST MEASURED, EACH PRESET", "mean over its peers", "t (s)", "loss (%)"},
      {},
      {},
      {},
      {},
      false,
      false,
      false};
  std::vector<gl::Sample> event_latency;
  gl::LabTable bulk_table{"big messages (the bulk), sent in pieces, each joined whole",
                          {"preset", "sent", "whole", "late", "broken", "order", "refused",
                           "p50 ms", "p95 ms", "max ms"},
                          {}};
  const bool secure = lab.arg_num("secure") != 0;
  gl::LabTable sec_table{
      "secure, under attack (" + lab.arg("clients") +
          " honest clients; attackers each second: 20 forged tokens, 20 stolen ones, 50 replayed "
          "payloads, 50 forged ones, 4000 requests from 64 addresses, and one client sending "
          "3000 packets)",
      {"preset", "joined", "rtt ms", "plain", "loss", "plain", "events", "host out kbps", "plain",
       "bad tokens", "stolen refused", "replays refused", "forgeries refused", "throttled",
       "flooded", "answered"},
      {}};
  gl::LineChart attacked_chart{
      {"ROUND TRIP UNDER ATTACK, LESS THE SAME PRESET'S WITHOUT",
       "the host's mean over its honest peers, secure and attacked minus plain", "t (s)",
       "difference (ms)"},
      {},
      {},
      {},
      {},
      false,
      false,
      false};
  const std::string transport = lab.arg("transport");
  if (transport != "udp" && transport != "tcp") {
    lab.fail("unknown transport '" + transport + "'",
             "Pass --transport=udp (datagrams) or --transport=tcp (every link a stream); "
             "skill: net.");
    return;
  }

  for (const std::string& name : presets) {
    gn::TrafficPlan plan;
    if (!gn::conditions_preset(name, plan.conditions)) {
      lab.fail("unknown preset '" + name + "'",
               "Pick from lan, broadband, wifi, mobile, bad, awful (engine/net/conditions.cpp); "
               "skill: net.");
      continue;
    }
    if (!gn::traffic_mix(lab.arg("mix"), plan.mix)) {
      lab.fail("unknown traffic mix '" + lab.arg("mix") + "'",
               "Pick from racing, action, rpg, world, lockstep, download "
               "(engine/net/traffic.cpp); skill: net.");
      return;
    }
    plan.conditions.stream = transport == "tcp";
    plan.budget_kbps = lab.arg_num("budget");
    plan.clients = static_cast<int>(lab.arg_num("clients"));
    plan.seconds = lab.arg_num("seconds");
    plan.drain_seconds = lab.arg_num("drain");
    plan.tick_hz = lab.arg_num("tick-hz");
    plan.seed = static_cast<uint32_t>(lab.arg_num("seed"));
    plan.capture = name == capture_preset;
    const gn::TrafficReport r = gn::run_traffic(plan);
    const gn::LinkConditions& c = plan.conditions;

    const auto ev_sent = r.events_up.sent + r.events_down.sent;
    const auto ev_got = r.events_up.delivered + r.events_down.delivered;
    const auto ev_bad = r.events_up.out_of_order + r.events_down.out_of_order +
                        r.events_up.duplicated + r.events_down.duplicated;
    std::vector<double> ev_lat = r.events_up.latency_ms;
    ev_lat.insert(ev_lat.end(), r.events_down.latency_ms.begin(), r.events_down.latency_ms.end());
    const gl::Summary ev = gl::summarize(ev_lat);
    // A node reads its socket once a tick, so each way a packet sent on a tick is read on the
    // first tick after it lands: the round trip is twice the latency rounded up to whole ticks
    // (no jitter) to twice the latency and the jitter rounded up (the most jitter).
    const double tick = 1000.0 / plan.tick_hz;
    const auto ticks_up = [tick](double v) { return std::ceil(v / tick - 1e-9) * tick; };
    const double want_lo = 2 * ticks_up(c.latency_ms),
                 want_hi = 2 * ticks_up(c.latency_ms + c.jitter_ms);
    table.rows.push_back(
        {name, gn::describe_conditions(c), std::to_string(r.connected), fmt(r.rtt_ms, 1),
         fmt(want_lo, 0) + "-" + fmt(want_hi, 0), fmt(r.jitter_ms, 1),
         fmt(100 * r.loss_total, 2) + "%", fmt(100 * c.loss, 2) + "%",
         std::to_string(ev_got) + "/" + std::to_string(ev_sent),
         ev_bad ? std::to_string(ev_bad) + " bad" : "ok", fmt(ev.p50, 1), fmt(ev.p95, 1),
         share(r.snapshots.delivered, r.snapshots.sent), share(r.inputs.delivered, r.inputs.sent),
         fmt(r.host_send_kbps, 0), fmt(r.host_recv_kbps, 0), std::to_string(r.resends)});
    const std::string k = name + ".";
    lab.metric(k + "rtt-ms", r.rtt_ms, "ms");
    lab.metric(k + "jitter-ms", r.jitter_ms, "ms");
    lab.metric(k + "loss-measured", 100 * r.loss_total, "%");
    lab.metric(k + "loss-recent", 100 * r.loss, "%");
    lab.metric(k + "events-delivered", static_cast<double>(ev_got), "count");
    lab.metric(k + "events-sent", static_cast<double>(ev_sent), "count");
    lab.metric(k + "event-p50-ms", ev.p50, "ms");
    lab.metric(k + "event-p95-ms", ev.p95, "ms");
    lab.metric(k + "snapshots-delivered",
               r.snapshots.sent ? 100.0 * static_cast<double>(r.snapshots.delivered) /
                                      static_cast<double>(r.snapshots.sent)
                                : 0,
               "%");
    lab.metric(k + "host-send-kbps", r.host_send_kbps, "kbit/s");
    lab.metric(k + "resends", static_cast<double>(r.resends), "count");
    lab.metric(k + "wire-lost", static_cast<double>(r.wire.lost + r.wire.queue_dropped), "count");

    std::vector<double> loss_pct;
    loss_pct.reserve(r.series_loss.size());
    for (const double l : r.series_loss) loss_pct.push_back(100 * l);
    rtt_chart.traces.push_back({name, r.t_s, r.series_rtt_ms});
    loss_chart.traces.push_back({name, r.t_s, loss_pct});
    event_latency.push_back({name, ev_lat});

    if (r.connected != plan.clients || r.dropped) {
      lab.fail(name + ": " + std::to_string(r.connected) + " of " + std::to_string(plan.clients) +
                   " clients connected and " + std::to_string(r.dropped) + " dropped",
               "Trace the handshake and timeouts in engine/net/node.cpp under this preset, then "
               "run tests/net_node_test.cpp; skill: net.");
    }
    if (ev_got != ev_sent || ev_bad) {
      lab.fail(name + ": " + std::to_string(ev_got) + " of " + std::to_string(ev_sent) +
                   " reliable events arrived, " + std::to_string(ev_bad) + " out of order or twice",
               "Read the reliable channel in engine/net/connection.cpp (resends, the held window), "
               "then run tests/net_connection_test.cpp; skill: net.");
    }
    if (secure) {
      gn::TrafficPlan sp = plan;
      sp.capture = false;
      sp.secure = true;
      sp.guards = gn::default_guards();
      const double x = lab.arg_num("attack");
      sp.attack.forged_tokens_per_s = 20 * x;
      sp.attack.stolen_tokens_per_s = 20 * x;
      sp.attack.replays_per_s = 50 * x;
      sp.attack.forgeries_per_s = 50 * x;
      sp.attack.flood_per_s = 4000 * x;
      sp.attack.flood_addresses = 64;
      sp.attack.flooders = x > 0 ? 1 : 0;
      sp.attack.flooder_packets_per_s = 3000 * x;
      const gn::TrafficReport s = gn::run_traffic(sp);
      const gn::NodeStats& st = s.host_stats;
      const auto s_sent = s.events_up.sent + s.events_down.sent;
      const auto s_got = s.events_up.delivered + s.events_down.delivered;
      const auto s_bad = s.events_up.out_of_order + s.events_down.out_of_order +
                         s.events_up.duplicated + s.events_down.duplicated;
      const auto pair = [](uint64_t refused, uint64_t sent) {
        return std::to_string(refused) + "/" + std::to_string(sent);
      };
      sec_table.rows.push_back(
          {name, std::to_string(s.connected), fmt(s.rtt_ms, 1), fmt(r.rtt_ms, 1),
           fmt(100 * s.loss_total, 2) + "%", fmt(100 * r.loss_total, 2) + "%",
           s_got == s_sent && !s_bad ? "ok" : std::to_string(s_got) + "/" + std::to_string(s_sent),
           fmt(s.host_send_kbps, 0), fmt(r.host_send_kbps, 0),
           pair(st.bad_tokens, s.attack.forged_tokens + s.attack.flood),
           pair(st.replayed_tokens, s.attack.stolen_tokens),
           pair(st.replayed, s.attack.replays), pair(st.forged, s.attack.forgeries),
           std::to_string(st.throttled), pair(st.flooded, s.attack.flooder_packets),
           std::to_string(s.attack.answered)});
      lab.metric(k + "secure-rtt-ms", s.rtt_ms, "ms");
      lab.metric(k + "secure-loss", 100 * s.loss_total, "%");
      lab.metric(k + "secure-host-send-kbps", s.host_send_kbps, "kbit/s");
      lab.metric(k + "refused-bad-tokens", static_cast<double>(st.bad_tokens), "count");
      lab.metric(k + "refused-stolen", static_cast<double>(st.replayed_tokens), "count");
      lab.metric(k + "refused-replays", static_cast<double>(st.replayed), "count");
      lab.metric(k + "refused-forgeries", static_cast<double>(st.forged), "count");
      lab.metric(k + "throttled", static_cast<double>(st.throttled), "count");
      lab.metric(k + "flooded", static_cast<double>(st.flooded), "count");
      lab.metric(k + "answered", static_cast<double>(s.attack.answered), "count");
      std::vector<double> diff;
      for (size_t i = 0; i < s.series_rtt_ms.size() && i < r.series_rtt_ms.size(); i++) {
        diff.push_back(s.series_rtt_ms[i] - r.series_rtt_ms[i]);
      }
      attacked_chart.traces.push_back(
          {name, std::vector<double>(s.t_s.begin(), s.t_s.begin() + static_cast<long>(diff.size())),
           diff});
      // THE ROW'S BOUNDS: no attacker is answered or let in, every honest client joins and gets
      // every event once and in order, and the honest links measure what the same preset does
      // without the attack: the round trip within two ticks and a fifth, the loss within two
      // points and half the preset's own (a short run's loss is a few bursts: the slack is its
      // noise, not the attack's).
      if (s.attack.answered || s.connected != sp.clients || s.dropped ||
          s.attack.flooders_connected != sp.attack.flooders) {
        lab.fail(name + " secure: " + std::to_string(s.attack.answered) +
                     " answers to attackers, " + std::to_string(s.connected) + " of " +
                     std::to_string(sp.clients) + " honest clients joined, " +
                     std::to_string(s.dropped) + " dropped",
                 "Read the token checks and the guards in engine/net/node.cpp (node_update), then "
                 "run tests/net_node_secure_test.cpp; skill: net.");
      }
      if (s_got != s_sent || s_bad) {
        lab.fail(name + " secure: " + std::to_string(s_got) + " of " + std::to_string(s_sent) +
                     " reliable events arrived, " + std::to_string(s_bad) + " out of order or twice",
                 "Read the sealing and the replay window in engine/net/node.cpp, then run "
                 "tests/net_node_secure_test.cpp; skill: net.");
      }
      const double rtt_slack = 2 * tick + 0.2 * r.rtt_ms;
      if (std::fabs(s.rtt_ms - r.rtt_ms) > rtt_slack ||
          std::fabs(s.loss_total - r.loss_total) > 0.02 + 0.5 * c.loss) {
        lab.fail(name + " secure: the attack moved the honest links: rtt " + fmt(s.rtt_ms, 1) +
                     " ms against " + fmt(r.rtt_ms, 1) + ", loss " + fmt(100 * s.loss_total, 2) +
                     "% against " + fmt(100 * r.loss_total, 2) + "%",
                 "Check what the guards let through (NodeStats::throttled, flooded) and the "
                 "rates in default_guards (engine/net/node.cpp); skill: net.");
      }
    }
    if (plan.mix.bulk_hz > 0) {
      const gl::Summary b = gl::summarize(r.bulk.latency_ms);
      bulk_table.rows.push_back(
          {name, std::to_string(r.bulk.sent), std::to_string(r.bulk.delivered),
           std::to_string(r.bulk.sent - r.bulk.delivered), std::to_string(r.bulk_broken),
           r.bulk.out_of_order ? "bad" : "ok", std::to_string(r.bulk.refused), fmt(b.p50, 1),
           fmt(b.p95, 1), fmt(b.max, 1)});
      lab.metric(k + "bulk-delivered", static_cast<double>(r.bulk.delivered), "count");
      lab.metric(k + "bulk-p50-ms", b.p50, "ms");
      // Late is the link's capacity (still in flight when the run ends); broken or out of order
      // is a bug in the pieces.
      if (r.bulk_broken || r.bulk.out_of_order || r.bulk.duplicated) {
        lab.fail(name + ": " + std::to_string(r.bulk_broken) + " big messages arrived broken, " +
                     std::to_string(r.bulk.out_of_order + r.bulk.duplicated) +
                     " out of order or twice",
                 "Read the pieces' sending and joining in engine/net/connection.cpp (BIG "
                 "MESSAGES), then run tests/net_connection_test.cpp; skill: net.");
      }
    }
    if (plan.capture) {
      lab.attach(name + ".pcap", r.capture.bytes);
      std::vector<gn::CapturedDatagram> back;
      std::string err;
      if (!gn::read_capture(r.capture.bytes.data(), r.capture.bytes.size(), back, &err)) {
        lab.fail(name + std::string(": the capture does not read back: ").append(err),
                 "Run tests/net_capture_test.cpp; skill: net.");
      }
      std::string lines =
          name + ".pcap: " + std::to_string(back.size()) + " datagrams at the host; the first 12:";
      for (size_t i = 0; i < back.size() && i < 12; i++) {
        lines += "\n  " + fmt(static_cast<double>(back[i].at_us) / 1000.0, 1) + " ms " +
                 gn::to_string(back[i].from) + " > " + gn::to_string(back[i].to) + "  " +
                 gn::describe_packet(back[i].data.data(), back[i].data.size());
      }
      lab.note(lines);
      lab.metric(k + "captured", static_cast<double>(back.size()), "count");
    }
  }
  lab.table(std::move(table));
  if (secure) {
    lab.table(std::move(sec_table));
    lab.note(
        "secure: every client connects with a token the matchmaker made (net/token.hpp) and every "
        "packet is sealed with its keys; the host keeps default_guards(). 'plain' is the same "
        "preset without either, above. 'bad tokens' are requests whose token did not open, of the "
        "forged tokens and the flood sent (the rest of the flood was throttled first); 'stolen "
        "refused' are honest tokens sent from another address; 'replays' and 'forgeries' are an "
        "honest client's sealed payloads sent again from its address, as they are and changed; "
        "'flooded' are the loud client's packets dropped over its rate. What the network lost on "
        "the way never reached the host, so a refused count is under what was sent. 'answered' "
        "must be 0: the host never answers an attacker.");
  }
  if (!bulk_table.rows.empty()) {
    lab.table(std::move(bulk_table));
    lab.note(
        "The bulk: each big message is sent in pieces of one packet, at most 8 packets a peer a "
        "flush, behind the inputs, the events and the snapshots. 'whole' arrived joined and "
        "checked byte for byte; 'late' were still in flight when the run ended (the link's "
        "capacity, not a fault); 'broken' or a bad order is a fault. 'refused' counts the ticks a "
        "message waited for room in the reliable window.");
  }
  lab.note(
      "rtt is the host's smoothed round trip at the end of the sending, averaged over its peers; "
      "'want' is the model's range: a node reads its socket once a tick, so each way is the "
      "latency "
      "(and up to the jitter) rounded up to whole ticks; a round trip past it is the queue. "
      "'loss' is the share of the host's packets never acked over the whole sending. 'events' are "
      "reliable messages both ways: every one must arrive, "
      "once and in order. 'snaps' (unreliable) and 'inputs' (sequenced) may be lost; a "
      "sequenced input overtaken by a newer one is dropped, never delivered late.");
  if (!presets.empty()) {
    shoot(lab, "rtt", gl::line_chart(rtt_chart),
          "The host's smoothed round trip over the run, one line a preset, log scale. The sending "
          "stops at the run's length and the links drain after it.",
          "each line settles inside its preset's want range (the table) and stays flat; a climbing "
          "line is "
          "a queue building (the bandwidth too small for the traffic).");
    if (secure) {
      shoot(lab, "attacked", gl::line_chart(attacked_chart),
            "How much longer the host's round trip to its honest clients was, secure with every "
            "attack running, than the same preset plain: one line a preset.",
            "every line hugs zero once the clients have joined, inside the jitter of its preset: "
            "the attack costs the honest links nothing they can measure.");
    }
    shoot(lab, "loss", gl::line_chart(loss_chart),
          "The share of the host's packets never acked, smoothed over about 64 packets, one line "
          "a preset.",
          "each line wanders round its preset's configured loss; bursts show as humps.");
    gl::ChartFrame frame{"RELIABLE EVENTS: ONE-WAY LATENCY, EACH PRESET", "both directions",
                         "latency (ms)", ""};
    shoot(lab, "event-latency", gl::box_chart(frame, event_latency),
          "How late each reliable event arrived, sent to delivered, one box a preset: p25 to p75, "
          "the median, whiskers p5 to p95.",
          "the median sits near the preset's one-way latency; the long right tail is resends "
          "after loss, about a round trip each.");
  }
}

const gl::LabSpec kSpec{
    "net-link",
    gl::LabKind::sim,
    "engine",
    "engine/net (connection, node, sim_network, conditions, traffic)",
    "net",
    "Over each network condition, does every reliable message arrive once and in order, and what "
    "round trip, loss, latency and bandwidth do the links measure?",
    {
        {"presets", "lan,broadband,wifi,mobile,bad,awful", "condition presets, comma-separated"},
        {"clients", "8", "clients connecting to the host (at most the host's 32)"},
        {"mix", "racing", "the traffic's shape: racing, action, rpg, world, lockstep, download"},
        {"transport", "udp", "udp (datagrams) or tcp (every link a stream)"},
        {"budget", "0", "every connection's send budget, kbit/s (0: none)"},
        {"seconds", "10", "how long everyone sends"},
        {"drain", "4", "seconds after, for the reliable messages to finish"},
        {"tick-hz", "60", "how often every node updates and flushes"},
        {"seed", "1", "the network's and the nodes' stream"},
        {"capture", "bad", "the preset whose session is attached as a pcap ('' none)"},
        {"secure", "0",
         "1: each preset again with tokens, sealed packets and the guards, under attack"},
        {"attack", "1", "with --secure: the attackers' rates times this (0: none)"},
    },
    "--clients=4 --seconds=3 --drain=3 --secure=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
