// The tree's parser (a recursive descent over RFC 8259, \u escapes to UTF-8 with surrogate pairs)
// and its compact writer.
//
// ge:summary The JSON tree's parser and writer: ordered members, numbers in their own spelling
// ge:keywords cli json parse dump escape utf-8 surrogate recursive-descent
// ge:skill agent-interface
// ge:test tests/cli_model_test.cpp
#include "cli/json_tree.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>

#include "core/num_text.hpp"

namespace ge::cli {

namespace {

struct Parser {
  std::string_view s;
  size_t i = 0;
  std::string error;

  bool fail(const std::string& what) {
    if (error.empty()) error = what + " at byte " + std::to_string(i);
    return false;
  }
  void space() {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) i++;
  }
  bool word(std::string_view w) {
    if (s.substr(i, w.size()) != w) return fail("expected " + std::string(w));
    i += w.size();
    return true;
  }
  static void utf8(std::string& out, uint32_t c) {
    if (c < 0x80) {
      out += static_cast<char>(c);
    } else if (c < 0x800) {
      out += static_cast<char>(0xc0 | (c >> 6));
      out += static_cast<char>(0x80 | (c & 0x3f));
    } else if (c < 0x10000) {
      out += static_cast<char>(0xe0 | (c >> 12));
      out += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
      out += static_cast<char>(0x80 | (c & 0x3f));
    } else {
      out += static_cast<char>(0xf0 | (c >> 18));
      out += static_cast<char>(0x80 | ((c >> 12) & 0x3f));
      out += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
      out += static_cast<char>(0x80 | (c & 0x3f));
    }
  }
  bool hex4(uint32_t& out) {
    if (i + 4 > s.size()) return fail("a short \\u escape");
    out = 0;
    for (int k = 0; k < 4; k++) {
      const char c = s[i++];
      out <<= 4;
      if (c >= '0' && c <= '9') {
        out |= static_cast<uint32_t>(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        out |= static_cast<uint32_t>(c - 'a' + 10);
      } else if (c >= 'A' && c <= 'F') {
        out |= static_cast<uint32_t>(c - 'A' + 10);
      } else {
        return fail("a bad \\u escape");
      }
    }
    return true;
  }
  bool string(std::string& out) {
    if (i >= s.size() || s[i] != '"') return fail("expected a string");
    i++;
    while (i < s.size() && s[i] != '"') {
      const char c = s[i++];
      if (c != '\\') {
        out += c;
        continue;
      }
      if (i >= s.size()) return fail("an unfinished escape");
      const char e = s[i++];
      switch (e) {
        case '"':
          out += '"';
          break;
        case '\\':
          out += '\\';
          break;
        case '/':
          out += '/';
          break;
        case 'b':
          out += '\b';
          break;
        case 'f':
          out += '\f';
          break;
        case 'n':
          out += '\n';
          break;
        case 'r':
          out += '\r';
          break;
        case 't':
          out += '\t';
          break;
        case 'u': {
          uint32_t c1 = 0;
          if (!hex4(c1)) return false;
          if (c1 >= 0xd800 && c1 < 0xdc00 && s.substr(i, 2) == "\\u") {
            i += 2;
            uint32_t c2 = 0;
            if (!hex4(c2)) return false;
            c1 = 0x10000 + ((c1 - 0xd800) << 10) + (c2 - 0xdc00);
          }
          utf8(out, c1);
          break;
        }
        default:
          return fail("an unknown escape");
      }
    }
    if (i >= s.size()) return fail("an unfinished string");
    i++;
    return true;
  }
  bool value(JsonNode& v, int depth) {
    if (depth > 256) return fail("nesting too deep");
    space();
    if (i >= s.size()) return fail("expected a value");
    const char c = s[i];
    if (c == '{') {
      v.kind = JsonNode::Kind::object;
      i++;
      space();
      if (i < s.size() && s[i] == '}') {
        i++;
        return true;
      }
      while (true) {
        space();
        std::string key;
        if (!string(key)) return false;
        space();
        if (i >= s.size() || s[i] != ':') return fail("expected ':'");
        i++;
        JsonNode m;
        if (!value(m, depth + 1)) return false;
        v.members.push_back({std::move(key), std::move(m)});
        space();
        if (i < s.size() && s[i] == ',') {
          i++;
          continue;
        }
        if (i < s.size() && s[i] == '}') {
          i++;
          return true;
        }
        return fail("expected ',' or '}'");
      }
    }
    if (c == '[') {
      v.kind = JsonNode::Kind::array;
      i++;
      space();
      if (i < s.size() && s[i] == ']') {
        i++;
        return true;
      }
      while (true) {
        JsonNode m;
        if (!value(m, depth + 1)) return false;
        v.items.push_back(std::move(m));
        space();
        if (i < s.size() && s[i] == ',') {
          i++;
          continue;
        }
        if (i < s.size() && s[i] == ']') {
          i++;
          return true;
        }
        return fail("expected ',' or ']'");
      }
    }
    if (c == '"') {
      v.kind = JsonNode::Kind::string;
      return string(v.text);
    }
    if (c == 't' || c == 'f') {
      v.kind = JsonNode::Kind::boolean;
      v.boolean = c == 't';
      return word(v.boolean ? "true" : "false");
    }
    if (c == 'n') {
      v.kind = JsonNode::Kind::null;
      return word("null");
    }
    const size_t start = i;
    while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '-' ||
                            s[i] == '+' || s[i] == '.' || s[i] == 'e' || s[i] == 'E')) {
      i++;
    }
    if (i == start) return fail("unexpected character");
    v.kind = JsonNode::Kind::number;
    v.text = std::string(s.substr(start, i - start));
    return true;
  }
};

void quote(std::string& out, const std::string& s) {
  out += '"';
  for (const char ch : s) {
    const auto c = static_cast<unsigned char>(ch);
    switch (ch) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      default:
        if (c < 0x20) {
          static constexpr char kHex[] = "0123456789abcdef";
          out += "\\u00";
          out += kHex[c >> 4];
          out += kHex[c & 15];
        } else {
          out += ch;
        }
    }
  }
  out += '"';
}

void dump(std::string& out, const JsonNode& v) {
  switch (v.kind) {
    case JsonNode::Kind::null:
      out += "null";
      break;
    case JsonNode::Kind::boolean:
      out += v.boolean ? "true" : "false";
      break;
    case JsonNode::Kind::number:
      out += v.text;
      break;
    case JsonNode::Kind::string:
      quote(out, v.text);
      break;
    case JsonNode::Kind::array:
      out += '[';
      for (size_t k = 0; k < v.items.size(); k++) {
        if (k) out += ',';
        dump(out, v.items[k]);
      }
      out += ']';
      break;
    case JsonNode::Kind::object:
      out += '{';
      for (size_t k = 0; k < v.members.size(); k++) {
        if (k) out += ',';
        quote(out, v.members[k].key);
        out += ':';
        dump(out, v.members[k].value);
      }
      out += '}';
      break;
  }
}

}  // namespace

JsonNode* JsonNode::find(std::string_view key) {
  for (auto& [k, v] : members) {
    if (k == key) return &v;
  }
  return nullptr;
}

const JsonNode* JsonNode::find(std::string_view key) const {
  for (const auto& [k, v] : members) {
    if (k == key) return &v;
  }
  return nullptr;
}

JsonNode& JsonNode::at(std::string_view key) {
  if (JsonNode* v = find(key)) return *v;
  kind = Kind::object;
  members.push_back({std::string(key), JsonNode{}});
  return members.back().value;
}

bool JsonNode::erase(std::string_view key) {
  for (auto it = members.begin(); it != members.end(); ++it) {
    if (it->key == key) {
      members.erase(it);
      return true;
    }
  }
  return false;
}

double JsonNode::number() const {
  return kind == Kind::number ? std::strtod(text.c_str(), nullptr) : 0;
}

JsonNode JsonNode::of_number(double v) {
  JsonNode n;
  n.kind = Kind::number;
  if (v == std::floor(v) && std::abs(v) < 9e15) {
    n.text = std::to_string(static_cast<long long>(v));
  } else {
    n.text = core::num::to_string(v);
  }
  return n;
}

JsonNode JsonNode::of_string(std::string s) {
  JsonNode n;
  n.kind = Kind::string;
  n.text = std::move(s);
  return n;
}

bool parse_json(std::string_view text, JsonNode& out, std::string& error) {
  Parser p;
  p.s = text;
  out = JsonNode{};
  if (!p.value(out, 0)) {
    error = p.error;
    return false;
  }
  p.space();
  if (p.i != text.size()) {
    p.fail("text after the value; next: check the file holds one JSON value and nothing after it");
    error = p.error;
    return false;
  }
  return true;
}

std::string dump_json(const JsonNode& v) {
  std::string out;
  dump(out, v);
  return out;
}

}  // namespace ge::cli
