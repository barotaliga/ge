// The recording settings' cvars bound (record_cvars.hpp).
//
// ge:summary Binds demo_pack and demo_pack_level to the recording settings
// ge:keywords demo_pack demo_pack_level cvar bind
// ge:skill shooter console
// ge:test tests/shooters_edge_classic_record_test.cpp
#include "shooters_edge/client/record_cvars.hpp"

#include "console/console.hpp"

namespace shooters_edge::client {

void bind_record_cvars(ge::console::Console& console, RecordSettings& s) {
  console.cvars().bind({.name = "demo_pack",
                        .fallback = "1",
                        .help = "recordings packed as they are written (.mvdz, .qwdz and the voice "
                                "forms, about 9 times smaller): 1 on, 0 plain",
                        .kind = ge::console::CvarKind::integer,
                        .min = 0,
                        .max = 1},
                       &s.pack);
  console.cvars().bind({.name = "demo_pack_level",
                        .fallback = "19",
                        .help = "the zstd level a packed recording is written at: 1 fastest .. 22 "
                                "smallest (19: the chosen balance)",
                        .kind = ge::console::CvarKind::integer,
                        .min = 1,
                        .max = 22},
                       &s.level);
}

int record_pack_level(const RecordSettings& s) { return s.pack != 0 ? s.level : 0; }

}  // namespace shooters_edge::client
