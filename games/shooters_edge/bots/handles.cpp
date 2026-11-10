// The names bots play under (handles.hpp).
//
// ge:summary The names bots play under among people: made-up handles drawn off a seed
// ge:keywords bot_handles handles names
// ge:skill bots
// ge:test tests/shooters_edge_footwork_test.cpp
#include "shooters_edge/bots/handles.hpp"

#include <array>

#include "core/prng.hpp"

namespace shooters_edge::bots {

namespace {

// Made-up handles, as people pick them: words, short names, a few with a digit or a case of their
// own.
constexpr std::array<const char*, 96> kHandles = {
    "mosk",   "Varg",   "tilde",  "brakk",  "Juno",  "skarv",  "pelle",  "hexa",   "Dregg",
    "nimbus", "rook",   "Saga",   "fennel", "tarn",  "Oskr",   "grit",   "Mallo",  "vexa",
    "kipp",   "Lund",   "doze",   "Brisk",  "milo",  "Zeph",   "harr",   "Tova",   "slag",
    "Krull",  "pixel",  "wraith", "Bodo",   "ember", "quill",  "Raske",  "nox",    "Fjell",
    "tamp",   "Gorm",   "lurk",   "Ivra",   "soot",  "Hakon",  "mire",   "Plink",  "otso",
    "Vale",   "crux",   "Dunn",   "frisk",  "Yarro", "pebble", "Stig",   "wisp",   "Kalla",
    "brine",  "Tusk",   "fable",  "Rune",   "gloam", "Hesk",   "jolt",   "Arvo",   "sprocket",
    "Mjau",   "teal",   "Boris",  "vapor",  "Nils",  "drift",  "Elko",   "smirk",  "Pontus",
    "gl1tch", "Ferro",  "husk",   "Lomm",   "strix", "Ottar",  "cinder", "Vemund", "k0rp",
    "Ylva",   "basalt", "Trond",  "nettle", "Ask",   "rumble", "Siv",    "ozzy",   "Kettil",
    "lynx",   "Bror",   "sk1ff",  "Ragna",  "murk",  "Edda",
};

}  // namespace

std::vector<std::string> bot_handles(uint32_t seed, int count) {
  std::vector<std::string> out;
  ge::core::Rng rng = ge::core::make_rng(seed * 2654435761u + 40503u);
  std::array<bool, kHandles.size()> used{};
  while (static_cast<int>(out.size()) < count) {
    if (out.size() >= kHandles.size()) {
      out.push_back("player" +
                    std::to_string(out.size() + 1));  // past the list: plain, still none alike
      continue;
    }
    auto k = static_cast<size_t>(ge::core::next(rng) * static_cast<double>(kHandles.size()));
    if (k >= kHandles.size()) k = kHandles.size() - 1;
    while (used[k]) k = (k + 1) % kHandles.size();
    used[k] = true;
    out.emplace_back(kHandles[k]);
  }
  return out;
}

}  // namespace shooters_edge::bots
