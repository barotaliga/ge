// Spectating Fall Line by inputs (engine/replay/spectate.hpp) and through a TV tree
// (engine/tv/trial.hpp): the bot's run written as a replay stream while it is played; a viewer
// following the whole stream, one joining at a later keyframe, and every viewer through a source
// node and a relay under every simulated network preset each step their own run to the player's
// digest at the player's last step.
//
// ge:summary Tests spectating Fall Line by inputs: whole, late, and through a TV tree, by digest
// ge:keywords test spectate spectating fall_line inputs keyframe digest tv relay preset late-joiner
// ge:skill replay testing
#include <string>
#include <vector>

#include "check.hpp"
#include "fall_line/sim/play.hpp"
#include "net/conditions.hpp"
#include "play/game.hpp"
#include "play/session.hpp"
#include "replay/spectate.hpp"
#include "replay/stream.hpp"
#include "tv/trial.hpp"

namespace gp = ge::play;
namespace rp = ge::replay;
namespace fs = fall_line::sim;

namespace {

struct Played {
  std::vector<uint8_t> stream;
  int64_t steps = 0;
  uint64_t digest = 0;
};

// The bot down seed 38 for `seconds`, written as it goes, a keyframe every `key` seconds, every
// `full`-th one whole.
Played play(double seconds, int key, int full = 6) {
  Played p;
  gp::Setup setup;
  std::string error;
  CHECK_MSG(gp::make_setup(fs::kGame, 38, {}, setup, error), error);
  gp::Session session(fs::kGame, setup);
  CHECK_MSG(session.ok(), session.error());
  rp::SpectateWriter writer(fs::kGame, setup, p.stream, {key, full});
  CHECK_MSG(writer.start(session.run(), error), error);
  std::vector<double> input(fs::kGame.axes.size(), 0), snapped(input.size(), 0);
  const auto steps = static_cast<int64_t>(seconds * fs::kGame.hz);
  for (int64_t s = 0; s < steps && !session.done(); s++) {
    std::fill(input.begin(), input.end(), 0);
    session.run().pilot(input);
    for (size_t i = 0; i < input.size(); i++)
      snapped[i] = gp::snap_axis(input[i], fs::kGame.axes[i].kind);
    session.step(input);
    CHECK_MSG(writer.step(snapped, session.run(), error), error);
  }
  writer.finish(session.run());
  p.steps = session.steps();
  p.digest = session.run().digest();
  return p;
}

}  // namespace

TEST(a_viewer_from_the_start_and_one_joining_late_reach_the_players_digest) {
  // Whole keyframes at 0, 10 and 20 s, deltas at 5, 15 and 25 s.
  const Played p = play(25, 5, 2);
  const auto hz = static_cast<int64_t>(fs::kGame.hz);
  rp::Spectator whole;
  whole.skip_ahead = false;
  CHECK_MSG(rp::spectate_follow(whole, fs::kGame, p.stream), whole.error);
  CHECK(whole.started == 0 && whole.deltas == 0);
  CHECK(whole.step == p.steps && whole.digest == p.digest);
  // Handed the whole stream, a viewer skips to the newest keyframe: the delta at 25 s.
  rp::Spectator skipper;
  CHECK_MSG(rp::spectate_follow(skipper, fs::kGame, p.stream), skipper.error);
  CHECK(skipper.started == 25 * hz && skipper.deltas == 1);
  CHECK(skipper.step == p.steps && skipper.digest == p.digest);
  // Joined at the keyframe block of second 10 while the player is at second 17: the header, the
  // blocks from 10 on. It rebuilds the save at 15 from 10's and the delta and steps on from there.
  const auto trailer = rp::read_trailer(p.stream);
  CHECK(trailer.has_value() && trailer->index.size() > 18);
  if (!trailer || trailer->index.size() <= 18) return;
  CHECK(trailer->index[10].keyframe && !trailer->index[15].keyframe);
  rp::StreamReader r;
  std::string error;
  CHECK(rp::read_stream_header(p.stream, r, error));
  std::vector<uint8_t> late(p.stream.begin(), p.stream.begin() + static_cast<std::ptrdiff_t>(r.at));
  const auto from = static_cast<std::ptrdiff_t>(trailer->index[10].offset);
  const auto cut = static_cast<std::ptrdiff_t>(trailer->index[18].offset);
  late.insert(late.end(), p.stream.begin() + from, p.stream.begin() + cut);
  rp::Spectator joiner;
  CHECK_MSG(rp::spectate_follow(joiner, fs::kGame, late), joiner.error);
  CHECK(joiner.started == 15 * hz && joiner.deltas == 1);
  CHECK(joiner.step == 18 * hz);
  late.insert(late.end(), p.stream.begin() + cut, p.stream.end());
  CHECK_MSG(rp::spectate_follow(joiner, fs::kGame, late), joiner.error);
  CHECK(joiner.step == p.steps && joiner.digest == p.digest);
}

TEST(a_delta_keyframe_is_a_small_part_of_a_whole_one) {
  const Played p = play(20, 5);
  rp::StreamReader r;
  std::string error;
  CHECK(rp::read_stream_header(p.stream, r, error));
  rp::Block b;
  std::vector<size_t> whole, delta;
  while (rp::next_block(r, b, error))
    for (const rp::Chunk& c : b.chunks)
      if (c.track == rp::kKeyframeTrack) (b.keyframe ? whole : delta).push_back(c.bytes.size());
  CHECK(whole.size() == 1 && delta.size() == 4);
  // A delta is the save's size XORed: plain, as big; the block's zstd is what shrinks it.
  CHECK(!delta.empty() && delta[0] > whole[0] / 2);
  const auto t = rp::read_trailer(p.stream);
  CHECK(t.has_value() && t->index.size() > 6);
  if (!t || t->index.size() <= 6) return;
  const auto bytes = [&](size_t i) { return t->index[i + 1].offset - t->index[i].offset; };
  CHECK_MSG(bytes(5) * 20 < bytes(0), std::to_string(bytes(5)) +
                                          " bytes for the delta's block against " +
                                          std::to_string(bytes(0)) + " for the whole one's");
}

TEST(the_stream_fed_in_pieces_is_followed_as_it_grows) {
  const Played p = play(6, 2);
  rp::Spectator s;
  std::vector<uint8_t> so_far;
  for (size_t at = 0; at < p.stream.size(); at += 1000) {
    const size_t to = std::min(p.stream.size(), at + 1000);
    so_far.insert(so_far.end(), p.stream.begin() + static_cast<std::ptrdiff_t>(at),
                  p.stream.begin() + static_cast<std::ptrdiff_t>(to));
    CHECK_MSG(rp::spectate_follow(s, fs::kGame, so_far), s.error);
  }
  CHECK(s.step == p.steps && s.digest == p.digest);
}

TEST(a_stream_of_another_game_or_rules_version_is_refused) {
  const Played p = play(2, 1);
  gp::GameSpec other = fs::kGame;
  other.version = fs::kGame.version + 1;
  rp::Spectator s;
  CHECK(!rp::spectate_follow(s, other, p.stream));
  CHECK_MSG(s.error.find("version") != std::string::npos, s.error);
}

TEST(viewers_through_a_tv_tree_reach_the_players_digest_under_every_preset) {
  const Played p = play(20, 5);
  for (const std::string& name : ge::net::preset_names()) {
    ge::tv::TrialConfig c;
    CHECK(ge::net::conditions_preset(name, c.link));
    c.source = &p.stream;
    c.keep_streams = true;
    const ge::tv::TrialResult r = ge::tv::run_tv_trial(c);
    CHECK_MSG(r.all_whole, name + ": " + r.why);
    for (const ge::tv::TrialViewer& v : r.viewers) {
      rp::Spectator s;
      CHECK_MSG(rp::spectate_follow(s, fs::kGame, v.stream), name + ": " + s.error);
      CHECK_MSG(
          s.step == p.steps && s.digest == p.digest,
          name + ": viewer at step " + std::to_string(s.step) + " of " + std::to_string(p.steps));
    }
  }
}
