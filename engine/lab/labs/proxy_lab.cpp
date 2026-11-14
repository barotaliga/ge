// LAB `proxy`: A GAME THROUGH A NEAR AND A FAR PROXY OVER A LOSSY LINK (engine/proxy). The proxies'
// link is given each loss in turn; the game's client (a 24-byte command at 60 Hz) and server (a
// 400-byte snapshot at 20 Hz, each much like the last) talk through the pair with the proxies
// sending each frame once, twice or three times, plain, packed or coded. For each it answers the
// share of the game's datagrams lost and the link's bytes a second each way, against the game's
// own. The loss the game sees is charted per setting; it all runs on a virtual clock off one seed.
//
// ge:summary Lab: a game through a proxy pair at each loss: repeats, packing, loss seen, link bytes
// ge:keywords lab proxy repeat pack loss link bytes near far
// ge:skill net labs
// ge:test tests/proxy_test.cpp
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "proxy/trial.hpp"

namespace {

namespace gl = ge::lab;
namespace px = ge::proxy;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

std::vector<double> numbers(const std::string& s) {
  std::vector<double> out;
  size_t at = 0;
  while (at < s.size()) {
    const size_t comma = s.find(',', at);
    out.push_back(std::atof(s.substr(at, comma == std::string::npos ? comma : comma - at).c_str()));
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return out;
}

void run(gl::Lab& lab) {
  struct Setting {
    const char* name;
    int repeat;
    bool pack;
    bool code;
  };
  const Setting settings[] = {{"plain", 1, false, false},
                              {"twice", 2, false, false},
                              {"three times", 3, false, false},
                              {"packed", 1, true, false},
                              {"three times packed", 3, true, false},
                              {"coded", 1, true, true},
                              {"three times coded", 3, true, true}};
  const int seconds = static_cast<int>(lab.arg_num("seconds"));
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  gl::LabTable table{
      "the proxies' link at each loss; the game's own bytes are the plain row's",
      {"link loss %", "setting", "game lost %", "link up B/s", "link down B/s", "down / game's"},
      {}};
  std::vector<gl::Trace> lines;
  for (const Setting& s : settings) lines.push_back({s.name, {}, {}});
  double plain10 = -1, three10 = -1, plain_down = -1, packed_down = -1, coded_down = -1;
  for (const double loss_pct : numbers(lab.arg("losses"))) {
    for (size_t k = 0; k < std::size(settings); k++) {
      const Setting& s = settings[k];
      px::TrialConfig c;
      c.loss = loss_pct / 100.0;
      c.repeat = s.repeat;
      c.pack = s.pack;
      c.code = s.code;
      c.seconds = seconds;
      c.seed = seed;
      const px::TrialResult r = px::run_proxy_trial(c);
      const double lost = 100.0 * px::trial_lost(r);
      const double up = static_cast<double>(r.link_up) / seconds;
      const double down = static_cast<double>(r.link_down) / seconds;
      const double game_down = static_cast<double>(r.payload_down) / seconds;
      table.rows.push_back({fmt(loss_pct, 0), s.name, fmt(lost, 2), fmt(up, 0), fmt(down, 0),
                            fmt(game_down > 0 ? down / game_down : 0, 2)});
      lines[k].x.push_back(loss_pct);
      lines[k].y.push_back(lost);
      const std::string key = "loss" + fmt(loss_pct, 0) + "." + std::string(s.name);
      lab.metric(key + ".lost-pct", lost, "%");
      lab.metric(key + ".link-down-bytes-per-s", down, "bytes/s");
      if (loss_pct == 10 && k == 0) plain10 = lost;
      if (loss_pct == 10 && k == 2) three10 = lost;
      if (loss_pct == 0 && k == 0) plain_down = down;
      if (loss_pct == 0 && k == 3) packed_down = down;
      if (loss_pct == 0 && k == 5) coded_down = down;
    }
  }
  lab.table(table);
  if (plain10 >= 0 && three10 >= 0)
    lab.note("at 10% link loss the game lost " + fmt(plain10, 2) + "% plain and " +
             fmt(three10, 2) + "% with three copies");
  if (plain_down > 0 && packed_down > 0)
    lab.note("packing sent " + fmt(100.0 * packed_down / plain_down, 0) +
             "% of the plain link's bytes down");
  if (plain_down > 0 && coded_down > 0)
    lab.note("coding sent " + fmt(100.0 * coded_down / plain_down, 0) +
             "% of the plain link's bytes down");
  const gl::ChartFrame frame{"THE LOSS THE GAME SEES, BY THE PROXIES' SETTING",
                             "a 60 Hz client and a 20 Hz server; " + lab.arg("seconds") +
                                 " s each; seed " + lab.arg("seed"),
                             "loss on the proxies' link (%)", "the game's datagrams lost (%)"};
  lab.shot({"loss",
            frame.title,
            "Each setting's share of the game's datagrams lost, both ways, against the loss on the "
            "link between the two proxies; the game's two hops to its proxies are clean.",
            "the plain line on the diagonal and the repeated ones flat near zero",
            {{"losses", lab.arg("losses")}, {"seed", lab.arg("seed")}}},
           gl::to_picture(gl::line_chart({frame, lines, {}, {}, {}, false, true, false}).plot));
  if (plain10 >= 0 && three10 >= 0 && three10 * 5 > plain10)
    lab.fail("three copies did not cut the loss at 10% five times",
             "next: run tests/proxy_test.cpp and read the link stats; skill: net.");
  if (plain_down > 0 && packed_down * 2 > plain_down)
    lab.fail("packing did not halve the link's bytes",
             "next: read pack_datagram in engine/proxy/proxy.cpp; skill: net.");
  if (packed_down > 0 && coded_down * 2 > packed_down)
    lab.fail("coding did not halve the packed link's bytes",
             "next: read code_datagram in engine/proxy/link_code.cpp; skill: net.");
}

const gl::LabSpec kSpec{
    "proxy",
    gl::LabKind::sim,
    "engine",
    "engine/proxy (the near and far proxies, repeats, packing, the link coder)",
    "net",
    "Through a near and a far proxy over a lossy link, how much of a game's traffic is lost with "
    "each number of copies, and how many bytes do packing and the coder save?",
    {
        {"losses", "0,2,5,10,20", "the proxies' link's loss, percent, comma-separated"},
        {"seconds", "20", "each setting's counted run"},
        {"seed", "5", "the network's stream"},
    },
    "--losses=0,10 --seconds=4",
    run,
};
GE_LAB(kSpec);

}  // namespace
