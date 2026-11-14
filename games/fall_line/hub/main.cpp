// FALL LINE'S HUB (docs/hub.md, M56k): the engine's hub with Fall Line's words, named
// `fall_line_hub` on disk. Fall Line has no dedicated server yet (M17k), so its hub lists no live
// games: it keeps runs. An input log dropped in the archive's inbox (or added by the operator) is
// kept by its hash, its row read by playing it out (runs.hpp), found on the Demos page and
// watched on the page with Fall Line's web viewer from `--viewer=<dir>`. `--help` lists its flags.
//
// ge:summary Fall Line's hub binary: the engine's hub keeping and showing the game's runs
// ge:keywords hub fall_line_hub runs input-log archive main
// ge:skill net
#include "hub/main.hpp"
#include "fall_line/hub/runs.hpp"
#include "fall_line/sim/play.hpp"
#include "net/packet.hpp"

namespace {

ge::hub::HubSpec spec() {
  ge::hub::HubSpec s;
  s.game = "Fall Line";
  s.id = "fall_line";
  s.protocol = ge::net::protocol_id("fall_line", 1);
  s.modes = {{"race", 3}, {"free ride", 2}};
  s.score = "time";
  s.score_low_first = true;
  s.score_ms = true;
  s.look = "alpine";
  s.map_word = "seed";  // a run is found by its seed
  s.http_port = 28090;
  s.run_row = fall_line::hub::run_row;
  return s;
}

}  // namespace

int main(int argc, char** argv) { return ge::hub::hub_main(argc, argv, spec()); }
