// A hub's process, defined: the flags, the sockets, the wall-clock loop and its status lines.
//
// ge:summary A hub's process defined: flags, sockets, the wall-clock loop, status lines
// ge:keywords hub hub_main flags loop wall-clock status
// ge:skill net
// ge:test tests/hub_http_test.cpp
#include "hub/main.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "hub/http.hpp"
#include "hub/poller.hpp"
#include "net/address.hpp"
#include "net/master.hpp"
#include "net/transport.hpp"

namespace ge::hub {

namespace gn = ge::net;

namespace {

bool number(const std::string& v, double& out) {
  char* end = nullptr;
  out = std::strtod(v.c_str(), &end);
  return !v.empty() && end && *end == 0 && out >= 0;
}

}  // namespace

bool hub_options(const std::vector<std::string>& args, HubOptions& out, std::string& error) {
  for (const std::string& a : args) {
    if (a == "--help" || a == "-h") {
      out.help = true;
      continue;
    }
    const size_t eq = a.find('=');
    const std::string key = a.substr(0, eq),
                      value = eq == std::string::npos ? "" : a.substr(eq + 1);
    double d = 0;
    bool ok = true;
    if (key == "--serve-master") {
      ok = value.empty() || (number(value, d) && d >= 1 && d <= 65535 && d == static_cast<int>(d));
      out.serve_master = value.empty() ? gn::kMasterPort : static_cast<int>(d);
    } else if (key == "--master") {
      out.masters = value;
    } else if (key == "--ip") {
      out.ip = value;
    } else if (key == "--port" || key == "--http") {
      ok = number(value, d) && d <= 65535 && d == static_cast<int>(d);
      (key == "--port" ? out.port : out.http) = static_cast<int>(d);
    } else if (key == "--list-every") {
      ok = number(value, out.list_s) && out.list_s > 0;
    } else if (key == "--status-every") {
      ok = number(value, out.status_s) && out.status_s > 0;
    } else if (key == "--busy-every") {
      ok = number(value, out.busy_s) && out.busy_s > 0;
    } else if (key == "--for") {
      ok = number(value, out.for_s);
    } else if (key == "--status") {
      ok = number(value, out.status_line_s) && out.status_line_s > 0;
    } else {
      error = "unknown flag '" + a + "'; next: --help lists the flags";
      return false;
    }
    if (!ok) {
      error = "flag ";
      error += key;
      error += " needs a number in range, not '";
      error += value;
      error += "'; next: --help";
      return false;
    }
  }
  if (out.masters.empty() && !out.help && out.serve_master == 0) {
    error =
        "no master to read; next: pass --master=<host:port> (several: comma-separated), or "
        "--serve-master to run one here";
    return false;
  }
  return true;
}

std::string hub_help(const HubSpec& spec) {
  return spec.game +
         "'s hub: every live server and broadcast its masters list, as a JSON API.\n"
         "  --master=<host:port,...>  the masters to read (port 27900 when none is given)\n"
         "  --serve-master[=<port>]   run the master here too (port 27900 by default)\n"
         "  --ip=<address>            where both sockets bind (default 0.0.0.0)\n"
         "  --port=<n>                the UDP socket's port (default any)\n"
         "  --http=<n>                the web port (default " +
         std::to_string(spec.http_port) +
         ")\n"
         "  --list-every=<s>          the masters' lists this often (default 60)\n"
         "  --status-every=<s>        each node's status this often (default 30)\n"
         "  --busy-every=<s>          ... while a human plays there (default 4)\n"
         "  --status=<s>              a status line this often (default 60)\n"
         "  --for=<s>                 stop after this long (default 0: run until killed)\n"
         "Then GET http://<ip>:<http>/v1 lists the paths.\n";
}

int hub_main(int argc, char** argv, const HubSpec& spec) {
  std::vector<std::string> args(argv + 1, argv + argc);
  HubOptions o;
  std::string error;
  if (!hub_options(args, o, error)) {
    std::fprintf(stderr, "%s_hub: %s\n", spec.id.c_str(), error.c_str());
    return 1;
  }
  if (o.help) {
    std::fputs(hub_help(spec).c_str(), stdout);
    return 0;
  }
  PollerConfig pc;
  if (o.masters.empty()) o.masters = "127.0.0.1:" + std::to_string(o.serve_master);
  if (!gn::master_list(o.masters, pc.masters, error)) {
    std::fprintf(stderr, "%s_hub: --master: %s\n", spec.id.c_str(), error.c_str());
    return 1;
  }
  pc.protocol = spec.protocol;
  pc.list_us = static_cast<int64_t>(o.list_s * 1e6);
  pc.status_us = static_cast<int64_t>(o.status_s * 1e6);
  pc.busy_us = static_cast<int64_t>(o.busy_s * 1e6);
  gn::Address udp_at, http_at;
  const uint16_t http_port = static_cast<uint16_t>(o.http ? o.http : spec.http_port);
  if (!gn::parse_address(o.ip + ":" + std::to_string(o.port), udp_at) ||
      !gn::parse_address(o.ip + ":" + std::to_string(http_port), http_at)) {
    std::fprintf(stderr, "%s_hub: --ip '%s' is not an address; next: pass --ip=0.0.0.0\n",
                 spec.id.c_str(), o.ip.c_str());
    return 1;
  }
  gn::Transport udp;
  if (!gn::open_udp_transport(udp, udp_at, &error)) {
    std::fprintf(stderr, "%s_hub: cannot open UDP on %s: %s; next: pass another --port\n",
                 spec.id.c_str(), gn::to_string(udp_at).c_str(), error.c_str());
    return 1;
  }
  gn::Transport master_port;
  gn::Master master;
  if (o.serve_master) {
    gn::Address master_at;
    if (!gn::parse_address(o.ip + ":" + std::to_string(o.serve_master), master_at) ||
        !gn::open_udp_transport(master_port, master_at, &error)) {
      std::fprintf(stderr,
                   "%s_hub: cannot run the master on port %d: %s; next: pass another "
                   "--serve-master port\n",
                   spec.id.c_str(), o.serve_master, error.c_str());
      gn::close_transport(udp);
      return 1;
    }
    gn::master_init(master, gn::MasterConfig{}, master_port);
  }
  HttpServer http;
  if (!http_open(http, http_at, error)) {
    std::fprintf(stderr, "%s_hub: %s\n", spec.id.c_str(), error.c_str());
    gn::close_transport(udp);
    if (o.serve_master) gn::close_transport(master_port);
    return 1;
  }
  using clock = std::chrono::steady_clock;
  const clock::time_point start = clock::now();
  const auto now_us = [&] {
    return std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - start).count();
  };
  Poller p;
  poller_init(p, pc, 0);
  std::printf("%s_hub: reading %zu master(s), serving http://%s/v1\n", spec.id.c_str(),
              pc.masters.size(), gn::to_string(http_at).c_str());
  std::fflush(stdout);
  int64_t next_line = static_cast<int64_t>(o.status_line_s * 1e6);
  for (;;) {
    const int64_t now = now_us();
    if (o.for_s > 0 && now >= static_cast<int64_t>(o.for_s * 1e6)) break;
    if (o.serve_master) gn::master_update(master, now);
    gn::transport_pump(udp, now);
    poller_pump(p, udp, now);
    http_update(http, p, spec, now);
    if (now >= next_line) {
      std::printf(
          "%s_hub: %d servers, %d broadcasts, %zu nodes listed; %llu asks, %llu records, "
          "%llu http answers, %zu open\n",
          spec.id.c_str(), poller_count(p, gn::kind_bit(gn::NodeKind::game)),
          poller_count(p, gn::kind_bit(gn::NodeKind::tv)), p.nodes.size(),
          static_cast<unsigned long long>(p.stats.asks),
          static_cast<unsigned long long>(p.stats.records),
          static_cast<unsigned long long>(http.stats.answered), http.conns.size());
      std::fflush(stdout);
      next_line = now + static_cast<int64_t>(o.status_line_s * 1e6);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  http_close(http);
  gn::close_transport(udp);
  if (o.serve_master) gn::close_transport(master_port);
  return 0;
}

}  // namespace ge::hub
