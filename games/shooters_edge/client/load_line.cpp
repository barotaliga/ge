// The console held down while a map loads, and let go once the load is done or the world shown is
// another's (load_line.hpp).
//
// ge:summary The console held down while a map loads, and let go once the world is another's
// ge:keywords load-line console-load loading-console held-console join-while-loading
// ge:skill shooter ui
// ge:test tests/shooters_edge_q2_join_test.cpp
#include "shooters_edge/client/load_line.hpp"

#include <cstdio>

namespace shooters_edge::client {

void hold_load_line(std::string_view what, double since_s, LoadLine& line,
                    ge::console::Console& con, ge::console::ConsoleView& view, double now_s) {
  char text[160];
  std::snprintf(text, sizeof text, "Loading %.*s... %.1fs", static_cast<int>(what.size()),
                what.data(), now_s - since_s);
  if (line.what != what || !view.rewrite(line.n, text)) {
    con.print(text);
    line.what = std::string(what);
    line.n = view.newest();
  }
  view.set_full(true);
  line.held = true;
}

void release_load_line(bool loading, bool elsewhere, LoadLine& line, ge::console::ConsoleView& view,
                       bool keep) {
  if (!line.held || (loading && !elsewhere)) return;
  line.held = false;
  line.what.clear();
  if (!keep) view.set_full(false);
}

}  // namespace shooters_edge::client
