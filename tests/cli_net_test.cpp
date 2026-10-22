// `ge net` (engine/cli/net.hpp): ge net sim runs the same session the traffic generator does (its
// digest), writes the host's capture, and ge net inspect and ge net stats read that capture back:
// every datagram a line, every client a connection, the series and its charts written; usage
// errors for an unknown preset, mix, link key or packet type and a missing file.
//
// ge:summary Test: ge net sim, inspect and stats on a capture; their usage errors
// ge:keywords test cli net sim inspect stats pcap capture digest
// ge:skill testing agent-interface net
#include <filesystem>
#include <string>

#include "check.hpp"
#include "cli/finding.hpp"
#include "cli/net.hpp"
#include "net/conditions.hpp"
#include "net/traffic.hpp"

namespace fs = std::filesystem;
using namespace ge::cli;

namespace {

fs::path scratch() {
  const fs::path dir = fs::temp_directory_path() / "ge_cli_net_test";
  fs::remove_all(dir);
  fs::create_directories(dir);
  return dir;
}

std::string hex16(uint64_t v) {
  static const char* k = "0123456789abcdef";
  std::string s(16, '0');
  for (int i = 15; i >= 0; i--, v >>= 4) s[static_cast<size_t>(i)] = k[v & 15];
  return s;
}

std::string say(const Outcome& o) {
  std::string s;
  for (const Finding& f : o.findings) s += format_finding("ge net", f);
  return s;
}

}  // namespace

TEST(net_sim_is_the_traffic_generator_s_session_and_its_capture_reads_back) {
  const fs::path dir = scratch();
  NetSimOptions sim;
  sim.presets = {"wifi"};
  sim.clients = 3;
  sim.seconds = 3;
  sim.drain = 2;
  sim.capture = (dir / "wifi.pcap").string();
  const Outcome s = run_net_sim(sim);
  CHECK_MSG(exit_of(s) == 0, say(s));
  CHECK(fs::exists(dir / "wifi.pcap") && s.written.size() == 1);

  ge::net::TrafficPlan plan;
  plan.clients = 3;
  plan.seconds = 3;
  plan.drain_seconds = 2;
  ge::net::conditions_preset("wifi", plan.conditions);
  const uint64_t digest = ge::net::run_traffic(plan).digest;
  CHECK_MSG(s.result.find("\"digest\":\"" + hex16(digest) + "\"") != std::string::npos,
            "ge net sim's digest is not run_traffic's " + hex16(digest) + ": " + s.result);

  NetInspectOptions in;
  in.file = sim.capture;
  in.limit = 5;
  const Outcome i = run_net_inspect(in);
  CHECK_MSG(exit_of(i) == 0, say(i));
  CHECK(i.text.find("request") != std::string::npos &&
        i.text.find("more (--skip=5") != std::string::npos);
  in.type = "accept";
  in.limit = 0;
  const Outcome accepts = run_net_inspect(in);
  CHECK(accepts.result.find("\"matched\":3") != std::string::npos);

  NetStatsOptions st;
  st.file = sim.capture;
  const Outcome t = run_net_stats(st);
  CHECK_MSG(exit_of(t) == 0, say(t));
  CHECK(t.result.find("\"local\":\"sim:0:1\"") != std::string::npos);
  CHECK(t.summary.starts_with("3 connections"));
  CHECK(fs::exists(dir / "wifi-net" / "wifi.net.tsv"));
  CHECK_MSG(t.written.size() > 1, "the series' charts were not written");
}

TEST(net_sim_writes_a_capture_a_preset_when_several) {
  const fs::path dir = scratch();
  NetSimOptions sim;
  sim.presets = {"lan", "none"};
  sim.mix = "lockstep";
  sim.link = "latency=20,loss=1%";
  sim.clients = 2;
  sim.seconds = 1;
  sim.drain = 1;
  sim.capture = (dir / "s.pcap").string();
  const Outcome s = run_net_sim(sim);
  CHECK_MSG(exit_of(s) == 0, say(s));
  CHECK(fs::exists(dir / "s.lan.pcap") && fs::exists(dir / "s.none.pcap"));
  CHECK(s.text.find("no snapshots") != std::string::npos);
  CHECK(s.text.find("20 ms") != std::string::npos);
}

TEST(net_usage_errors_say_what_to_pass) {
  NetSimOptions sim;
  sim.presets = {"moon"};
  CHECK(exit_of(run_net_sim(sim)) == 2);
  sim.presets = {"lan"};
  sim.mix = "chess";
  CHECK(exit_of(run_net_sim(sim)) == 2);
  sim.mix = "racing";
  sim.link = "latency=fast";
  CHECK(exit_of(run_net_sim(sim)) == 2);
  sim.link = "colour=blue";
  CHECK(exit_of(run_net_sim(sim)) == 2);
  sim.link = "loss=1.5";
  CHECK(exit_of(run_net_sim(sim)) == 2);

  NetInspectOptions in;
  in.file = (fs::temp_directory_path() / "ge_cli_net_test_missing.pcap").string();
  const Outcome missing = run_net_inspect(in);
  CHECK(exit_of(missing) == 1 && missing.findings.size() == 1 && !missing.findings[0].next.empty());
  NetStatsOptions st;
  st.file = in.file;
  CHECK(exit_of(run_net_stats(st)) == 1);
}
