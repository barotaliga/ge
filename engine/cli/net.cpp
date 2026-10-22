// `ge net`'s body (net.hpp): a capture loaded and decoded or analyzed, a fake-traffic session
// run, each answered as text, findings and one JSON result.
//
// ge:summary ge net's body: inspect, stats (series and charts) and sim, and their flags read
// ge:keywords cli net run_net run_net_inspect run_net_stats run_net_sim link-overrides
// ge:skill agent-interface net
// ge:test tests/cli_net_test.cpp
#include "cli/net.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "cli/graph.hpp"
#include "cli/json.hpp"
#include "core/num_text.hpp"
#include "lab/series.hpp"
#include "net/analyze.hpp"
#include "net/capture.hpp"
#include "net/conditions.hpp"
#include "net/packet.hpp"
#include "net/traffic.hpp"

namespace ge::cli {

namespace {

std::string fixed(double v, int digits) {
  return std::isfinite(v) ? core::num::to_fixed(v, digits) : std::string("-");
}

std::string num(double v) { return std::isfinite(v) ? core::num::to_string(v) : "null"; }

std::string pct(double share) { return fixed(100.0 * share, 2) + "%"; }

std::string hex(uint64_t v, int digits) {
  static const char* k = "0123456789abcdef";
  std::string s(static_cast<size_t>(digits), '0');
  for (int i = digits - 1; i >= 0; i--, v >>= 4) s[static_cast<size_t>(i)] = k[v & 15];
  return s;
}

// A table: every column as wide as its widest cell, two spaces between.
std::string table(const std::vector<std::vector<std::string>>& rows) {
  std::vector<size_t> width;
  for (const auto& r : rows) {
    if (width.size() < r.size()) width.resize(r.size(), 0);
    for (size_t i = 0; i < r.size(); i++) width[i] = std::max(width[i], r[i].size());
  }
  std::string out;
  for (const auto& r : rows) {
    std::string line = " ";
    for (size_t i = 0; i < r.size(); i++) {
      line += " " + r[i];
      if (i + 1 < r.size()) line += std::string(width[i] - r[i].size() + 1, ' ');
    }
    out += line + "\n";
  }
  return out;
}

bool parse_double(const std::string& text, double& out) {
  if (text.empty()) return false;
  char* end = nullptr;
  out = std::strtod(text.c_str(), &end);
  return end && *end == '\0' && std::isfinite(out);
}

Outcome failed(const std::string& what, const std::string& where, const std::string& next,
               int exit = 1) {
  Outcome o;
  o.findings.push_back({Severity::error, what, where, next});
  o.exit = exit;
  return o;
}

const char* kCaptureNext =
    "pass a .pcap the engine wrote: a lab's attachment (previews/<lab>/*.pcap), "
    "`ge net sim --capture=FILE`, or a live node's save_capture (skill: multiplayer)";

// "latency=40,jitter=10,loss=2%" onto `c`; the error, or "".
std::string apply_link(const std::string& spec, net::LinkConditions& c) {
  size_t start = 0;
  while (start < spec.size()) {
    size_t comma = spec.find(',', start);
    if (comma == std::string::npos) comma = spec.size();
    const std::string item = spec.substr(start, comma - start);
    start = comma + 1;
    if (item.empty()) continue;
    const size_t eq = item.find('=');
    if (eq == std::string::npos) return "\"" + item + "\" is not key=value";
    const std::string key = item.substr(0, eq);
    std::string value = item.substr(eq + 1);
    const bool percent = !value.empty() && value.back() == '%';
    if (percent) value.pop_back();
    double v = 0;
    if (!parse_double(value, v) || v < 0) return "\"" + item + "\" needs a number of 0 or more";
    const double share = percent ? v / 100.0 : v;
    if (key == "latency") {
      c.latency_ms = v;
    } else if (key == "jitter") {
      c.jitter_ms = v;
    } else if (key == "loss") {
      c.loss = share;
    } else if (key == "burst") {
      c.burst = v < 1 ? 1 : v;
    } else if (key == "duplicate") {
      c.duplicate = share;
    } else if (key == "reorder") {
      c.reorder = share;
    } else if (key == "reorder-ms") {
      c.reorder_ms = v;
    } else if (key == "bandwidth") {
      c.bandwidth_kbps = v;
    } else if (key == "queue") {
      c.queue_ms = v;
    } else {
      return "no link key \"" + key +
             "\" (latency, jitter, loss, burst, duplicate, reorder, reorder-ms, bandwidth, queue)";
    }
    if ((key == "loss" || key == "duplicate" || key == "reorder") && share >= 1) {
      return "\"" + item + "\" is a share: give it under 1, or as a percent (loss=2%)";
    }
  }
  return "";
}

std::vector<std::string> split_commas(const std::string& text) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= text.size()) {
    size_t comma = text.find(',', start);
    if (comma == std::string::npos) comma = text.size();
    if (comma > start) out.push_back(text.substr(start, comma - start));
    start = comma + 1;
  }
  return out;
}

}  // namespace

Outcome run_net_inspect(const NetInspectOptions& opts) {
  std::vector<net::CapturedDatagram> ds;
  std::string error;
  if (!net::load_capture(opts.file, ds, &error)) return failed(error, opts.file, kCaptureNext);
  net::Address peer;
  const bool by_peer = !opts.peer.empty();
  if (by_peer && !net::parse_address(opts.peer, peer)) {
    return failed("--peer=" + opts.peer + " is not an address", "the command line",
                  "give it as the lines print it: 10.0.0.2:4000, [::1]:4000 or sim:3:1", 2);
  }
  int type_filter = 0;
  if (!opts.type.empty()) {
    for (int t = 1; t <= 7; t++) {
      if (opts.type == net::packet_type_name(static_cast<net::PacketType>(t))) type_filter = t;
    }
    if (!type_filter) {
      return failed("no packet type \"" + opts.type + "\"", "the command line",
                    "rerun with --type=request|challenge|response|accept|deny|payload|disconnect",
                    2);
    }
  }

  Outcome o;
  const int64_t t0 = ds.empty() ? 0 : ds.front().at_us;
  long long matched = 0, shown = 0;
  std::vector<std::string> rows;
  for (const net::CapturedDatagram& d : ds) {
    if (by_peer && d.from != peer && d.to != peer) continue;
    if (type_filter) {
      net::Reader r = net::make_reader(d.data.data(), d.data.size());
      uint32_t protocol = 0;
      net::PacketType type{};
      if (!net::read_packet_start(r, protocol, type) || static_cast<int>(type) != type_filter) {
        continue;
      }
    }
    matched++;
    if (matched <= opts.skip || (opts.limit > 0 && shown >= opts.limit)) continue;
    shown++;
    const double ms = static_cast<double>(d.at_us - t0) / 1000.0;
    const std::string line = net::describe_packet(d.data.data(), d.data.size());
    o.text += "  " + fixed(ms, 1) + " ms  " + net::to_string(d.from) + " > " +
              net::to_string(d.to) + "  " + line + "\n";
    rows.push_back(json_obj({{"t_ms", num(ms)},
                             {"from", json_str(net::to_string(d.from))},
                             {"to", json_str(net::to_string(d.to))},
                             {"bytes", json_num(static_cast<long long>(d.data.size()))},
                             {"packet", json_str(line)}}));
  }
  const long long after = matched - opts.skip - shown;
  if (after > 0) {
    o.text += "  ... " + std::to_string(after) +
              " more (--skip=" + std::to_string(opts.skip + shown) +
              " for the next, --limit=0 for all)\n";
  }
  o.result = json_obj({{"file", json_str(opts.file)},
                       {"datagrams", json_num(static_cast<long long>(ds.size()))},
                       {"matched", json_num(matched)},
                       {"shown", json_num(shown)},
                       {"packets", json_arr(rows)}});
  o.summary = std::to_string(ds.size()) + " datagrams, " + std::to_string(matched) + " matched, " +
              std::to_string(shown) + " shown";
  o.next = "ge net stats " + opts.file;
  return o;
}

Outcome run_net_stats(const NetStatsOptions& opts) {
  std::vector<net::CapturedDatagram> ds;
  std::string error;
  if (!net::load_capture(opts.file, ds, &error)) return failed(error, opts.file, kCaptureNext);
  const net::CaptureAnalysis a = net::analyze_capture(ds, opts.bucket_ms / 1000.0);
  Outcome o;
  const double span_s =
      ds.size() > 1 ? static_cast<double>(ds.back().at_us - ds.front().at_us) / 1e6 : 0;
  o.text += "  " + std::to_string(a.datagrams) + " datagrams over " + fixed(span_s, 2) +
            " s, protocol " + hex(a.protocol, 8) + ", " + std::to_string(a.foreign) + " foreign; " +
            (a.local_known ? "taken at " + net::to_string(a.local)
                           : std::string("the capturing node is unknown: a > b as first seen")) +
            "\n";
  if (a.foreign) {
    o.findings.push_back({Severity::warn,
                          std::to_string(a.foreign) + " datagrams are not the capture's protocol " +
                              hex(a.protocol, 8),
                          opts.file,
                          "ge net inspect " + opts.file +
                              " to read them: another build or another game on the port"});
  }

  std::vector<std::vector<std::string>> rows{{"peer", "pkts out", "in", "kbps out", "in", "rtt p50",
                                              "p95", "turn p50", "loss out", "loss in", "missing",
                                              "dup", "reord", "resends out", "in", "handshake"}};
  std::vector<std::string> conns;
  for (const net::ConnectionStats& c : a.connections) {
    const net::DirectionStats& out = a.local_known ? net::sent_by(c, a.local) : c.ab;
    const net::DirectionStats& in = a.local_known ? net::received_by(c, a.local) : c.ba;
    const int64_t first = std::min(out.first_us < 0 ? in.first_us : out.first_us,
                                   in.first_us < 0 ? out.first_us : in.first_us);
    const int64_t last = std::max(out.last_us, in.last_us);
    const double secs = static_cast<double>(last - first) / 1e6;
    const auto kbps = [&](const net::DirectionStats& s) {
      return secs > 0 ? static_cast<double>(s.bytes) * 8.0 / 1000.0 / secs : NAN;
    };
    const double rtt50 = net::percentile_of(out.ack_delay_ms, 50);
    const double rtt95 = net::percentile_of(out.ack_delay_ms, 95);
    const double turn50 = net::percentile_of(in.ack_delay_ms, 50);
    const auto type_n = [&](net::PacketType t) {
      return out.by_type[static_cast<size_t>(t)] + in.by_type[static_cast<size_t>(t)];
    };
    std::string hs = type_n(net::PacketType::accept)    ? "accepted"
                     : type_n(net::PacketType::deny)    ? "denied"
                     : type_n(net::PacketType::request) ? "asked"
                                                        : "mid-session";
    if (type_n(net::PacketType::disconnect)) hs += ", closed";
    const std::string who = a.local_known
                                ? net::to_string(in.from)
                                : net::to_string(out.from) + " > " + net::to_string(out.to);
    rows.push_back({who, std::to_string(out.packets), std::to_string(in.packets),
                    fixed(kbps(out), 1), fixed(kbps(in), 1), fixed(rtt50, 1), fixed(rtt95, 1),
                    fixed(turn50, 1), pct(out.loss), pct(in.loss), std::to_string(in.missing),
                    std::to_string(in.duplicated), std::to_string(in.reordered),
                    std::to_string(out.resends), std::to_string(in.resends), hs});
    const auto side = [&](const net::DirectionStats& s, double ack50, double ack95) {
      return json_obj({{"packets", json_num(static_cast<long long>(s.packets))},
                       {"bytes", json_num(static_cast<long long>(s.bytes))},
                       {"kbps", num(kbps(s))},
                       {"messages", json_num(static_cast<long long>(s.messages))},
                       {"keepalives", json_num(static_cast<long long>(s.empty))},
                       {"ack_delay_p50_ms", num(ack50)},
                       {"ack_delay_p95_ms", num(ack95)},
                       {"loss", num(s.loss)},
                       {"settled", json_num(static_cast<long long>(s.settled))},
                       {"missing", json_num(static_cast<long long>(s.missing))},
                       {"duplicated", json_num(static_cast<long long>(s.duplicated))},
                       {"reordered", json_num(static_cast<long long>(s.reordered))},
                       {"resends", json_num(static_cast<long long>(s.resends))}});
    };
    conns.push_back(json_obj({{"peer", json_str(net::to_string(in.from))},
                              {"handshake", json_str(hs)},
                              {"out", side(out, rtt50, rtt95)},
                              {"in", side(in, turn50, net::percentile_of(in.ack_delay_ms, 95))}}));
    if (out.loss > 0.05 || in.loss > 0.05) {
      o.findings.push_back({Severity::warn,
                            who + " loses " + pct(out.loss) + " of what it is sent and " +
                                pct(in.loss) + " of what it sends",
                            opts.file,
                            "ge net inspect " + opts.file + " --peer=" + net::to_string(in.from) +
                                " to see the gaps; ge net sim --link=loss=" +
                                fixed(100 * out.loss, 1) + "% reproduces such a link"});
    }
  }
  o.text += table(rows);
  o.text += "  out: what " +
            (a.local_known ? net::to_string(a.local) : std::string("the first side")) +
            " sent; rtt: its sends' wait for an ack, its own tick included; turn: how long it "
            "took to ack what it got; loss: settled by the acks, a send 32 behind the newest ack "
            "never acked; missing: sequences of the peer's never seen here\n";

  std::string series_path;
  if (a.local_known && !a.t_s.empty()) {
    const fs::path file(opts.file);
    const fs::path dir = opts.out.empty() ? file.parent_path() / (file.stem().string() + "-net")
                                          : fs::path(opts.out);
    lab::Series s = lab::make_series({"t_s", "out_kbps", "in_kbps", "rtt_ms", "loss_pct"});
    s.set("title", file.stem().string() + " at " + net::to_string(a.local));
    s.set("x", "t_s");
    s.set("unit.out_kbps", "kbit/s");
    s.set("unit.in_kbps", "kbit/s");
    s.set("unit.rtt_ms", "ms");
    s.set("unit.loss_pct", "%");
    for (size_t i = 0; i < a.t_s.size(); i++) {
      lab::add_row(s, {a.t_s[i], a.out_kbps[i], a.in_kbps[i], a.rtt_ms[i], a.loss_pct[i]});
    }
    const fs::path tsv = dir / (file.stem().string() + ".net.tsv");
    std::string werr;
    if (!write_text(tsv, lab::write_series(s), werr)) {
      o.findings.push_back({Severity::error, "could not write " + tsv.string() + ": " + werr,
                            dir.string(), "pass a writable directory with --out=DIR"});
    } else {
      series_path = tsv.string();
      o.written.push_back(series_path);
      if (opts.charts) {
        GraphOptions g;
        g.files = {series_path};
        g.kind = "line";
        g.out = dir.string();
        const Outcome charts = run_graph(g);
        o.written.insert(o.written.end(), charts.written.begin(), charts.written.end());
        for (const Finding& f : charts.findings) {
          if (f.severity == Severity::error) o.findings.push_back(f);
        }
      }
    }
  }
  o.result = json_obj({{"file", json_str(opts.file)},
                       {"datagrams", json_num(static_cast<long long>(a.datagrams))},
                       {"foreign", json_num(static_cast<long long>(a.foreign))},
                       {"protocol", json_str(hex(a.protocol, 8))},
                       {"local", a.local_known ? json_str(net::to_string(a.local)) : "null"},
                       {"connections", json_arr(conns)},
                       {"series", series_path.empty() ? "null" : json_str(series_path)}});
  o.summary = std::to_string(a.connections.size()) + " connections in " +
              std::to_string(a.datagrams) + " datagrams";
  o.next = series_path.empty() ? "ge net inspect " + opts.file
                               : "ge graph " + series_path + " --columns=rtt_ms,loss_pct";
  return o;
}

Outcome run_net_sim(const NetSimOptions& opts) {
  net::TrafficMix mix;
  if (!net::traffic_mix(opts.mix, mix)) {
    std::string all;
    for (const std::string& m : net::mix_names()) all += (all.empty() ? "" : "|") + m;
    return failed("no traffic mix \"" + opts.mix + "\"", "the command line",
                  "rerun with --mix=" + all, 2);
  }
  std::vector<std::string> presets = opts.presets;
  if (presets.empty()) presets = net::preset_names();
  if (opts.clients < 0 || opts.clients > 64 || opts.max_peers < 1 || opts.max_peers > 32 ||
      !(opts.seconds > 0) || !(opts.tick_hz > 0) || opts.drain < 0) {
    return failed("the session is out of range", "the command line",
                  "use --clients=0..64, --max-peers=1..32, --seconds and --tick-hz above 0", 2);
  }

  Outcome o;
  std::vector<std::vector<std::string>> rows{
      {"preset", "conditions", "joined", "rtt ms", "jitter", "loss", "events", "order", "event p50",
       "p95", "snaps", "inputs", "host kbps out", "in", "resends", "digest"}};
  std::vector<std::string> results;
  for (const std::string& name : presets) {
    net::TrafficPlan plan;
    if (name != "none" && !net::conditions_preset(name, plan.conditions)) {
      std::string all = "none";
      for (const std::string& p : net::preset_names()) all += "," + p;
      return failed("no preset \"" + name + "\"", "the command line", "rerun with --presets=" + all,
                    2);
    }
    if (const std::string bad = apply_link(opts.link, plan.conditions); !bad.empty()) {
      return failed("--link: " + bad, "the command line",
                    "rerun with --link=latency=40,jitter=10,loss=2%", 2);
    }
    plan.clients = opts.clients;
    plan.max_peers = opts.max_peers;
    plan.seconds = opts.seconds;
    plan.drain_seconds = opts.drain;
    plan.tick_hz = opts.tick_hz;
    plan.seed = opts.seed;
    plan.mix = mix;
    plan.capture = !opts.capture.empty();
    const net::TrafficReport r = net::run_traffic(plan);

    const uint64_t ev_sent = r.events_up.sent + r.events_down.sent;
    const uint64_t ev_got = r.events_up.delivered + r.events_down.delivered;
    const bool in_order = r.events_up.out_of_order + r.events_down.out_of_order +
                              r.events_up.duplicated + r.events_down.duplicated ==
                          0;
    std::vector<double> lat = r.events_up.latency_ms;
    lat.insert(lat.end(), r.events_down.latency_ms.begin(), r.events_down.latency_ms.end());
    const auto share = [](const net::StreamTally& t) {
      return t.sent ? static_cast<double>(t.delivered) / static_cast<double>(t.sent) : NAN;
    };
    const double p50 = net::percentile_of(lat, 50), p95 = net::percentile_of(lat, 95);
    rows.push_back({name, net::describe_conditions(plan.conditions), std::to_string(r.connected),
                    fixed(r.rtt_ms, 1), fixed(r.jitter_ms, 1), pct(r.loss_total),
                    std::to_string(ev_got) + "/" + std::to_string(ev_sent),
                    in_order ? "ok" : "BROKEN", fixed(p50, 1), fixed(p95, 1),
                    pct(share(r.snapshots)), pct(share(r.inputs)), fixed(r.host_send_kbps, 0),
                    fixed(r.host_recv_kbps, 0), std::to_string(r.resends), hex(r.digest, 16)});
    if (ev_got != ev_sent || !in_order) {
      o.findings.push_back(
          {Severity::error,
           name + ": the reliable events arrived " + std::to_string(ev_got) + " of " +
               std::to_string(ev_sent) + (in_order ? ", in order" : ", out of order or twice"),
           "engine/net/connection.cpp",
           "a reliable channel lost or reordered a message: reproduce with these flags and read "
           "the capture (--capture=FILE, then ge net inspect) (skill: net)"});
    }
    std::string capture_path;
    if (plan.capture) {
      fs::path path(opts.capture);
      if (presets.size() > 1) {
        path.replace_filename(path.stem().string() + "." + name + path.extension().string());
      }
      std::string err;
      std::error_code ec;
      if (path.has_parent_path()) fs::create_directories(path.parent_path(), ec);
      if (!net::save_capture(r.capture, path.string(), &err)) {
        o.findings.push_back(
            {Severity::error, err, path.string(), "pass a writable path with --capture=FILE"});
      } else {
        capture_path = path.string();
        o.written.push_back(capture_path);
      }
    }
    results.push_back(
        json_obj({{"preset", json_str(name)},
                  {"conditions", json_str(net::describe_conditions(plan.conditions))},
                  {"joined", json_num(r.connected)},
                  {"denied", json_num(r.denied)},
                  {"dropped", json_num(r.dropped)},
                  {"rtt_ms", num(r.rtt_ms)},
                  {"jitter_ms", num(r.jitter_ms)},
                  {"loss", num(r.loss_total)},
                  {"events_sent", json_num(static_cast<long long>(ev_sent))},
                  {"events_delivered", json_num(static_cast<long long>(ev_got))},
                  {"in_order", json_bool(in_order)},
                  {"event_p50_ms", num(p50)},
                  {"event_p95_ms", num(p95)},
                  {"snapshots", num(share(r.snapshots))},
                  {"inputs", num(share(r.inputs))},
                  {"host_send_kbps", num(r.host_send_kbps)},
                  {"host_recv_kbps", num(r.host_recv_kbps)},
                  {"resends", json_num(static_cast<long long>(r.resends))},
                  {"digest", json_str(hex(r.digest, 16))},
                  {"capture", capture_path.empty() ? "null" : json_str(capture_path)}}));
  }
  o.text = "  " + std::to_string(opts.clients) + " clients, " + net::describe_mix(mix) + ", " +
           core::num::to_string(opts.seconds) + " s sending and " +
           core::num::to_string(opts.drain) + " s draining at " +
           core::num::to_string(opts.tick_hz) + " Hz, seed " + std::to_string(opts.seed) + "\n" +
           table(rows) +
           "  rtt: the host's smoothed round trip at the end of the sending, its tick included; "
           "loss: the host's sends never acked; events: reliable, both ways, every one owed in "
           "order; snaps and inputs may be lost\n";
  o.result = json_obj({{"mix", json_str(opts.mix)},
                       {"clients", json_num(opts.clients)},
                       {"runs", json_arr(results)}});
  o.summary = std::to_string(presets.size()) + " sessions run";
  o.next = o.written.empty() ? "ge net sim --capture=runs/net.pcap, then ge net stats on it"
                             : "ge net stats " + o.written.front();
  return o;
}

Outcome run_net(const Args& a) {
  const std::string name(a.command->name);
  const auto number = [&](const char* flag, double fallback, double& out) {
    const std::string v = flag_value(a, flag);
    if (v.empty()) {
      out = fallback;
      return true;
    }
    return parse_double(v, out);
  };
  double x = 0;
  const auto bad = [&](const char* flag) {
    return failed(std::string("--") + flag + "=" + flag_value(a, flag) + " is not a number",
                  "the command line", "run `ge help " + name + "` for its flags", 2);
  };
  if (name == "net inspect") {
    NetInspectOptions n;
    n.file = a.words[0];
    n.peer = flag_value(a, "peer");
    n.type = flag_value(a, "type");
    if (!number("skip", 0, x) || x < 0) return bad("skip");
    n.skip = static_cast<long long>(x);
    if (!number("limit", 200, x) || x < 0) return bad("limit");
    n.limit = static_cast<long long>(x);
    return run_net_inspect(n);
  }
  if (name == "net stats") {
    NetStatsOptions n;
    n.file = a.words[0];
    if (!number("bucket", 250, n.bucket_ms) || n.bucket_ms <= 0) return bad("bucket");
    n.out = flag_value(a, "out");
    n.charts = !has_flag(a, "no-charts");
    return run_net_stats(n);
  }
  NetSimOptions n;
  n.presets = split_commas(flag_value(a, "presets"));
  n.mix = flag_value(a, "mix", "racing");
  n.link = flag_value(a, "link");
  if (!number("clients", 8, x)) return bad("clients");
  n.clients = static_cast<int>(x);
  if (!number("max-peers", 32, x)) return bad("max-peers");
  n.max_peers = static_cast<int>(x);
  if (!number("seconds", 10, n.seconds)) return bad("seconds");
  if (!number("drain", 4, n.drain)) return bad("drain");
  if (!number("tick-hz", 60, n.tick_hz)) return bad("tick-hz");
  if (!number("seed", 1, x) || x < 0) return bad("seed");
  n.seed = static_cast<unsigned>(x);
  n.capture = flag_value(a, "capture");
  return run_net_sim(n);
}

}  // namespace ge::cli
