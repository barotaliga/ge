// QC's text rules (text.hpp): the number formats, the number reader and the word splitter.
//
// ge:summary QC's text rules' definitions: number formats through core/num_text, stof, the words
// ge:keywords format_ftos format_vtos parse_stof tokenize_words to_fixed
// ge:skill progs
// ge:test tests/progs_text_test.cpp
#include "progs/text.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "core/num_text.hpp"

namespace ge::progs {

namespace {

// One decimal, right-aligned in five characters.
void one_decimal(float v, char* out, size_t cap) {
  std::string s;
  if (v != v)
    s = "nan";
  else if (std::isinf(v))
    s = v > 0 ? "inf" : "-inf";
  else
    s = core::num::to_fixed(static_cast<double>(v), 1);
  std::snprintf(out, cap, "%5s", s.c_str());
}

}  // namespace

void format_ftos(float v, char* out) {
  if (v == v && !std::isinf(v) && v == std::trunc(v)) {
    // Whole: the integer, through double (exact for every float), with -0 as 0.
    const double d = static_cast<double>(v);
    std::snprintf(out, kFtosBytes, "%s", core::num::to_fixed(d == 0 ? 0.0 : d, 0).c_str());
    return;
  }
  one_decimal(v, out, kFtosBytes);
}

void format_vtos(float x, float y, float z, char* out) {
  char a[kFtosBytes], b[kFtosBytes], c[kFtosBytes];
  one_decimal(x, a, sizeof a);
  one_decimal(y, b, sizeof b);
  one_decimal(z, c, sizeof c);
  std::snprintf(out, kVtosBytes, "'%s %s %s'", a, b, c);
}

float parse_stof(const char* s) {
  while (*s != 0 && static_cast<unsigned char>(*s) <= ' ') s++;
  double sign = 1;
  if (*s == '-') {
    sign = -1;
    s++;
  } else if (*s == '+') {
    s++;
  }
  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    s += 2;
    double v = 0;
    for (;; s++) {
      int d = -1;
      if (*s >= '0' && *s <= '9') d = *s - '0';
      if (*s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
      if (*s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
      if (d < 0) break;
      v = v * 16 + d;
    }
    return static_cast<float>(sign * v);
  }
  if (*s == '\'') return static_cast<float>(sign * static_cast<unsigned char>(s[1]));
  double v = 0;
  int fraction_digits = -1;  // -1 until the point
  for (;; s++) {
    if (*s == '.' && fraction_digits < 0) {
      fraction_digits = 0;
      continue;
    }
    if (*s < '0' || *s > '9') break;
    v = v * 10 + (*s - '0');
    if (fraction_digits >= 0) fraction_digits++;
  }
  for (int i = 0; i < fraction_digits; i++) v /= 10;
  return static_cast<float>(sign * v);
}

int tokenize_words(const char* line, Words& w) {
  w.count = 0;
  w.args[0] = 0;
  int used = 0;
  const char* p = line;
  for (;;) {
    while (*p == ' ' || *p == '\t' || *p == '\r') p++;
    if (*p == 0 || *p == '\n') break;
    if (p[0] == '/' && p[1] == '/') break;  // a comment runs to the end of the line, which stops
    if (w.count == 1) {
      // The arguments line: from here to the end of the line.
      size_t n = 0;
      while (p[n] != 0 && p[n] != '\n' && n + 1 < sizeof w.args) n++;
      std::memcpy(w.args, p, n);
      w.args[n] = 0;
    }
    char word_buf[kMaxWordBytes + 1];
    int len = 0;
    if (*p == '"') {
      p++;
      while (*p != 0 && *p != '"') {
        if (len < kMaxWordBytes) word_buf[len++] = *p;
        p++;
      }
      if (*p == '"') p++;
    } else {
      while (*p != 0 && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
        if (len < kMaxWordBytes) word_buf[len++] = *p;
        p++;
      }
    }
    if (w.count >= kMaxWords || used + len + 1 > kWordBytes) break;
    std::memcpy(w.text + used, word_buf, static_cast<size_t>(len));
    w.text[used + len] = 0;
    w.at[w.count++] = used;
    used += len + 1;
  }
  return w.count;
}

}  // namespace ge::progs
