// Fall Line's runs on a hub, defined (runs.hpp): the log parsed and fitted to the game, the run
// stepped to its end, its racers' times read off the state.
//
// ge:summary Fall Line's runs on a hub defined: the log played out, the racers' finish times
// ge:keywords hub run input-log run_row finish-time
// ge:skill net sim
// ge:test tests/fall_line_hub_runs_test.cpp
#include "fall_line/hub/runs.hpp"

#include <memory>
#include <optional>
#include <string_view>

#include "core/num.hpp"
#include "core/num_text.hpp"
#include "fall_line/sim/play.hpp"
#include "fall_line/sim/state.hpp"
#include "play/inputs.hpp"
#include "play/session.hpp"

namespace fall_line::hub {

bool run_row(std::span<const uint8_t> log, ge::hub::DemoRow& out, std::string& error) {
  const std::string_view text(reinterpret_cast<const char*>(log.data()), log.size());
  const std::optional<ge::play::InputLog> parsed = ge::play::parse_inputs(text, error);
  if (!parsed) return false;
  if (!ge::play::fits(*parsed, sim::kGame, error)) return false;
  ge::play::Setup setup;
  if (!ge::play::setup_of(sim::kGame, *parsed, setup, error)) return false;
  std::unique_ptr<ge::play::Run> run = sim::kGame.open(setup, error);
  if (!run) return false;
  const int64_t steps = parsed->steps();
  for (int64_t i = 0; i < steps && !run->over(); i++) run->step(parsed->at(i));
  const sim::GameState* s = sim::state_of(*run);
  if (s == nullptr) {
    error = "the run opened no Fall Line state";
    return false;
  }
  out = ge::hub::DemoRow{};
  out.game = sim::kGame.name;
  out.game_version = sim::kGame.version;
  const std::string seed = ge::core::num::to_string(parsed->seed);
  out.map = "seed " + seed;
  out.mode = s->mode == sim::Mode::free ? "free ride" : "race";
  const size_t rivals = s->racers.empty() ? 0 : s->racers.size() - 1;
  out.title = std::string(s->mode == sim::Mode::free ? "A free ride" : "A race") + " on seed " +
              seed +
              (rivals ? ", " + std::to_string(rivals) + (rivals == 1 ? " rival" : " rivals")
                      : std::string());
  out.duration_ms = static_cast<int64_t>(static_cast<double>(steps) * 1000.0 / parsed->hz);
  for (size_t i = 0; i < s->racers.size(); i++) {
    ge::hub::DemoPlayer p;
    p.name = i == 0 ? std::string("Player") : "Rival " + std::to_string(i);
    const sim::Progress& progress = s->racers[i].progress;
    if (progress.finished) {
      p.score = static_cast<int32_t>(ge::core::num::round(progress.finish_time * 1000.0));
      p.scored = true;
    }
    out.players.push_back(std::move(p));
  }
  return true;
}

}  // namespace fall_line::hub
