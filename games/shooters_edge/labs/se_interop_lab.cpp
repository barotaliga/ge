// LAB `se-interop`: does an existing client of the classic protocol sign on to our server and play,
// and does our protocol read what an existing server sends? NOT RUN YET: it needs an existing
// client and server built beside the checkout (never in the tree), a loopback UDP socket and a
// capture of both directions, which this container does not keep. Until it runs, the lab reports
// "not run" with the commands it would run, so the gap stays visible in every lab listing and the
// first session with those programs at hand knows what to do. The server half's own checks are
// the `se-net` lab and tests/shooters_edge_classic_server_test.cpp.
//
// ge:summary Lab (stub): an existing classic client and server against ours; reports not run
// ge:keywords lab se-interop interop classic-protocol existing-client capture loopback not-run
// ge:skill net labs
// ge:test tests/shooters_edge_classic_server_test.cpp
#include <string>

#include "lab/lab.hpp"

namespace {

namespace gl = ge::lab;

void run(gl::Lab& lab) {
  gl::LabTable steps{"what the lab would run (not run: no existing client or server here)",
                     {"step", "command"},
                     {}};
  steps.rows.push_back({"1. our classic server on a loopback UDP socket, capturing",
                        "build/<preset>/shooters_edge_server --classic --port=27500 "
                        "--capture=runs/interop.pcap (the flag is still to come)"});
  steps.rows.push_back({"2. an existing client connects and plays a minute",
                        "<client> +connect 127.0.0.1:27500 (built beside the checkout, never "
                        "committed)"});
  steps.rows.push_back(
      {"3. the capture's datagrams listed", "ge net inspect runs/interop.pcap --type=payload"});
  steps.rows.push_back({"4. the capture decoded message by message with our codec",
                        "ge net inspect runs/interop.pcap --protocol=classic (still to come)"});
  steps.rows.push_back({"5. our scripted client against an existing server",
                        "make lab LAB=se-net ARGS=--server=127.0.0.1:27500 (still to come)"});
  lab.table(steps);
  lab.metric("interop.run", 0, "bool");
  lab.note(
      "se-interop: not run. It needs an existing classic client and server beside the "
      "checkout; the commands above are what a session with them runs.");
}

const gl::LabSpec kSpec{
    "se-interop",
    gl::LabKind::drive,
    "game:shooters_edge",
    "games/shooters_edge/net: the classic protocol against existing clients and servers",
    "net",
    "Does an existing classic client sign on to our server and play, and does our codec read an "
    "existing server's packets? (A stub until those programs are at hand: it reports not run.)",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
