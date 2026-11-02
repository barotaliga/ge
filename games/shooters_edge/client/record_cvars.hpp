// THE RECORDING SETTINGS a player sets from the console: whether recordings are packed as they
// are written (`demo_pack`, on by default) and at which zstd level (`demo_pack_level`, 19 by
// default: the level the `se-demo-pack` lab chose, docs/goals/M23j.md). A recorder is started with
// `record_pack_level` of them (net/classic_record.hpp's `record_start` and `mvd_start`): 0 plain,
// else the level, so a recording is a `.qwdz`/`.mvdz` (or the voice forms) unless the player says
// not.
//
// ge:summary The demo_pack and demo_pack_level cvars: recordings packed by default, at which level
// ge:keywords demo_pack demo_pack_level cvar record packed zstd level recording-settings
// ge:keywords bind_record_cvars record_pack_level
// ge:skill shooter console
// ge:test tests/shooters_edge_classic_record_test.cpp
#pragma once

namespace ge::console {
class Console;
}

namespace shooters_edge::client {

inline constexpr int kDefaultPackLevel = 19;

struct RecordSettings {
  int pack = 1;                   // demo_pack: 1 packed as written, 0 plain
  int level = kDefaultPackLevel;  // demo_pack_level: zstd's 1 (fastest) .. 22 (smallest)
};

// The two cvars bound to `s` on `console`.
void bind_record_cvars(ge::console::Console& console, RecordSettings& s);
// The level a recorder starts at: 0 when `demo_pack` is off, else `demo_pack_level`.
int record_pack_level(const RecordSettings& s);

}  // namespace shooters_edge::client
