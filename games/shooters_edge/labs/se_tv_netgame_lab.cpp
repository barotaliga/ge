// LAB `se-tv-netgame`: A RECORDING WATCHED BY A SINGLE-PLAYER CLIENT THROUGH THE TV
// (games/shooters_edge/net/classic_tv_netgame.hpp). `setv`'s netgame face serves a recording on
// the single-player protocol (version 15); the scripted client of net/classic_netgame_watcher.hpp,
// written from the transport's facts, connects over the simulated network under each link's
// conditions, signs on and watches; partway through a multiview recording it presses fire to
// follow the next player. It answers how long the sign-on and the switch take, how many frames and
// bytes a second reach the client, how many commands were sent again, and that every message read
// and the loop brought the client back; the frames received are charted over time, one line a link.
//
// ge:summary Lab: a single-player client watching a recording through the TV, per link
// ge:keywords lab netgame single-player protocol-15 tv sign-on frames-per-second resend loop
// ge:keywords follow switch-player multiview
// ge:skill net shooter labs
// ge:test tests/shooters_edge_classic_tv_netgame_test.cpp
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/conditions.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/net/classic_netgame_watcher.hpp"
#include "shooters_edge/net/classic_tv_netgame.hpp"

namespace {

namespace gl = ge::lab;
namespace cn = shooters_edge::net;
namespace cl = shooters_edge::client;
namespace gn = ge::net;

constexpr uint16_t kPortTv = 26000, kPortClient = 27001;
constexpr int64_t kTickUs = 13000;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

std::vector<std::string> words(const std::string& s) {
  std::vector<std::string> out;
  std::stringstream in(s);
  std::string w;
  while (std::getline(in, w, ','))
    if (!w.empty()) out.push_back(w);
  return out;
}

struct Watch {
  gn::SimNetwork net;
  gn::Transport face_t, client_t;
  cn::NetgameTv tv;
  cn::NetgameWatcher c;
  gl::Trace line;
};

void run(gl::Lab& lab) {
  const double seconds = lab.arg_num("seconds");
  const std::vector<std::string> links = words(lab.arg("links"));
  std::vector<uint8_t> bytes;
  std::string source;
  if (!lab.arg("demo").empty()) {
    std::ifstream in(lab.arg("demo"), std::ios::binary);
    bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (!in.good() && !in.eof()) bytes.clear();
    if (bytes.empty())
      return lab.fail("the recording " + lab.arg("demo") + " did not read (missing or empty)",
                      "next: pass --demo=<a recording file>, or leave it out for the built one.");
    source = lab.arg("demo");
  } else if (lab.arg("recording") == "match") {
    bytes = cl::write_multiview_match(cl::highlight_match());
    source = "a built 30 s multiview match of four players";
  } else {
    const int frames = static_cast<int>(lab.arg_num("frames"));
    bytes = cl::write_single_demo(cl::loop_script("halls", {128, 192, 24}, 64, frames));
    source = "a built single-player recording of " + std::to_string(frames) + " frames";
  }

  gl::LabTable table{"each link: the sign-on, then what reached the client",
                     {"link", "sign-on ms", "switch ms", "frames/s", "bytes/s", "resends", "loops",
                      "seated", "read"},
                     {}};
  std::vector<gl::Trace> lines;
  bool all_seated = true, all_read = true, looped = true, switched = true;
  const double follow_at = lab.arg_num("follow-at");
  for (size_t i = 0; i < links.size(); i++) {
    const std::string& link = links[i];
    gn::LinkConditions lc;
    if (!gn::conditions_preset(link, lc)) {
      lab.fail("no link preset is named " + link,
               "next: pass --links= from lan, wifi, mobile, bad, awful (net/conditions.hpp).");
      continue;
    }
    auto w = std::make_unique<Watch>();
    w->net = gn::make_sim_network(static_cast<uint32_t>(i + 1), lc);
    gn::open_sim_transport(w->face_t, w->net, gn::sim_address(0, kPortTv));
    gn::open_sim_transport(w->client_t, w->net, gn::sim_address(1, kPortClient));
    std::string error;
    if (!cn::udp_tv_netgame_open(w->tv, bytes, true, error))
      return lab.fail("the recording did not open as a netgame: " + error,
                      "next: run tests/shooters_edge_classic_tv_netgame_test.cpp; skill: net.");
    cn::udp_tv_netgame_start(w->tv, w->face_t, {});
    cn::netgame_watch(w->c, w->client_t, gn::sim_address(0, kPortTv));
    w->line.label = link;
    int64_t now = 0;
    const int64_t end = static_cast<int64_t>(seconds * 1e6);
    int tick = 0;
    int64_t pressed_us = -1;  // when fire was pressed to follow the next player
    int seats_at_press = 0;
    int64_t switched_us = -1;  // when the client was seated in the new view
    while (now < end) {
      now += kTickUs;
      w->c.buttons = 0;
      if (pressed_us < 0 && follow_at > 0 && w->tv.players.size() > 1 &&
          now >= static_cast<int64_t>(follow_at * 1e6) && w->c.signon == 3) {
        w->c.buttons = 1;  // fire: the next player
        pressed_us = now;
        seats_at_press = w->c.seatings;
      }
      gn::sim_advance(w->net, now);
      cn::udp_tv_netgame_frame(w->tv, now);
      cn::netgame_watch_tick(w->c, now);
      if (pressed_us >= 0 && switched_us < 0 && w->c.seatings > seats_at_press)
        switched_us = w->c.last_seated_us;
      if (++tick % 8 == 0) {
        w->line.x.push_back(static_cast<double>(now) / 1e6);
        w->line.y.push_back(w->c.frames);
      }
    }
    const cn::NetgameWatcher& c = w->c;
    const bool seated = c.seated_us >= 0;
    const double signon_ms =
        seated ? static_cast<double>(c.seated_us - c.first_connect_us) / 1000.0 : -1;
    const double watched_s = seated ? seconds - static_cast<double>(c.seated_us) / 1e6 : 0;
    const double fps = watched_s > 0 ? c.frames / watched_s : 0;
    const double bps = seconds > 0 ? static_cast<double>(c.bytes_in) / seconds : 0;
    const bool did_switch = switched_us >= 0;
    const double switch_ms =
        did_switch ? static_cast<double>(switched_us - pressed_us) / 1000.0 : -1;
    if (pressed_us >= 0) switched = switched && did_switch;
    all_seated = all_seated && seated;
    all_read = all_read && c.read_error.empty();
    // Each loop sends the client back to sign on; the last may still be under way at the end.
    looped =
        looped && c.reconnects + 1 >= static_cast<int>(w->tv.rounds) && c.seatings >= c.reconnects;
    table.rows.push_back(
        {link, seated ? fmt(signon_ms, 0) : "never", did_switch ? fmt(switch_ms, 0) : "-",
         fmt(fps, 1), fmt(bps, 0), std::to_string(c.resends), std::to_string(c.reconnects),
         std::to_string(c.seatings), c.read_error.empty() ? "every message" : c.read_error});
    lab.metric(link + ".signon-ms", signon_ms, "ms");
    if (pressed_us >= 0) lab.metric(link + ".switch-ms", switch_ms, "ms");
    lab.metric(link + ".frames-per-s", fps, "frames/s");
    lab.metric(link + ".bytes-per-s", bps, "bytes/s");
    lab.metric(link + ".resends", c.resends, "commands");
    lab.metric(link + ".loops", c.reconnects, "loops");
    lab.metric(link + ".seatings", c.seatings, "sign-ons");
    lines.push_back(std::move(w->line));
  }
  lab.table(table);
  lab.note("the recording: " + source + "; watched for " + lab.arg("seconds") + " s on each link");
  const gl::ChartFrame frame{"FRAMES THAT REACHED THE SINGLE-PLAYER CLIENT",
                             source + "; " + lab.arg("seconds") + " s a link",
                             "time since the first connect (s)", "frames received"};
  lab.shot({"frames",
            frame.title,
            "Frame datagrams the scripted client took from the TV's netgame face, one line a link: "
            "flat while it signs on, then rising at the recording's pace; a loop restarts the "
            "sign-on and shows as a short flat step.",
            "every line rising at the same slope once seated; the worse the link, the later and "
            "the more ragged",
            {{"links", lab.arg("links")}, {"seconds", lab.arg("seconds")}}},
           gl::to_picture(gl::line_chart({frame, lines, {}, {}, {}, false, false, false}).plot));
  if (!all_seated)
    lab.fail("a client did not sign on to the netgame",
             "next: run tests/shooters_edge_classic_tv_netgame_test.cpp; read the signon in "
             "games/shooters_edge/net/classic_tv_netgame.cpp; skill: net.");
  if (!all_read)
    lab.fail("a message the face sent did not read with the game's reader",
             "next: read the table's read column; skill: net.");
  if (!switched)
    lab.fail("fire did not take a client to the next player's view",
             "next: run tests/shooters_edge_classic_tv_netgame_test.cpp (picks who it follows); "
             "read follow in games/shooters_edge/net/classic_tv_netgame.cpp.");
  if (!looped)
    lab.fail("the recording looped and a client was not sent back to its start",
             "next: read the level change in games/shooters_edge/net/classic_tv_netgame.cpp.");
}

const gl::LabSpec kSpec{
    "se-tv-netgame",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/net/classic_tv_netgame: a recording served as a netgame on the "
    "single-player protocol, signed on and watched by a scripted client",
    "net",
    "Watched by a single-player client over each kind of link, how long do the sign-on and a "
    "switch to another player take, how many frames and bytes a second arrive, and does the loop "
    "bring the client back?",
    {
        {"seconds", "40", "how long each link is watched"},
        {"recording", "match",
         "the built recording: match (a 30 s multiview match of four players) or single"},
        {"follow-at", "6",
         "the second the client presses fire to follow the next player (0: never)"},
        {"links", "lan,mobile,awful", "the link presets, comma-separated (net/conditions.hpp)"},
        {"frames", "900", "the built single-player recording's frames"},
        {"demo", "", "a recording file to serve instead of the built one (any open_demo reads)"},
    },
    "--seconds=16",
    run,
};
GE_LAB(kSpec);

}  // namespace
