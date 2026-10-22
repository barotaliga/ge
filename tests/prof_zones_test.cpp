// The profiler (engine/prof): a zone records one span with its depth and its allocations only
// while a recording runs, totals fold spans into each zone's own and inclusive time, the room
// a recording reserves drops what does not fit and says how much, and the counting operator new
// counts every allocation and its bytes.
//
// ge:summary Tests: timing zones, their totals, a recording's room, and allocation counting
// ge:keywords test prof zones ge_zone spans totals exclusive inclusive dropped allocations
// ge:skill perf testing
// ge:test tests/prof_zones_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "check.hpp"
#include "prof/memory.hpp"
#include "prof/zones.hpp"

namespace prof = ge::prof;

namespace {

void inner() {
  GE_ZONE("test.inner");
  // A call of the allocation function itself: an optimiser may drop a new-expression whose
  // block it can see is never used, but not this.
  void* block = ::operator new(16);
  ::operator delete(block);
}

void outer() {
  GE_ZONE("test.outer");
  inner();
  inner();
}

const prof::ZoneTotal* row(const std::vector<prof::ZoneTotal>& rows, const char* name) {
  for (const prof::ZoneTotal& t : rows) {
    if (prof::zone_name(t.name) == name) return &t;
  }
  return nullptr;
}

}  // namespace

TEST(a_zone_records_nothing_until_a_recording_starts) {
  prof::clear();
  outer();
  CHECK(prof::spans().empty());
}

TEST(zones_nest_and_their_totals_split_own_time_from_their_childrens) {
  prof::count_allocations();
  prof::start(64);
  outer();
  prof::stop();
  const auto spans = prof::spans();
  CHECK_MSG(spans.size() == 3, std::to_string(spans.size()));
  CHECK(prof::zone_name(spans[0].name) == "test.outer" && spans[0].depth == 0);
  CHECK(prof::zone_name(spans[1].name) == "test.inner" && spans[1].depth == 1);
  CHECK(spans[0].dur_ns >= spans[1].dur_ns + spans[2].dur_ns);
  // Each inner zone made one allocation; the outer sees both of them.
  CHECK(spans[1].allocs == 1 && spans[2].allocs == 1 && spans[0].allocs == 2);
  std::vector<prof::ZoneTotal> rows;
  prof::totals(spans, rows);
  const prof::ZoneTotal* o = row(rows, "test.outer");
  const prof::ZoneTotal* i = row(rows, "test.inner");
  CHECK(o && i && o->calls == 1 && i->calls == 2);
  CHECK(o->incl_ns == spans[0].dur_ns && o->excl_ns == o->incl_ns - i->incl_ns);
  CHECK(o->excl_allocs == 0 && i->excl_allocs == 2);
  prof::clear();
}

TEST(a_recording_keeps_what_fits_and_counts_the_rest) {
  prof::start(2);
  outer();
  prof::stop();
  CHECK(prof::spans().size() == 2 && prof::dropped() == 1);
  prof::start(8);
  CHECK(prof::spans().empty() && prof::dropped() == 0);
  prof::stop();
}

TEST(the_same_name_is_the_same_zone) {
  CHECK(prof::zone_id("test.same") == prof::zone_id("test.same"));
  CHECK(prof::zone_id("test.same") != prof::zone_id("test.other"));
  CHECK(prof::zone_name(prof::zone_id("test.same")) == "test.same");
}

TEST(the_counting_operator_new_counts_each_allocation_and_its_bytes) {
  prof::count_allocations();
  const prof::Allocations a = prof::allocations();
  CHECK(a.counting);
  auto v = std::make_unique<std::vector<char>>(1000);
  const prof::Allocations b = prof::allocations();
  CHECK_MSG(b.count - a.count == 2, std::to_string(b.count - a.count));
  CHECK(b.bytes - a.bytes >= 1000);
  v.reset();
  CHECK(prof::allocations().frees - b.frees == 2);
  CHECK(prof::peak_rss_bytes() > 0);
}
