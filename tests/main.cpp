// Runs every TEST registered in this executable, prints one line per test. Exit 0 all
// passed, 1 any failed, 77 skipped (ge::test::skip: the machine cannot run it).
//
// ge:summary Runs every TEST registered in a test executable
// ge:keywords test runner main ctest
// ge:skill testing
#include <cstdio>

#include "check.hpp"

int main() {
  int ran = 0;
  for (ge::test::Case* c = ge::test::cases(); c; c = c->next) {
    const int before = ge::test::failures();
    c->fn();
    ++ran;
    std::printf("%s %s\n", ge::test::failures() == before ? "ok  " : "FAIL", c->name);
  }
  std::printf("%d tests, %d failures\n", ran, ge::test::failures());
  if (ge::test::failures() != 0) return 1;
  return ge::test::skipped() ? 77 : 0;  // 77: CTest's SKIP_RETURN_CODE for gpu_ and platform_
}
