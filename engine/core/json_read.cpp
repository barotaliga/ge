// The flat JSON reader (json_read.hpp).
//
// ge:summary The flat JSON reader: objects, scalars and optionally arrays into dotted paths
// ge:keywords json read parse flat reader
// ge:skill agent-interface services
// ge:test tests/play_session_test.cpp tests/services_discord_test.cpp
#include "core/json_read.hpp"

#include <cstdlib>

namespace ge::core {

namespace {

class Reader {
 public:
  Reader(std::string_view s, bool arrays) : s_(s), arrays_(arrays) {}

  bool read(FlatJson& out, std::string& error) {
    skip();
    if (!object("", out)) {
      error = error_.empty() ? "expected a JSON object" : error_;
      return false;
    }
    skip();
    if (i_ != s_.size()) {
      error = "text after the object at column " + std::to_string(i_ + 1);
      return false;
    }
    return true;
  }

 private:
  void skip() {
    while (i_ < s_.size() &&
           (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\r' || s_[i_] == '\n')) {
      i_++;
    }
  }
  bool eat(char c) {
    skip();
    if (i_ < s_.size() && s_[i_] == c) {
      i_++;
      return true;
    }
    return false;
  }
  bool fail(const std::string& what) {
    if (error_.empty()) error_ = what + " at column " + std::to_string(i_ + 1);
    return false;
  }
  bool string(std::string& out) {
    if (!eat('"')) return fail("expected a string");
    while (i_ < s_.size() && s_[i_] != '"') {
      if (s_[i_] == '\\') {
        if (++i_ >= s_.size()) break;
        const char e = s_[i_];
        out += e == 'n' ? '\n' : e == 't' ? '\t' : e;
      } else {
        out += s_[i_];
      }
      i_++;
    }
    if (i_ >= s_.size()) return fail("an unterminated string");
    i_++;
    return true;
  }
  bool number(JsonValue& v) {
    const size_t from = i_;
    while (i_ < s_.size() && (s_[i_] == '-' || s_[i_] == '+' || s_[i_] == '.' || s_[i_] == 'e' ||
                              s_[i_] == 'E' || (s_[i_] >= '0' && s_[i_] <= '9'))) {
      i_++;
    }
    v.text = std::string(s_.substr(from, i_ - from));
    char* end = nullptr;
    v.number = v.text.empty() ? 0 : std::strtod(v.text.c_str(), &end);
    if (v.text.empty() || end != v.text.c_str() + v.text.size()) {
      return fail("a malformed number");
    }
    return true;
  }
  bool value(const std::string& path, FlatJson& out) {
    skip();
    if (i_ >= s_.size()) return fail("expected a value");
    const char c = s_[i_];
    if (c == '{') return object(path + ".", out);
    if (c == '[') {
      if (!arrays_) return fail("an array (this line takes none)");
      return array(path, out);
    }
    JsonValue v;
    if (c == '"') {
      v.kind = JsonValue::Kind::string;
      if (!string(v.text)) return false;
    } else if (s_.substr(i_, 4) == "true" || s_.substr(i_, 5) == "false") {
      v.kind = JsonValue::Kind::boolean;
      v.boolean = c == 't';
      i_ += v.boolean ? 4 : 5;
    } else if (s_.substr(i_, 4) == "null") {
      i_ += 4;
    } else {
      v.kind = JsonValue::Kind::number;
      if (!number(v)) return false;
    }
    out[path] = std::move(v);
    return true;
  }
  bool array(const std::string& path, FlatJson& out) {
    if (!eat('[')) return fail("expected '['");
    if (eat(']')) return true;
    int k = 0;
    do {
      if (!value(path + "." + std::to_string(k++), out)) return false;
    } while (eat(','));
    if (!eat(']')) return fail("expected ',' or ']'");
    return true;
  }
  bool object(const std::string& prefix, FlatJson& out) {
    if (!eat('{')) return fail("expected '{'");
    if (eat('}')) return true;
    do {
      skip();
      std::string key;
      if (!string(key)) return false;
      if (!eat(':')) return fail("expected ':'");
      if (!value(prefix + key, out)) return false;
    } while (eat(','));
    if (!eat('}')) return fail("expected ',' or '}'");
    return true;
  }

  std::string_view s_;
  bool arrays_;
  size_t i_ = 0;
  std::string error_;
};

}  // namespace

bool read_flat(std::string_view text, FlatJson& out, std::string& error, bool arrays) {
  out.clear();
  return Reader(text, arrays).read(out, error);
}

std::string json_text(const JsonValue& v) {
  switch (v.kind) {
    case JsonValue::Kind::number:
      return v.text;
    case JsonValue::Kind::boolean:
      return v.boolean ? "true" : "false";
    case JsonValue::Kind::string: {
      std::string out = "\"";
      for (const char c : v.text) {
        if (c == '"' || c == '\\') out += '\\';
        if (static_cast<unsigned char>(c) >= 0x20) out += c;
      }
      return out + "\"";
    }
    case JsonValue::Kind::null:
      break;
  }
  return "null";
}

}  // namespace ge::core
