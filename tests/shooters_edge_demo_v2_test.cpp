// Shooter's Edge's face on the replay file version 2 (games/shooters_edge/client/demo_v2.hpp):
// every recording comes back byte for byte (multiview walked and field-coded, client and
// single-player kept as bytes); a multiview recording packs smaller than zstd alone; a recording
// cut short keeps its last bytes; a file cut short gives back its whole blocks; another game's file
// is refused; the demo player opens a replay file as the recording it holds; a packed match carries
// the hub's words (map, mode, server, players, teams, scores).
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

TEST(the_demo_player_opens_a_replay_file) {
  const auto rec = se::write_multiview_match(match());
  std::vector<uint8_t> file;
  std::string error;
  CHECK(se::pack_demo(rec, file, error));
  se::Demo d;
  CHECK_MSG(se::open_demo(file, d, error), error);
  CHECK(d.replay_v2 && d.state.format == se::DemoFormat::multiview && !d.truncated);
}

namespace {

std::vector<std::string> values(const ge::replay::Meta& meta, const std::string& key) {
  std::vector<std::string> out;
  for (const auto& [k, v] : meta)
    if (k == key) out.push_back(v);
  return out;
}

}  // namespace

TEST(a_packed_match_carries_the_hubs_words) {
  se::MatchScript m = match();
  m.map = "arena";
  m.serverinfo = {{"hostname", "Night League"}, {"teamplay", "2"}};
  for (size_t p = 0; p < m.players.size(); p++) m.players[p].team = p % 2 == 0 ? "blue" : "red";
  const auto rec = se::write_multiview_match(m);
  std::vector<uint8_t> file;
  std::string error;
  se::DemoPackOptions options;
  options.meta = {{"title", "Final"}, {"tag", "league"}};
  CHECK(se::pack_demo(rec, file, error, options));
  ge::replay::StreamReader r;
  CHECK_MSG(ge::replay::read_stream_header(file, r, error), error);
  const ge::replay::Meta& h = r.header.meta;
  CHECK(values(h, "map") == std::vector<std::string>{"arena"});
  CHECK(values(h, "mode") == std::vector<std::string>{"2on2"});
  CHECK(values(h, "server") == std::vector<std::string>{"Night League"});
  CHECK((values(h, "player") == std::vector<std::string>{"player0\tblue", "player1\tred",
                                                         "player2\tblue", "player3\tred"}));
  CHECK((values(h, "team") == std::vector<std::string>{"blue", "red"}));
  CHECK(values(h, "title") == std::vector<std::string>{"Final"});
  CHECK(values(h, "tag") == std::vector<std::string>{"league"});
  const auto t = ge::replay::read_trailer(file);
  CHECK(t.has_value());
  if (!t) return;
  // Each player's last frags: frame 1199 / 200 + the slot.
  CHECK((values(t->meta, "score") ==
         std::vector<std::string>{"player0\t5", "player1\t6", "player2\t7", "player3\t8"}));
  CHECK((values(t->meta, "team_score") == std::vector<std::string>{"blue\t12", "red\t14"}));
  CHECK(values(t->meta, "duration_ms").size() == 1);
  std::vector<uint8_t> back;
  CHECK(se::unpack_demo(file, back, error) && back == rec);
}

TEST(a_match_without_teams_is_named_by_its_players) {
  const auto rec = se::write_multiview_match(match());
  std::vector<uint8_t> file;
  std::string error;
  CHECK(se::pack_demo(rec, file, error));
  ge::replay::StreamReader r;
  CHECK_MSG(ge::replay::read_stream_header(file, r, error), error);
  CHECK(values(r.header.meta, "mode") == std::vector<std::string>{"ffa"});
  CHECK(values(r.header.meta, "map") == std::vector<std::string>{"halls"});
  CHECK(values(r.header.meta, "server").empty());
}
