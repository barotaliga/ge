// engine/save/part.hpp: a character's chunks cut out of a run's save, byte for byte, under a meta
// of kind character; a file that is not a run, another game's, or one missing a chunk refused.
//
// ge:summary Test: save::extract cuts a character's chunks out of a run's save
// ge:keywords test save part character extract chunk kind
// ge:skill save testing
#include "save/part.hpp"

#include <array>
#include <string>
#include <vector>

#include "check.hpp"
#include "save/file.hpp"

namespace {

using namespace ge::save;

constexpr uint32_t kWorld = tag("WRLD"), kHero = tag("HERO"), kBag = tag("BAGS");

std::vector<uint8_t> run_save(Kind kind = Kind::run) {
  Meta m;
  m.game = "demo";
  m.kind = kind;
  m.title = "the whole run";
  m.setup = "seed 38";
  std::vector<uint8_t> out;
  FileWriter w(out, m);
  w.put(kWorld, 0, 1, std::vector<int32_t>{1, 2, 3});
  w.put(kHero, 0, 3, std::string("a hero"));
  w.put(kBag, 0, 1, std::vector<double>{0.5});
  w.put(kBag, 1, 2, std::vector<double>{1.5, 2.5});
  CHECK(w.finish());
  return out;
}

}  // namespace

TEST(a_character_is_its_chunks_cut_out_byte_for_byte) {
  const std::vector<uint8_t> run = run_save();
  std::vector<uint8_t> part;
  std::string problem;
  const std::array<uint32_t, 2> tags{kHero, kBag};
  CHECK_MSG(extract(run, "demo", tags, "the hero", part, problem), problem);
  FileReader r;
  CHECK(r.open(part, "part", "demo"));
  CHECK(r.header().kind == Kind::character && r.meta().kind == Kind::character);
  CHECK(r.meta().title == "the hero" && r.meta().setup == "seed 38");
  CHECK(r.find(kWorld) == nullptr);
  std::string hero;
  CHECK(r.get(kHero, 0, 3, hero) && hero == "a hero");
  CHECK(r.find(kHero)->version == 3);
  std::vector<double> bag;
  CHECK(r.get(kBag, 1, 2, bag) && bag.size() == 2 && bag[1] == 2.5);
  CHECK(r.all(kBag).size() == 2);
}

TEST(a_file_that_is_not_a_run_or_lacks_a_chunk_is_refused) {
  std::vector<uint8_t> part;
  std::string problem;
  const std::array<uint32_t, 1> hero{kHero};
  CHECK(!extract(run_save(Kind::character), "demo", hero, "", part, problem));
  CHECK(problem.find("not a whole run") != std::string::npos);
  CHECK(!extract(run_save(), "other", hero, "", part, problem));
  const std::array<uint32_t, 1> missing{tag("NONE")};
  CHECK(!extract(run_save(), "demo", missing, "", part, problem));
  CHECK(problem.find("NONE") != std::string::npos);
}
