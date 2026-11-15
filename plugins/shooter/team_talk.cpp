// What a team message means (team_talk.hpp): the kinds' and plans' names, for logs and labs.
//
// ge:summary The names of a team message's kinds and the team's plans
// ge:keywords say_kind_name strategy_name team-talk
// ge:skill shooter bots
// ge:test tests/shooters_edge_talk_test.cpp
#include "shooter/team_talk.hpp"

namespace ge::shooter {

const char* say_kind_name(SayKind k) {
  switch (k) {
    case SayKind::none:
      return "none";
    case SayKind::report:
      return "report";
    case SayKind::took:
      return "took";
    case SayKind::safe:
      return "safe";
    case SayKind::lost:
      return "lost";
    case SayKind::help:
      return "help";
    case SayKind::coming:
      return "coming";
    case SayKind::point:
      return "point";
    case SayKind::enemy:
      return "enemy";
    case SayKind::enemy_power:
      return "enemy power";
    case SayKind::have:
      return "have";
    case SayKind::get:
      return "get";
    case SayKind::over:
      return "over";
    case SayKind::attack:
      return "attack";
    case SayKind::plan:
      return "plan";
    case SayKind::pack:
      return "pack";
    case SayKind::ack:
      return "ack";
    case SayKind::trap:
      return "trap";
    case SayKind::timer:
      break;
  }
  return "timer";
}

const char* strategy_name(Strategy s) {
  switch (s) {
    case Strategy::none:
      return "none";
    case Strategy::take:
      return "take";
    case Strategy::camp:
      return "camp";
    case Strategy::free:
      return "free";
  }
  return "none";
}

}  // namespace ge::shooter
