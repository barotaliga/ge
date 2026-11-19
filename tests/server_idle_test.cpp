// engine/server/idle: sv_idletime: a change of buttons or move, or a chat line, is activity; the
// same keys held or nothing pressed is idle; past the limit the seat is over, never at 0.
//
// ge:summary Test: sv_idletime's activity and limit
// ge:keywords test sv_idletime idle idle_move idle_chat idle_over
// ge:skill net testing
#include "check.hpp"
#include "server/idle.hpp"

using namespace ge::server;

TEST(only_a_change_of_keys_or_a_chat_line_is_activity) {
  IdleWatch w;
  idle_start(w, 10);
  idle_move(w, 20, 0, 0, 0, 0);  // nothing pressed
  CHECK(w.active_at == 10);
  idle_move(w, 30, 1, 400, 0, 0);  // fire and run
  CHECK(w.active_at == 30);
  idle_move(w, 40, 1, 350, 0, 0);  // the same keys held
  CHECK(w.active_at == 30);
  idle_move(w, 45, 1, 400, 0, 0);
  CHECK(w.active_at == 30);
  idle_chat(w, 50);
  CHECK(w.active_at == 50);
  CHECK(!idle_over(w, 60, 10));
  CHECK(idle_over(w, 60.5, 10));
  CHECK(!idle_over(w, 1000, 0));  // off
}
