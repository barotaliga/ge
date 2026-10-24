// THE LIVE RUN'S RECORDER AND THE INSTANT REPLAY (docs/replays.md § The replay file and the
// instant replay). A `Live` is a play session (play/session.hpp: every input snapped, recorded on
// the input log and stepped) that also keeps keyframes as it goes (keep.hpp) and the marks the
// game files, so at any moment it can hand out:
//
// - THE FILE: the log so far with its marks and the game's version (file.hpp), to save;
// - THE INSTANT REPLAY: "the last ten seconds" is a `Player` (player.hpp) handed the log so far
//   and the live keeper itself (moved, never copied), sought back. The live run is set aside
//   untouched; `resume` takes the keeper back, with any copies the replay took, and the run goes on
//   as if nothing had been watched. The keeper is dense near the present (every copy of the last
//   `near_s` seconds), so the seek is a clone and a couple of seconds of steps.
//
// While a replay is out the live run may still be stepped (a game that keeps playing behind the
// replay); it takes no copies until the keeper is back, which costs a later seek steps, never a
// wrong frame.
//
// ge:summary The live run recorded with keyframes and marks; the instant replay handed out and back
// ge:keywords replay live record recorder instant-replay watch resume mark marks back-date file
// ge:keywords keeper adopt session last-seconds
// ge:skill replay
// ge:test tests/replay_live_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "play/game.hpp"
#include "play/session.hpp"
#include "replay/file.hpp"
#include "replay/keep.hpp"
#include "replay/player.hpp"

namespace ge::replay {

class Live {
 public:
  // A new run of `spec` on `setup`; `ok()` says whether it opened, `error` why not. `policy` must
  // have no `policy_problem` at the spec's step rate.
  Live(const play::GameSpec& spec, const play::Setup& setup, const KeepPolicy& policy = {});
  bool ok() const { return error_.empty() && session_.ok(); }
  const std::string& error() const { return error_.empty() ? session_.error() : error_; }

  // One step on `input`, snapped, recorded and stepped (Session::step), and a copy offered.
  void step(std::span<const double> input);
  int64_t steps() const { return session_.steps(); }
  bool done() const { return session_.done(); }
  play::Run& run() { return session_.run(); }
  const play::Run& run() const { return session_.run(); }
  play::Session& session() { return session_; }
  const Keeper& keeper() const { return keeper_; }

  // A moment filed now, or back-dated to `step` (a flight's take-off, known at its landing).
  // `kind` must be `mark_kind_ok`; one that is not is dropped and counted in `bad_marks`.
  void mark(const std::string& kind, double weight = 1);
  void mark_at(int64_t step, const std::string& kind, double weight = 1);
  const std::vector<Mark>& marks() const { return marks_; }
  int32_t bad_marks() const { return bad_marks_; }

  // The run so far as a replay file: the log (its digest filled), the marks, the game's version.
  // `recorded` is the app's text for when (the engine reads no clock).
  ReplayFile file(const std::string& recorded = "") const;

  // THE INSTANT REPLAY: a player over the log so far, holding the live keeper, sought to
  // `seconds_back` before now (held at the run's start). Null while a replay is already out or
  // when the log does not open (`error` then says why).
  std::unique_ptr<Player> watch(double seconds_back);
  // The keeper taken back from the replay `watch` handed out.
  void resume(Player& replay);
  bool watching() const { return watching_; }

 private:
  const play::GameSpec& spec_;
  play::Session session_;
  Keeper keeper_;
  std::vector<Mark> marks_;
  int32_t bad_marks_ = 0;
  bool watching_ = false;
  std::string error_;
};

}  // namespace ge::replay
