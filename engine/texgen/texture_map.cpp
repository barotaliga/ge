// The texture map's reader and writer.
//
// ge:summary Reads and writes the texture map file a converter and an agent share
// ge:keywords texture-map texmap parse write pin comment
// ge:skill textures
// ge:test tests/texgen_texture_map_test.cpp
#include "texgen/texture_map.hpp"

namespace ge::texgen {
namespace {

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
    s.remove_suffix(1);
  return s;
}

// Where a comment starts: a '#' at the line's start or after a space or tab.
size_t comment_at(std::string_view line) {
  for (size_t i = 0; i < line.size(); ++i)
    if (line[i] == '#' && (i == 0 || line[i - 1] == ' ' || line[i - 1] == '\t')) return i;
  return std::string_view::npos;
}

std::string_view first_word(std::string_view s, std::string_view* rest) {
  s = trim(s);
  const size_t sp = s.find_first_of(" \t");
  *rest = sp == std::string_view::npos ? std::string_view{} : trim(s.substr(sp));
  return s.substr(0, sp);
}

bool check_ref(std::string_view ref, int line_no, std::vector<Diagnostic>& errors) {
  std::vector<Diagnostic> ref_errors;
  parse_ref(ref, ref_errors);
  for (Diagnostic& d : ref_errors) errors.push_back({line_no, d.message});
  return ref_errors.empty();
}

}  // namespace

namespace {

bool is_note(std::string_view k) {
  return k == "class" || k == "traits" || k == "judge" || k == "cluster";
}

bool is_texture_kind(std::string_view k) {
  return k == "texture" || k == "liquid" || k == "sky" || k == "surface";
}

}  // namespace

TextureMapLine* TextureMap::find(std::string_view texture) {
  for (TextureMapLine& l : lines)
    if (is_texture_kind(l.keyword) && l.texture == texture) return &l;
  return nullptr;
}

const TextureMapLine* TextureMap::find(std::string_view texture) const {
  for (const TextureMapLine& l : lines)
    if (is_texture_kind(l.keyword) && l.texture == texture) return &l;
  return nullptr;
}

std::string TextureMap::note(std::string_view keyword, std::string_view texture) const {
  for (const TextureMapLine& l : lines)
    if (l.keyword == keyword && l.texture == texture) return l.value;
  return "";
}

TextureMap parse_texture_map(std::string_view text, std::vector<Diagnostic>& errors) {
  TextureMap map;
  int line_no = 0;
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    const std::string_view line = trim(text.substr(at, end - at));
    at = end + 1;
    ++line_no;
    TextureMapLine l;
    l.raw = std::string(line);
    const size_t hash = comment_at(line);
    std::string_view rest;
    std::string_view keyword = first_word(line, &rest);
    if (!is_note(keyword)) keyword = first_word(trim(line.substr(0, hash)), &rest);
    if (is_note(keyword)) {
      std::string_view prose;
      const std::string_view name = first_word(rest, &prose);
      if (name.empty() || prose.empty()) {
        errors.push_back(
            {line_no, "write `" + std::string(keyword) + " <name> <text>`, e.g. " +
                          (keyword == "class"    ? "`class city2_1 bricks`"
                           : keyword == "traits" ? "`traits city2_1 running bond, 8 courses`"
                                                 : "`judge city2_1 pass courses and bond match`")});
        continue;
      }
      if (!map.note(keyword, name).empty()) {
        errors.push_back({line_no, "`" + std::string(name) + "` has two `" + std::string(keyword) +
                                       "` lines; keep one"});
        continue;
      }
      l.keyword = std::string(keyword);
      l.texture = std::string(name);
      l.value = std::string(prose);  // the whole line past the name, '#' and all: it is prose
      map.lines.push_back(std::move(l));
      continue;
    }
    if (!is_texture_kind(keyword)) {
      map.lines.push_back(std::move(l));  // a comment, a blank or a line for another reader
      continue;
    }
    std::string_view value;
    const std::string_view name = first_word(rest, &value);
    const bool needs_value = keyword != "sky";
    if (name.empty() || (needs_value && value.empty())) {
      errors.push_back({line_no, "write `" + std::string(keyword) + " <name> " +
                                     (keyword == "liquid" ? "<kind> [<reference>]"
                                      : keyword == "sky"  ? "[<reference>]"
                                                          : "<reference>") +
                                     "`, e.g. `texture city2_1 red_bricks seed=7`"});
      continue;
    }
    if (keyword == "texture" && !check_ref(value, line_no, errors)) continue;
    if (keyword == "sky" && !value.empty() && !check_ref(value, line_no, errors)) continue;
    if (keyword == "liquid") {
      std::string_view ref;
      first_word(value, &ref);
      if (!ref.empty() && !check_ref(ref, line_no, errors)) continue;
    }
    if (map.find(name) != nullptr) {
      errors.push_back({line_no, "`" + std::string(name) + "` is mapped twice; keep one line"});
      continue;
    }
    l.keyword = std::string(keyword);
    l.texture = std::string(name);
    l.value = std::string(value);
    if (hash != std::string_view::npos) {
      std::string_view note = trim(line.substr(hash + 1));
      std::string_view after;
      if (first_word(note, &after) == "pin") {
        l.pinned = true;
        note = after;
      }
      l.note = std::string(note);
    }
    map.lines.push_back(std::move(l));
  }
  return map;
}

std::string texture_map_text(const TextureMap& map) {
  std::string out;
  for (const TextureMapLine& l : map.lines) {
    if (is_note(l.keyword)) {
      out += l.keyword + " " + l.texture + " " + l.value + "\n";
      continue;
    }
    if (l.keyword.empty()) {
      out += l.raw + "\n";
      continue;
    }
    out += l.keyword + " " + l.texture + (l.value.empty() ? "" : " " + l.value);
    if (l.pinned || !l.note.empty()) {
      out += "  #";
      if (l.pinned) out += " pin";
      if (!l.note.empty()) out += " " + l.note;
    }
    out += "\n";
  }
  return out;
}

}  // namespace ge::texgen
