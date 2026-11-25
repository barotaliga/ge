// Tests: the operator's own recording (engine/server/hand_record.hpp) on the core's sample game:
// `serverrecord` writes a replay stream from that frame to demos/<name>.ged, `serverstop` closes
// it whole; a name that would leave the folder, a second recording and a stop with none running
// are refused with what to do.
//
// ge:summary Tests: serverrecord and serverstop, the file and the refusals
// ge:keywords test serverrecord serverstop hand-record sample-game
// ge:skill replay net testing
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "check.hpp"
#include "replay/stream.hpp"
#include "server/hand_record.hpp"
#include "server/sample.hpp"
#include "server/server.hpp"

namespace sv = ge::server;
namespace rp = ge::replay;
namespace fs = std::filesystem;

TEST(serverrecord_writes_the_match_from_now_and_serverstop_closes_it) {
  const fs::path dir = fs::temp_directory_path() / "ge_hand_record" / "demos";
  fs::remove_all(dir.parent_path());
  sv::ServerConfig c;
  c.map = "ring";
  c.bots = 4;
  c.maxclients = 8;
  c.seed = 7;
  c.offline = true;
  sv::Server s;
  std::string error;
  CHECK_MSG(sv::server_open(s, sv::sample_game(), c, nullptr, nullptr, error), error);
  for (int f = 0; f < 60; f++) sv::server_frame(s);
  sv::HandRecording h;
  std::string said;
  CHECK(!sv::hand_record_stop(h, s, said) && said == "serverstop: nothing is being recorded");
  CHECK(!sv::hand_record_start(h, s, dir, "../away", said));
  CHECK_MSG(said.find("without ..") != std::string::npos, said);
  CHECK_MSG(sv::hand_record_start(h, s, dir, "final", said), said);
  CHECK(said ==
        "serverrecord: recording to " + (dir / "final.ged").string() + "; serverstop ends it");
  CHECK(!sv::hand_record_start(h, s, dir, "other", said));  // one at a time
  for (int f = 0; f < 3 * 60; f++) {
    sv::server_frame(s);
    sv::hand_record_frame(h, s);
  }
  CHECK_MSG(sv::hand_record_stop(h, s, said), said);
  CHECK_MSG(said.rfind("serverstop: " + (dir / "final.ged").string() + " kept (", 0) == 0, said);
  std::ifstream in(dir / "final.ged", std::ios::binary);
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
  rp::StreamReader r;
  CHECK_MSG(rp::read_stream_header(bytes, r, error), error);
  int blocks = 0;
  rp::Block b;
  while (rp::next_block(r, b, error)) blocks++;
  CHECK_MSG(error.empty(), error);
  CHECK_MSG(blocks == 3, std::to_string(blocks));  // a second a block: the three recorded
  CHECK(!sv::hand_record_stop(h, s, said));
  sv::server_close(s);
  fs::remove_all(dir.parent_path());
}
