// engine/lab's contract and runner, through the command line an agent types: the listing, an
// unknown flag, a run's files (metrics.tsv, report.json, the contact sheet, the captioned
// picture), --tag and --compare, --shots, the in-process repeat check, --shard and the digest
// baseline. The
// labs it runs are registered here, so the test holds the lab runner, not any one lab.
//
// ge:summary Tests the lab runner: flags, the report's files, tag and compare, repeat, digests
// ge:keywords test lab runner registry report digest repeat compare caption smoke measured-shot
// ge:keywords shard shots wants
// ge:skill labs testing
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "lab/lab.hpp"
#include "lab/runner.hpp"

namespace fs = std::filesystem;
using namespace ge::lab;

namespace {

void toy(Lab& lab) {
  const double n = lab.arg_num("n");
  lab.metric("n-squared", n * n, "count");
  lab.metric("wall", 1.5, "ms", Det::measured);
  lab.table({"rows", {"a", "b"}, {{"1", "2"}}});
  Picture p = create_picture(32, 16, {0, 90, 0});
  lab.shot({"green", "A GREEN BOX", "A box, green.", "that it is green", {{"n", lab.arg("n")}}}, p);
  lab.attach("frames/0001.txt", {'o', 'k'});
  if (lab.arg_bool("fail")) lab.fail("asked to", "Do not pass --fail.");
}

const LabSpec kToy{"toy",
                   LabKind::sim,
                   "engine",
                   "tests/lab_runner_test.cpp",
                   "labs",
                   "Does the runner work?",
                   {{"n", "3", "a number"}, {"fail", "", "fail on purpose", true}},
                   "--n=2",
                   toy};
GE_LAB(kToy);

int drift_calls = 0;
void drift(Lab& lab) { lab.metric("calls", ++drift_calls, "count"); }
const LabSpec kDrift{"drift",  LabKind::sim,
                     "engine", "a counter kept between runs",
                     "labs",   "Is a lab with hidden state caught?",
                     {},       "",
                     drift};
GE_LAB(kDrift);

// Two shots, shaded by --shade: `asked` is skipped by the lab when not wanted, `dropped` is left
// to shot() to drop, so --shots holds both ways.
int asked_renders = 0;
void pair(Lab& lab) {
  // The runner's --cvars, as a lab with a console reads them: "dark" draws the grey black.
  const auto v = lab.cvars() == "dark" ? uint8_t{0} : static_cast<uint8_t>(lab.arg_num("shade"));
  if (lab.wants("asked")) {
    ++asked_renders;
    lab.shot({"asked", "ASKED", "Skipped by the lab.", "", {}}, create_picture(8, 8, {v, v, v}));
  }
  lab.shot({"dropped", "DROPPED", "Dropped by shot().", "", {}}, create_picture(8, 8, {v, 0, 0}));
}
const LabSpec kPair{"pair",
                    LabKind::picture,
                    "engine",
                    "tests/lab_runner_test.cpp",
                    "labs",
                    "Does --shots draw only the shots it names?",
                    {{"shade", "10", "the grey"}},
                    "",
                    pair};
GE_LAB(kPair);

std::string dir() {
  static const std::string d = (fs::temp_directory_path() / "ge_lab_runner_test").string();
  return d;
}

int run(std::vector<std::string> words) {
  words.insert(words.begin(), "labs");
  words.push_back("--out=" + dir());
  words.push_back("--quiet");
  std::vector<const char*> argv;
  argv.reserve(words.size());
  for (const auto& w : words) argv.push_back(w.c_str());
  return run_labs(static_cast<int>(argv.size()), argv.data());
}

std::string slurp(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  std::stringstream s;
  s << f.rdbuf();
  return s.str();
}

}  // namespace

TEST(registry_is_sorted_and_found) {
  CHECK(find_lab("toy") == &kToy);
  CHECK(find_lab("nope") == nullptr);
  const auto& labs = registered_labs();
  for (size_t i = 1; i < labs.size(); i++) CHECK(std::string(labs[i - 1]->id) < labs[i]->id);
}

TEST(usage_errors_exit_2) {
  CHECK(run({"toy", "--bogus=1"}) == 2);
  CHECK(run({"no-such-lab"}) == 2);
  CHECK(run({"toy", "--n"}) == 2);  // a value flag with no value
}

TEST(a_run_writes_its_files) {
  fs::remove_all(dir());
  CHECK(run({"toy", "--n=4"}) == 0);
  const std::string d = dir() + "/toy";
  CHECK(slurp(d + "/metrics.tsv").find("n-squared\t16\tcount\tdeterministic") != std::string::npos);
  CHECK(slurp(d + "/metrics.tsv").find("wall\t1.5\tms\tmeasured") != std::string::npos);
  CHECK(slurp(d + "/frames/0001.txt") == "ok");  // an attachment in a subdirectory
  const std::string json = slurp(d + "/report.json");
  CHECK(json.find("\"lab\": \"toy\"") != std::string::npos);
  CHECK(json.find("\"look_for\": \"that it is green\"") != std::string::npos);
  const std::string sheet = slurp(d + "/index.md");
  CHECK(sheet.find("![A GREEN BOX](green.png)") != std::string::npos);
  CHECK(sheet.find("**Look for:** that it is green") != std::string::npos);
  CHECK(fs::file_size(d + "/green.png") > size_t{32} * 16 * 4);  // the band makes it bigger
  CHECK(fs::exists(d + "/green.json"));
}

TEST(a_failing_lab_exits_1) { CHECK(run({"toy", "--fail"}) == 1); }

TEST(tag_and_compare) {
  CHECK(run({"toy", "--n=2", "--tag=before"}) == 0);
  CHECK(fs::exists(dir() + "/toy@before/metrics.tsv"));
  CHECK(run({"toy", "--n=5", "--compare=before"}) == 0);
}

TEST(shots_draws_only_the_named_ones) {
  fs::remove_all(dir());
  const std::string d = dir() + "/pair";
  CHECK(run({"pair"}) == 0);
  const std::string asked = slurp(d + "/asked.png"), dropped = slurp(d + "/dropped.png");
  const std::string sheet = slurp(d + "/index.md");
  asked_renders = 0;
  CHECK(run({"pair", "--shade=200", "--shots=dropped"}) == 0);
  CHECK(asked_renders == 0);                    // the lab asked, and skipped the work
  CHECK(slurp(d + "/asked.png") == asked);      // left as the whole run wrote it
  CHECK(slurp(d + "/dropped.png") != dropped);  // drawn again at the new shade
  CHECK(slurp(d + "/index.md") == sheet);       // the contact sheet still lists both
  CHECK(slurp(d + "/report.json").find("--shots: drew dropped") != std::string::npos);
  CHECK(run({"pair", "--shots=asked"}) == 0);
  CHECK(asked_renders == 1);
  // A typo fails, naming the ids shot() dropped; the runner refuses a partial digest.
  CHECK(run({"pair", "--shots=askd"}) == 1);
  CHECK(slurp(d + "/report.json").find("--shots=asked,dropped") != std::string::npos);
  CHECK(run({"pair", "--shots=asked", "--write-digests=" + dir() + "/d.txt"}) == 2);
  CHECK(run({"--all", "--shots=asked"}) == 2);
}

TEST(cvars_reach_a_lab_that_reads_them_and_fail_one_that_does_not) {
  fs::remove_all(dir());
  CHECK(run({"pair", "--tag=lit"}) == 0);
  CHECK(run({"pair", "--cvars=dark"}) == 0);
  CHECK(slurp(dir() + "/pair/asked.png") != slurp(dir() + "/pair@lit/asked.png"));
  CHECK(slurp(dir() + "/pair/report.json").find("--cvars: dark") != std::string::npos);
  CHECK(run({"toy", "--cvars=dark"}) == 1);  // toy reads none: a pair would differ in nothing
  CHECK(slurp(dir() + "/toy/report.json").find("the lab reads none") != std::string::npos);
}

TEST(repeat_catches_hidden_state) {
  CHECK(run({"toy", "--repeat=3"}) == 0);
  CHECK(run({"drift", "--repeat=2"}) == 1);
}

TEST(shards_split_the_labs) {
  // The registry here is {drift, pair, toy}: slice 1/2 is drift and toy (drift fails its
  // repeat), 2/2 pair.
  CHECK(run({"--all", "--smoke", "--repeat=2", "--shard=1/2"}) == 1);
  CHECK(run({"--all", "--smoke", "--repeat=2", "--shard=2/2"}) == 0);
  CHECK(run({"--all", "--smoke", "--shard=3/2"}) == 2);
  CHECK(run({"--all", "--smoke", "--shard=2"}) == 2);
  CHECK(run({"toy", "--shard=1/2"}) == 2);  // a slice of one named lab means nothing
}

TEST(digest_baseline) {
  const std::string base = dir() + "/digests.txt";
  CHECK(run({"toy", "--smoke", "--write-digests=" + base}) == 0);
  CHECK(slurp(base).find("toy ") != std::string::npos);
  CHECK(run({"toy", "--smoke", "--check-digests=" + base}) == 0);
  // A different input is a different digest: the baseline catches it.
  CHECK(run({"toy", "--n=9", "--check-digests=" + base}) == 1);
  // A lab missing from the baseline fails too.
  CHECK(run({"drift", "--check-digests=" + base}) == 1);
  // Writing one lab's row keeps the others: drift's row joins toy's, sorted by id.
  CHECK(run({"drift", "--smoke", "--write-digests=" + base}) == 0);
  const std::string both = slurp(base);
  CHECK_MSG(both.find("drift ") != std::string::npos && both.find("toy ") > both.find("drift "),
            both);
  CHECK(run({"toy", "--smoke", "--check-digests=" + base}) == 0);
  // Rewriting every lab's row keeps the row of a lab this build does not register (a GPU lab
  // on a GPU-less build).
  std::ofstream(base, std::ios::app) << "gpu-only 0123456789abcdef\n";
  run({"--all", "--smoke", "--write-digests=" + base});
  const std::string every = slurp(base);
  CHECK_MSG(every.find("gpu-only 0123456789abcdef") != std::string::npos, every);
  CHECK(every.find("toy ") != std::string::npos);
}

TEST(digest_baseline_as_a_directory_of_one_file_a_lab) {
  // The repository's form: tests/digests/<lab>.txt, so two pull requests moving different labs
  // touch different files and never conflict.
  const std::string base = dir() + "/digests";
  fs::remove_all(base);
  CHECK(run({"toy", "--smoke", "--write-digests=" + base}) == 0);
  const std::string toy = slurp(base + "/toy.txt");
  CHECK_MSG(toy.size() == 17 && toy.back() == '\n', toy);  // 16 hex digits and a newline
  CHECK(!fs::exists(base + "/drift.txt"));
  CHECK(run({"toy", "--smoke", "--check-digests=" + base}) == 0);
  CHECK(run({"toy", "--n=9", "--check-digests=" + base}) == 1);
  CHECK(run({"drift", "--check-digests=" + base}) == 1);
  // Writing one lab leaves every other lab's file as it was, and a file of a lab this build does
  // not register (a GPU lab on a GPU-less build) stays.
  std::ofstream(base + "/gpu-only.txt", std::ios::binary) << "0123456789abcdef\n";
  CHECK(run({"drift", "--smoke", "--write-digests=" + base}) == 0);
  CHECK(slurp(base + "/toy.txt") == toy);
  CHECK(fs::exists(base + "/drift.txt"));
  run({"--all", "--smoke", "--write-digests=" + base});
  CHECK(slurp(base + "/gpu-only.txt") == "0123456789abcdef\n");
  CHECK(run({"toy", "--smoke", "--check-digests=" + base}) == 0);
}

TEST(a_measured_shot_keeps_its_pixels_out_of_the_digest) {
  const auto digest = [](Rgba colour, Det det) {
    Lab lab(kToy, {});
    lab.shot({"box", "A BOX", "A box.", "", {}}, create_picture(8, 8, colour), det);
    return lab.digest();
  };
  // Measured: the id is hashed, the pixels are not.
  CHECK(digest({0, 90, 0}, Det::measured) == digest({200, 0, 0}, Det::measured));
  // Deterministic: the pixels are hashed.
  CHECK(digest({0, 90, 0}, Det::deterministic) != digest({200, 0, 0}, Det::deterministic));
  CHECK(digest({0, 90, 0}, Det::measured) != digest({0, 90, 0}, Det::deterministic));
}
