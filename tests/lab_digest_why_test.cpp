// The digest ledger (Lab::ledger, digest.tsv) and its explanation (digest_why.hpp): every hashed
// input gets a line, a measured one none, and two ledgers compared name the metric, the row's
// column, the shot and the file that moved, what came and went, and a reorder.
//
// ge:summary Tests the digest ledger and ge digest --why's explanation of a moved digest
// ge:keywords test lab digest why ledger explain moved metric row column shot file
// ge:skill labs testing
#include <string>
#include <vector>

#include "check.hpp"
#include "lab/digest_why.hpp"
#include "lab/lab.hpp"

using namespace ge::lab;

namespace {

void nothing(Lab&) {}
const LabSpec kSpec{"why-toy", LabKind::sim, "engine", "tests/lab_digest_why_test.cpp",
                    "labs",    "Why?",       {},       "",
                    nothing};

struct Run {
  double speed = 31.25;
  std::string cell = "12";
  bool extra = false;
  Rgba colour{0, 90, 0};
  std::vector<uint8_t> wav{1, 2, 3};
};

std::vector<LedgerLine> ledger_of(const Run& r) {
  Lab lab(kSpec, {});
  lab.metric("speed", r.speed, "m/s");
  lab.metric("wall", 2.0, "ms", Det::measured);
  lab.table({"gates", {"gate", "time", "missed"}, {{"1", r.cell, "no"}, {"2", "20", "no"}}});
  if (r.extra) lab.metric("jumps", 3, "count");
  lab.shot({"box", "A BOX", "A box.", "", {}}, create_picture(8, 8, r.colour));
  lab.shot({"gpu", "A FRAME", "A frame.", "", {}}, create_picture(8, 8, r.colour), Det::measured);
  lab.attach("run.wav", r.wav);
  std::string text;
  for (const std::string& l : lab.ledger()) text.append(l).append("\n");
  return parse_ledger(text);
}

bool has(const DigestWhy& w, const std::string& line) {
  for (const std::string& l : w.lines) {
    if (l == line) return true;
  }
  return false;
}

}  // namespace

TEST(the_ledger_lists_what_the_digest_hashed) {
  const auto l = ledger_of({});
  // speed, the columns, two rows, two shots, the file; the measured metric is left out.
  CHECK(l.size() == 7);
  CHECK(l[0].kind == "metric" && l[0].name == "speed" && l[0].value == "31.25");
  CHECK(l[1].kind == "columns" && l[1].name == "gates");
  CHECK(l[2].kind == "row" && l[2].name == "gates / 1" && l[2].value == "1\t12\tno");
  CHECK(l[2].columns.size() == 3 && l[2].columns[1] == "time");
  CHECK(l[5].kind == "shot" && l[5].name == "gpu" && l[5].value == "measured");
  CHECK(l[6].kind == "file" && l[6].value.ends_with(" 3 bytes"));
}

TEST(the_same_run_explains_to_nothing) {
  const DigestWhy w = explain_digest(ledger_of({}), ledger_of({}));
  CHECK(w.same());
  CHECK(w.lines.empty());
}

TEST(a_moved_digest_is_named_by_what_moved) {
  Run moved;
  moved.speed = 31.5;
  moved.cell = "13";
  moved.extra = true;
  moved.colour = {200, 0, 0};
  moved.wav = {1, 2, 3, 4};
  const DigestWhy w = explain_digest(ledger_of({}), ledger_of(moved));
  CHECK(w.changed == 4 && w.added == 1 && w.removed == 0);
  CHECK(has(w, "metric speed: 31.25 -> 31.5"));
  CHECK(has(w, "row gates / 1: time 12 -> 13"));
  CHECK(has(w, "+ metric jumps: 3"));
  CHECK(has(w, "file run.wav: its bytes changed (3 bytes -> 4 bytes)"));
  bool shot = false;
  for (const std::string& l : w.lines) shot = shot || l.starts_with("shot box: its pixels changed");
  CHECK(shot);
  // The measured frame's pixels moved too, and are not named.
  for (const std::string& l : w.lines) CHECK(!l.starts_with("shot gpu"));
  // Backwards, the extra metric went.
  CHECK(has(explain_digest(ledger_of(moved), ledger_of({})), "- metric jumps"));
}

TEST(a_reorder_and_the_limit_are_said) {
  const auto a = parse_ledger("metric\tx\t1\nmetric\ty\t2\n");
  const auto b = parse_ledger("# header\nmetric\ty\t2\nmetric\tx\t1\n");
  const DigestWhy w = explain_digest(a, b);
  CHECK(w.reordered && !w.same());
  CHECK(w.lines.size() == 1 && w.lines[0].starts_with("the same inputs hashed in another order"));

  std::string many;
  for (int i = 0; i < 10; i++) many += "metric\tm" + std::to_string(i) + "\t" + "1\n";
  const DigestWhy capped = explain_digest({}, parse_ledger(many), 4);
  CHECK(capped.added == 10 && capped.lines.size() == 5);
  CHECK(capped.lines[4] == "... and 6 more (the two digest.tsv files hold them all)");
}
