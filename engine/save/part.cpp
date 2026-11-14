// The character's chunks copied out of a run's save: decoded, then written under the new meta.
//
// ge:summary save::extract: a run's chunks by tag copied into a character file
// ge:keywords character extract chunk copy
// ge:skill save
// ge:test tests/save_part_test.cpp
#include "save/part.hpp"

#include "save/file.hpp"

namespace ge::save {

bool extract(std::span<const uint8_t> run, std::string_view game, std::span<const uint32_t> tags,
             std::string_view title, std::vector<uint8_t>& out, std::string& problem) {
  FileReader r;
  if (!r.open(run, "the run's save", game)) {
    problem = r.problem();
    return false;
  }
  if (r.header().kind != Kind::run) {
    problem =
        "the file is not a whole run's save, so no character is taken from it; extract "
        "from a save the game wrote with Run::save";
    return false;
  }
  for (const uint32_t t : tags)
    if (r.all(t).empty()) {
      problem = "the run's save has no " + tag_name(t) +
                " chunk, which the character is made of; save the run with a build of the "
                "game that writes it";
      return false;
    }
  Meta meta = r.meta();
  meta.kind = Kind::character;
  if (!title.empty()) meta.title = std::string(title);
  out.clear();
  FileWriter w(out, meta);
  std::vector<uint8_t> bytes;
  for (const uint32_t t : tags)
    for (const ChunkInfo* c : r.all(t)) {
      Reader in = r.reader(*c);
      bytes.resize(in.left());
      in.raw(bytes.data(), bytes.size());
      if (!in.ok()) {
        problem = "chunk " + tag_name(t) + "#" + std::to_string(c->instance) +
                  " does not read: " + in.problem();
        return false;
      }
      w.chunk(t, c->instance, c->version).raw(bytes.data(), bytes.size());
    }
  return w.finish(&problem);
}

}  // namespace ge::save
