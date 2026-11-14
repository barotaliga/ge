// A RUN'S REPLAY FILE AS AN INPUT LOG for a game's `play` executable: one line beside GE_GAME,
// `GE_REPLAY_LOGS();`, and every command that takes a log (`ge replay`, `ge state`, `ge diverge`,
// `ge why`, the saves) takes the run's replay file (version 2) as well, read back by
// `spectate_inputs` into the log it was played from (play/inputs.hpp § A log from a file's bytes:
// the play library cannot call this one, which depends on it, so the executable registers it).
//
// ge:summary The replay file read as an input log, registered in a game's play executable
// ge:keywords replay-file input-log play ge-replay log-reader ger replay-logs-macro register
// ge:skill replay agent-interface
// ge:test tests/fall_line_spectate_test.cpp
#pragma once

#include "play/inputs.hpp"
#include "replay/spectate.hpp"

namespace ge::replay {

inline constexpr play::LogReaderSpec kReplayLogs{&spectate_inputs};

}  // namespace ge::replay

// Register the replay file's reader as this executable's (play/inputs.hpp).
#define GE_REPLAY_LOGS()                                            \
  static ::ge::play::LogReaderRegister GE_GAME_CAT(ge_replay_logs_, \
                                                   __LINE__)(::ge::replay::kReplayLogs)
