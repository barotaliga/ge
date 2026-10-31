// The map's entity text (entity_text.hpp): the tokenizer, the blocks and the key rules.
//
// ge:summary The entity text's tokenizer, blocks, and the key rules by field type
// ge:keywords parse_entity_text apply_entity_block block_value entity-text key-rules
// ge:skill progs
// ge:test tests/progs_entity_text_test.cpp
#include "progs/entity_text.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ge::progs {

namespace {

struct Lexer {
  const char* p;
  int line = 1;

  // The next token into `out`; false at the end. `quoted` says whether it was in quotes.
  bool next(std::string& out, bool& quoted) {
    for (;;) {
      while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        if (*p == '\n') line++;
        p++;
      }
      if (p[0] == '/' && p[1] == '/') {
        while (*p != 0 && *p != '\n') p++;
        continue;
      }
      break;
    }
    if (*p == 0) return false;
    out.clear();
    quoted = false;
    if (*p == '{' || *p == '}') {
      out.push_back(*p++);
      return true;
    }
    if (*p == '"') {
      quoted = true;
      p++;
      while (*p != 0 && *p != '"') {
        if (*p == '\n') line++;
        out.push_back(*p++);
      }
      if (*p == '"') p++;
      return true;
    }
    while (*p != 0 && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') out.push_back(*p++);
    return true;
  }
};

std::string unescape(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      out.push_back(s[i + 1] == 'n' ? '\n' : '\\');
      i++;
    } else {
      out.push_back(s[i]);
    }
  }
  return out;
}

float decimal(const char*& p) {
  char* end = nullptr;
  const double d = std::strtod(p, &end);
  if (end == p) return 0;
  p = end;
  return static_cast<float>(d);
}

// A value's text into the words at `w` by the definition's type (the rules in the header).
bool set_value(Vm& vm, const Def& def, uint32_t* w, const std::string& key,
               const std::string& value, int line, std::vector<std::string>& messages,
               std::string& error) {
  const Progs& p = *vm.progs;
  const char* q = value.c_str();
  switch (def.type & kTypeMask) {
    case kTypeString: {
      const int32_t s = engine_string(vm, unescape(value).c_str());
      if (vm.fault.set) {
        error = std::string("entity text: ") + vm.fault.message;
        return false;
      }
      *w = static_cast<uint32_t>(s);
      break;
    }
    case kTypeFloat:
      *w = as_word(decimal(q));
      break;
    case kTypeVector:
      for (int i = 0; i < 3; i++) w[i] = as_word(decimal(q));
      break;
    case kTypeEntity:
      *w = static_cast<uint32_t>(std::atoi(q) * vm.entity_bytes);
      break;
    case kTypeField: {
      const Def* f = find_field(p, q);
      if (f == nullptr) {
        error = "entity text, block at line " + std::to_string(line) + ": key '";
        error += key;
        error += "' names no field '";
        error += value;
        error += "'; fix the map or the mod";
        return false;
      }
      *w = f->offset;
      break;
    }
    case kTypeFunction: {
      const int fn = find_function(p, q);
      if (fn < 0) {
        error = "entity text, block at line " + std::to_string(line) + ": key '";
        error += key;
        error += "' names no function '";
        error += value;
        error += "'; fix the map or the mod";
        return false;
      }
      *w = static_cast<uint32_t>(fn);
      break;
    }
    default:
      messages.push_back(key + " has a type the entity text cannot set");
      break;
  }
  return true;
}

}  // namespace

bool parse_entity_text(const char* text, std::vector<EntityBlock>& blocks, std::string& error) {
  blocks.clear();
  Lexer lx{text};
  std::string tok, value;
  bool quoted = false;
  while (lx.next(tok, quoted)) {
    if (quoted || tok != "{") {
      error = "entity text, line " + std::to_string(lx.line) + ": expected '{' to start block " +
              std::to_string(blocks.size()) + ", found '" + tok + "'; fix the map's entity text";
      return false;
    }
    EntityBlock b;
    b.line = lx.line;
    for (;;) {
      if (!lx.next(tok, quoted)) {
        error = "entity text: block " + std::to_string(blocks.size()) + " (line " +
                std::to_string(b.line) + ") has no closing '}'; fix the map's entity text";
        return false;
      }
      if (!quoted && tok == "}") break;
      if (!lx.next(value, quoted) || (!quoted && (value == "}" || value == "{"))) {
        error = "entity text, line " + std::to_string(lx.line) + ": key '" + tok + "' of block " +
                std::to_string(blocks.size()) + " has no value; fix the map's entity text";
        return false;
      }
      b.pairs.emplace_back(tok, value);
    }
    blocks.push_back(std::move(b));
  }
  return true;
}

const char* block_value(const EntityBlock& b, const char* key) {
  for (const auto& [k, v] : b.pairs)
    if (k == key) return v.c_str();
  return nullptr;
}

bool apply_entity_block(Vm& vm, int e, const EntityBlock& b, EntityExtras& extras,
                        std::vector<std::string>& messages, std::string& error) {
  const Progs& p = *vm.progs;
  for (const auto& [key0, value0] : b.pairs) {
    std::string key = key0, value = value0;
    if (!key.empty() && key[0] == '_') continue;
    if (key == "angle") {
      key = "angles";
      value = std::string("0 ").append(value).append(" 0");
    } else if (key == "light") {
      key = "light_lev";
    }
    if (key == "alpha") {
      const char* q = value.c_str();
      float a = decimal(q);
      a = a < 0 ? 0 : a > 1 ? 1 : a;
      extras.has_alpha = true;
      extras.alpha = a;
      continue;
    }
    if (key == "colormod") {
      const char* q = value.c_str();
      float c[3];
      for (float& x : c) x = decimal(q);
      if (c[0] > 0 && c[1] > 0 && c[2] > 0) {
        extras.has_colormod = true;
        for (int i = 0; i < 3; i++) extras.colormod[i] = c[i];
      }
      continue;
    }
    const Def* d = find_field(p, key.c_str());
    if (d == nullptr) {
      messages.push_back(key + " is not a field");
      continue;
    }
    if (!set_value(vm, *d, entity_field(vm, e, d->offset), key, value, b.line, messages, error))
      return false;
  }
  return true;
}

bool apply_global_block(Vm& vm, const EntityBlock& b, std::vector<std::string>& messages,
                        std::string& error) {
  for (const auto& [key, value] : b.pairs) {
    const Def* d = find_global(*vm.progs, key.c_str());
    if (d == nullptr) {
      messages.push_back(key + " is not a global");
      continue;
    }
    if (!set_value(vm, *d, &vm.globals[d->offset], key, value, b.line, messages, error))
      return false;
  }
  return true;
}

bool value_is_zero(uint16_t type, const uint32_t* w) {
  for (int i = 0; i < type_words(type); i++)
    if (w[i] != 0 && !((type & kTypeMask) != kTypeString && w[i] == 0x80000000u)) return false;
  return true;
}

namespace {

// The shortest decimal that reads back to `f`: in plain digits (`100`, `0.25`) where that takes at
// most 9 decimals and the number is under 10^9, else in the exponent form.
std::string float_text(float f) {
  char buf[64];
  const double d = static_cast<double>(f);
  if (f == f && d < 1e9 && d > -1e9) {
    for (int decimals = 0; decimals <= 9; decimals++) {
      std::snprintf(buf, sizeof buf, "%.*f", decimals, d);
      if (std::strtof(buf, nullptr) == f) return buf;
    }
  }
  for (int digits = 1; digits <= 9; digits++) {
    std::snprintf(buf, sizeof buf, "%.*g", digits, d);
    if (std::strtof(buf, nullptr) == f || f != f) break;
  }
  return buf;
}

}  // namespace

std::string value_text(const Vm& vm, uint16_t type, const uint32_t* w) {
  const Progs& p = *vm.progs;
  switch (type & kTypeMask) {
    case kTypeString: {
      const char* t = vm_string(vm, static_cast<int32_t>(w[0]));
      std::string out;
      for (const char* c = t == nullptr ? "" : t; *c != 0; c++) {
        if (*c == '\n')
          out += "\\n";
        else if (*c != '"')
          out.push_back(*c);
      }
      return out;
    }
    case kTypeFloat:
      return float_text(as_float(w[0]));
    case kTypeVector:
      return float_text(as_float(w[0])) + " " + float_text(as_float(w[1])) + " " +
             float_text(as_float(w[2]));
    case kTypeEntity:
      return std::to_string(entity_number(vm, static_cast<int32_t>(w[0])));
    case kTypeField:
      for (const Def& d : p.fielddefs)
        if (d.offset == w[0] && (d.type & kTypeMask) != kTypeVoid) return progs_string(p, d.name);
      return "";
    case kTypeFunction: {
      const size_t fn = w[0];
      return fn < p.functions.size() ? progs_string(p, p.functions[fn].name) : "";
    }
    default:
      return "";
  }
}

}  // namespace ge::progs
