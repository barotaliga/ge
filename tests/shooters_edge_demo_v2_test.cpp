// Shooter's Edge's face on the replay file version 2 (games/shooters_edge/client/demo_v2.hpp):
// every recording comes back byte for byte (multiview walked and field-coded, client and
// single-player kept as bytes); a multiview recording packs smaller than zstd alone; a recording
// cut short keeps its last bytes; a file cut short gives back its whole blocks; another game's file
// is refused.
//
// ge:summary Tests: classic recordings to replay v2 and back byte exact, smaller, cut, refused
// ge:keywords test replay-v2 face multiview round-trip pack unpack cut truncated
// ge:skill replay shooter testing
// ge:test tests/shooters_edge_demo_v2_test.cpp
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "check.hpp"
#include "replay/stream.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_v2.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "vfs/zstd.hpp"

namespace se = shooters_edge::client;

namespace {

se::DemoScript script() { return se::loop_script("halls", {64, 32, 24}, 160, 2400); }

se::MatchScript match() {
  se::MatchScript m;
  m.frames = 1200;
  for (int p = 0; p < 4; p++) {
    se::MatchPlayer pl;
    pl.name = "player" + std::to_string(p);
    for (int i = 0; i < m.frames; i++) {
      const auto t = static_cast<float>(i);
      pl.origins.push_back({static_cast<float>(p * 100) + t * 0.75f, 40.0f - t * 0.25f, 24});
      pl.health.push_back(100 - (i / 50 + p) % 90);
      pl.frags.push_back(i / 200 + p);
    }
    pl.weapon = {32};
    pl.armour = {100};
    m.players.push_back(pl);
  }
  for (int r = 0; r < 40; r++)
    m.rockets.push_back({r * 30, r * 30 + 20, {0, 0, 24}, {static_cast<float>(r * 10), 300, 24}});
  return m;
}

bool round_trip(const std::vector<uint8_t>& rec, std::string& why,
                se::DemoPackReport* rep = nullptr, size_t* packed = nullptr) {
  std::vector<uint8_t> file, back;
  std::string error;
  if (!se::pack_demo(rec, file, error, {}, rep)) {
    why = "pack: " + error;
    return false;
  }
  if (packed != nullptr) *packed = file.size();
  if (!se::unpack_demo(file, back, error) || !error.empty()) {
    why = "unpack: " + error;
    return false;
  }
  if (back != rec) {
    size_t at = 0;
    while (at < back.size() && at < rec.size() && back[at] == rec[at]) at++;
    why = "the bytes differ from byte " + std::to_string(at) + " of " + std::to_string(rec.size()) +
          " (got " + std::to_string(back.size()) + ")";
    return false;
  }
  return true;
}

}  // namespace

TEST(a_multiview_recording_comes_back_byte_for_byte) {
  for (const auto& rec : {se::write_multiview_demo(script()), se::write_multiview_match(match())}) {
    std::string why;
    se::DemoPackReport rep;
    CHECK_MSG(round_trip(rec, why, &rep), why);
    CHECK(rep.format == "multiview");
    CHECK_MSG(rep.walked > rep.kept, "walked " + std::to_string(rep.walked) + ", kept " +
                                         std::to_string(rep.kept) + " as bytes");
    CHECK(rep.keyframes >= 1 && rep.blocks >= rep.keyframes);
  }
}

TEST(client_and_single_player_recordings_come_back_as_bytes) {
  for (const auto& rec : {se::write_client_demo(script()), se::write_single_demo(script())}) {
    std::string why;
    se::DemoPackReport rep;
    CHECK_MSG(round_trip(rec, why, &rep), why);
    CHECK(rep.format == "bytes");
  }
}

TEST(a_multiview_recording_packs_smaller_than_zstd_alone) {
  const auto rec = se::write_multiview_match(match());
  std::string why;
  size_t packed = 0;
  CHECK_MSG(round_trip(rec, why, nullptr, &packed), why);
  const size_t zstd = ge::vfs::zstd_pack(rec).size();
  CHECK_MSG(packed < zstd, "the face packed " + std::to_string(packed) + " bytes, zstd " +
                               std::to_string(zstd) + "; next: a model stopped predicting");
}

TEST(a_recording_cut_inside_a_record_keeps_its_tail) {
  auto rec = se::write_multiview_match(match());
  rec.resize(rec.size() - 37);
  std::string why;
  se::DemoPackReport rep;
  CHECK_MSG(round_trip(rec, why, &rep), why);
  CHECK(rep.tail > 0);
}

TEST(a_file_cut_short_gives_back_its_whole_blocks) {
  const auto rec = se::write_multiview_match(match());
  std::vector<uint8_t> file, back;
  std::string error;
  CHECK(se::pack_demo(rec, file, error));
  const auto t = ge::replay::read_trailer(file);
  CHECK(t.has_value() && t->index.size() > 4);
  if (!t || t->index.size() <= 4) return;
  file.resize(static_cast<size_t>(t->index[4].offset) + 20);
  CHECK(se::unpack_demo(file, back, error));
  CHECK(!error.empty());
  CHECK(!back.empty() && back.size() < rec.size());
  CHECK(std::equal(back.begin(), back.end(), rec.begin()));
}

TEST(another_games_file_is_refused) {
  std::vector<uint8_t> file, back;
  ge::replay::StreamWriter w;
  ge::replay::StreamHeader h;
  h.game = "fall_line";
  ge::replay::stream_begin(w, h, file);
  ge::replay::stream_finish(w, {});
  std::string error;
  CHECK(!se::unpack_demo(file, back, error));
  CHECK_MSG(error.find("fall_line") != std::string::npos, error);
}
