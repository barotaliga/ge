// A program's words by key (localized.hpp states the file's format and the print rule).
//
// ge:summary The localized-text file read and the `$key` print rule applied
// ge:keywords localization read_localized localize_args utf8-to-classic placeholder
// ge:skill progs
// ge:test tests/progs_localized_test.cpp
#include "progs/localized.hpp"

#include <cstdint>
#include <cstring>

#include "progs/stdlib.hpp"
#include "progs/vm.hpp"

namespace ge::progs {

namespace {

constexpr char kBox = 11;  // the classic font's box: a character it has no glyph for

// Latin-1's letters U+00C0..U+00FF as their plain letter ('?': none; two letters for a few).
constexpr const char* kLatin1[64] = {
    "A", "A", "A", "A", "A", "A", "AE", "C", "E", "E", "E", "E", "I", "I", "I",  "I",
    "D", "N", "O", "O", "O", "O", "O",  "x", "O", "U", "U", "U", "U", "Y", "TH", "ss",
    "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i",  "i",
    "d", "n", "o", "o", "o", "o", "o",  "/", "o", "u", "u", "u", "u", "y", "th", "y"};

// One code point in the classic set, appended to `out`.
void put_code_point(uint32_t cp, std::string& out) {
  if (cp < 0x80) {
    out.push_back(static_cast<char>(cp));
    return;
  }
  if (cp >= 0xC0 && cp <= 0xFF) {
    out += kLatin1[cp - 0xC0];
    return;
  }
  switch (cp) {
    case 0xA0:  // no-break space
      out.push_back(' ');
      return;
    case 0x2018:  // the single quotes
    case 0x2019:
      out.push_back('\'');
      return;
    case 0x201C:  // the double quotes
    case 0x201D:
      out.push_back('"');
      return;
    case 0x2010:  // the hyphens and dashes
    case 0x2011:
    case 0x2012:
    case 0x2013:
    case 0x2014:
      out.push_back('-');
      return;
    case 0x2026:  // the ellipsis
      out += "...";
      return;
    default:
      out.push_back(kBox);
  }
}

// UTF-8 text in the classic set (a broken sequence is one box per byte that starts it).
std::string to_classic(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  size_t i = 0;
  while (i < s.size()) {
    const auto b = static_cast<uint8_t>(s[i]);
    int more = 0;
    uint32_t cp = 0;
    if (b < 0x80) {
      cp = b;
    } else if ((b & 0xE0) == 0xC0) {
      more = 1, cp = b & 0x1Fu;
    } else if ((b & 0xF0) == 0xE0) {
      more = 2, cp = b & 0x0Fu;
    } else if ((b & 0xF8) == 0xF0) {
      more = 3, cp = b & 0x07u;
    } else {
      out.push_back(kBox);
      i++;
      continue;
    }
    bool ok = i + static_cast<size_t>(more) < s.size();
    for (int k = 1; ok && k <= more; k++) {
      const auto c = static_cast<uint8_t>(s[i + static_cast<size_t>(k)]);
      if ((c & 0xC0) != 0x80) ok = false;
      cp = (cp << 6) | (c & 0x3Fu);
    }
    if (!ok) {
      out.push_back(kBox);
      i++;
      continue;
    }
    put_code_point(cp, out);
    i += static_cast<size_t>(more) + 1;
  }
  return out;
}

bool space(char c) { return c == ' ' || c == '\t'; }

// The value after the `=` (its white space skipped): quoted or bare, escapes applied.
std::string read_value(std::string_view v) {
  size_t i = 0;
  while (i < v.size() && space(v[i])) i++;
  std::string out;
  const bool quoted = i < v.size() && v[i] == '"';
  if (quoted) i++;
  size_t end = v.size();
  if (!quoted)
    while (end > i && space(v[end - 1])) end--;
  for (; i < end; i++) {
    const char c = v[i];
    if (quoted && c == '"') break;
    if (c != '\\' || i + 1 >= end) {
      out.push_back(c);
      continue;
    }
    const char e = v[++i];
    switch (e) {
      case 'n':
        out.push_back('\n');
        break;
      case 't':
        out.push_back('\t');
        break;
      case 'v':
        out.push_back('\v');
        break;
      case 'b':
        out.push_back('\b');
        break;
      case 'f':
        out.push_back('\f');
        break;
      default:
        out.push_back(e);  // \" \' \\ and any other: the character itself
    }
  }
  return out;
}

// The placeholder at `s[i]` (a `{`): its argument index and length; false when it is none.
bool placeholder(std::string_view s, size_t i, int& index, size_t& length) {
  size_t j = i + 1;
  int n = 0;
  bool digits = false;
  while (j < s.size() && s[j] >= '0' && s[j] <= '9') {
    if (n < 1000) n = n * 10 + (s[j] - '0');
    digits = true;
    j++;
  }
  if (j >= s.size() || s[j] != '}') return false;
  index = digits ? n : 0;
  length = j + 1 - i;
  return true;
}

}  // namespace

int read_localized(std::string_view file, LocalizedText& out) {
  if (file.starts_with("\xEF\xBB\xBF")) file.remove_prefix(3);
  int added = 0;
  while (!file.empty()) {
    const size_t nl = file.find('\n');
    std::string_view line = file.substr(0, nl);
    file = nl == std::string_view::npos ? std::string_view{} : file.substr(nl + 1);
    size_t i = 0;
    while (i < line.size() && space(line[i])) i++;
    line.remove_prefix(i);
    if (line.empty() || line[0] == '/') continue;
    const size_t eq = line.find('=');
    if (eq == std::string_view::npos) continue;
    std::string_view key = line.substr(0, eq);
    while (!key.empty() && (space(key.back()) || key.back() == '\r')) key.remove_suffix(1);
    if (key.empty()) continue;
    const auto [it, fresh] =
        out.words.try_emplace(std::string(key), to_classic(read_value(line.substr(eq + 1))));
    if (fresh) added++;
  }
  return added;
}

const std::string* localized(const LocalizedText& table, std::string_view s) {
  if (s.size() < 2 || s[0] != '$') return nullptr;
  const auto it = table.words.find(std::string(s.substr(1)));
  return it == table.words.end() ? nullptr : &it->second;
}

std::string_view resolve_key(const LocalizedText* table, std::string_view s) {
  if (table == nullptr) return s;
  const std::string* w = localized(*table, s);
  return w != nullptr ? std::string_view(*w) : s;
}

void localize_args(const LocalizedText* table, std::span<const std::string_view> args, char* out,
                   int cap) {
  int used = 0;
  out[0] = 0;
  const auto put = [&](std::string_view s) {
    const int room = cap - 1 - used;
    const int take = static_cast<int>(s.size()) < room ? static_cast<int>(s.size()) : room;
    if (take <= 0) return;
    std::memcpy(out + used, s.data(), static_cast<size_t>(take));
    used += take;
    out[used] = 0;
  };
  if (args.empty()) return;
  const std::string_view text = resolve_key(table, args[0]);
  bool has_placeholder = false;
  if (table != nullptr) {
    for (size_t i = 0; i < text.size() && !has_placeholder; i++) {
      int index = 0;
      size_t length = 0;
      has_placeholder = text[i] == '{' && placeholder(text, i, index, length);
    }
  }
  if (!has_placeholder) {
    put(text);
    for (size_t k = 1; k < args.size(); k++) put(resolve_key(table, args[k]));
    return;
  }
  size_t i = 0;
  while (i < text.size()) {
    int index = 0;
    size_t length = 0;
    if (text[i] == '{' && placeholder(text, i, index, length)) {
      const size_t a = static_cast<size_t>(index) + 1;
      if (a < args.size()) put(resolve_key(table, args[a]));
      i += length;
      continue;
    }
    const size_t next = text.find('{', i + 1);
    const size_t end = next == std::string_view::npos ? text.size() : next;
    put(text.substr(i, end - i));
    i = end;
  }
}

void localize_join_args(Vm& vm, const LocalizedText* table, int first, char* out, int cap) {
  if (table == nullptr) {
    join_args(vm, first, out, cap);
    return;
  }
  std::string_view args[kMaxParms];
  int n = 0;
  for (int i = first; i < vm.argc && i < kMaxParms; i++) {
    args[n++] = arg_string(vm, i);
    if (vm.fault.set) {
      out[0] = 0;
      return;
    }
  }
  localize_args(table, std::span<const std::string_view>(args, static_cast<size_t>(n)), out, cap);
}

}  // namespace ge::progs
