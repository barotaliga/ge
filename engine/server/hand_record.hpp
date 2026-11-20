// A RECORDING THE OPERATOR STARTS AND STOPS: `serverrecord <name>` on the server's console writes
// the match from that frame on, every player's part (record.hpp), to `demos/<name>.ged` in the
// game's folder; `serverstop` ends it. It runs beside `--record`, the TV and `sv_autorecord`, each
// on its own stream. A name with `..`, `/`, `\` or `:` is refused (the file stays in `demos/`),
// and so is a second recording while one runs. The dedicated process (main.cpp) runs the commands
// and feeds each frame; a mod switch or the server's end closes the file.
//
// ge:summary A recording the operator starts and stops: serverrecord and serverstop
// ge:keywords serverrecord serverstop server-record record demos operator console
// ge:skill replay net
// ge:test tests/server_hand_record_test.cpp
#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "server/record.hpp"
#include "server/server.hpp"

namespace ge::server {

struct HandRecording {
  Recording rec;
  std::ofstream out;
  std::string path;  // the file being written ("" none)
};

// `serverrecord <name>`: the stream begun at the server's frame now, into `dir`/<name>.ged (the
// folder made when missing). False with what to do in `said`; true with where it goes.
bool hand_record_start(HandRecording& h, const Server& s, const std::filesystem::path& dir,
                       std::string_view name, std::string& said);
// After each server frame: the frame recorded and the new bytes written (nothing when none runs).
void hand_record_frame(HandRecording& h, const Server& s);
// `serverstop`, a mod switch or the end: the stream closed and the file with it. False when none
// ran; `said` says what was kept or that nothing was being recorded.
bool hand_record_stop(HandRecording& h, const Server& s, std::string& said);

}  // namespace ge::server
