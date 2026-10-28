// engine/net/local_pipe.hpp: bytes cross a local pipe both ways in order however much is queued,
// a closed end is seen by the other, and a pipe nobody serves, or a path the OS cannot name, is
// refused with a line that says what to do. A machine with no local socket (a sandbox) skips.
//
// ge:summary Test: local pipes carry bytes both ways, see a close, refuse a missing server
// ge:keywords test net local pipe unix socket named pipe round-trip close
// ge:skill net testing
#include "net/local_pipe.hpp"

#include <filesystem>
#include <string>

#include "check.hpp"

using namespace ge::net;

TEST(bytes_cross_a_local_pipe_both_ways_and_a_close_is_seen) {
  const std::string path =
      local_pipe_path(std::filesystem::temp_directory_path().string(), "ge-local-pipe-test");
  LocalListener l;
  std::string err;
  if (!local_listen(l, path, &err)) {
    ge::test::skip("no local pipe here: " + err);
    return;
  }
  LocalPipe c, s;
  CHECK_MSG(local_connect(c, path, &err), err);
  bool accepted = false;
  for (int i = 0; i < 100000 && !accepted; i++) accepted = local_accept(l, s);
  CHECK(accepted);
  std::string big;
  for (int i = 0; i < 200000; i++) big.push_back(static_cast<char>('a' + i % 26));
  CHECK(local_send(c, big) && local_send(s, "pong"));
  std::string at_s, at_c;
  for (int i = 0; i < 1000000 && (at_s.size() < big.size() || at_c.size() < 4); i++) {
    local_pump(c);
    local_pump(s);
    at_s += s.in;
    s.in.clear();
    at_c += c.in;
    c.in.clear();
  }
  CHECK(at_s == big && at_c == "pong");
  local_close(c);
  for (int i = 0; i < 100000 && !s.closed; i++) local_pump(s);
  CHECK(s.closed && !local_send(s, "late"));
  local_close(s);
  local_close_listener(l);
}

TEST(a_pipe_nobody_serves_is_refused_with_what_to_do) {
  LocalPipe p;
  std::string err;
  const std::string path =
      local_pipe_path(std::filesystem::temp_directory_path().string(), "ge-local-pipe-nobody");
  CHECK(!local_connect(p, path, &err) && err.find("next:") != std::string::npos);
#if !defined(_WIN32)
  CHECK(!local_connect(p, "/" + std::string(200, 'x'), &err) &&
        err.find("shorter") != std::string::npos);
#endif
  CHECK(local_process_id() > 0);
}
