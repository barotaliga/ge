// The extended string builtins and sprintf (qc_strings.hpp states every rule).
//
// ge:summary The extended string builtins' and sprintf's definitions
// ge:keywords qc progs builtins strconv sprintf infoadd infoget strstrofs fte-strings
// ge:skill progs
// ge:test tests/progs_qc_strings_test.cpp
#include "progs/qc_strings.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "progs/stdlib.hpp"
#include "progs/text.hpp"

namespace ge::progs {

namespace {

constexpr int kCap = kTempStringBytes - 1;  // a result's bytes at most

int32_t to_int(float f) {
  if (!(f > -2147483904.0f && f < 2147483648.0f)) return INT32_MIN;
  return static_cast<int32_t>(f);
}

int32_t count_arg(const Vm& vm, int i, int32_t missing) {
  return i < vm.argc ? to_int(arg_float(vm, i)) : missing;
}

void give(Vm& vm, std::string s) {
  if (s.size() > static_cast<size_t>(kCap)) s.resize(static_cast<size_t>(kCap));
  return_int(vm, builtin_temp_string(vm, s.c_str()));
}

std::string joined(Vm& vm, int first) {
  char buf[kTempStringBytes];
  join_args(vm, first, buf, sizeof buf);
  return buf;
}

bool is_digit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

void bi_stov(Vm& vm) {
  const std::string text = joined(vm, 0);
  const char* p = text.c_str();
  if (*p == '\'') p++;
  float v[3] = {0, 0, 0};
  for (float& out : v) {
    while (*p == ' ' || *p == '\t') p++;
    std::string token;
    while (*p != 0 && *p != ' ' && *p != '\t' && *p != '\'') token += *p++;
    const float f = parse_stof(token.c_str());
    if (f == 0 && !(token.size() > 0 && (is_digit(token[0]) || token[0] == '+' || token[0] == '-')))
      break;
    out = f;
    if (*p == '\'' || *p == 0) break;
  }
  return_vector(vm, v[0], v[1], v[2]);
}

void bi_strstrofs(Vm& vm) {
  const char* s = arg_string(vm, 0);
  const char* sub = arg_string(vm, 1);
  const auto len = static_cast<int32_t>(std::strlen(s));
  const int32_t start = count_arg(vm, 2, 0);
  if (start < 0 || start > len) {
    return_float(vm, -1);
    return;
  }
  const char* hit = std::strstr(s + start, sub);
  return_float(vm, hit == nullptr ? -1.0f : static_cast<float>(hit - s));
}

void bi_str2chr(Vm& vm) {
  const char* s = arg_string(vm, 0);
  const auto len = static_cast<int32_t>(std::strlen(s));
  int32_t i = count_arg(vm, 1, 0);
  if (i < 0) i += len;
  return_float(vm, i < 0 || i >= len ? 0.0f : static_cast<float>(static_cast<unsigned char>(s[i])));
}

void bi_chr2str(Vm& vm) {
  std::string out;
  for (int i = 0; i < vm.argc && i < kMaxParms; i++) {
    const int32_t c = to_int(arg_float(vm, i));
    if (c == 0) break;
    out += c >= 1 && c <= 255 ? static_cast<char>(c) : '?';
  }
  give(vm, out);
}

void bi_strconv(Vm& vm) {
  const int32_t ccase = count_arg(vm, 0, 0);
  const int32_t letters = count_arg(vm, 1, 0);
  const int32_t digits = count_arg(vm, 2, 0);
  std::string s = joined(vm, 3);
  for (size_t i = 0; i < s.size(); i++) {
    auto c = static_cast<unsigned char>(s[i]);
    const unsigned low = c & 0x7fu;
    const bool high = (c & 0x80u) != 0;
    if (low >= '0' && low <= '9') {
      const unsigned d = low - '0';
      if (digits == 1) c = static_cast<unsigned char>('0' + d);
      if (digits == 2) c = static_cast<unsigned char>(176 + d);
      if (digits == 3) c = static_cast<unsigned char>(18 + d);
      if (digits == 4) c = static_cast<unsigned char>(146 + d);
    } else if (low >= 18 && low <= 27) {  // the gold digits, either half
      const unsigned d = low - 18;
      if (digits == 1) c = static_cast<unsigned char>('0' + d);
      if (digits == 2) c = static_cast<unsigned char>(176 + d);
      if (digits == 3) c = static_cast<unsigned char>(18 + d);
      if (digits == 4) c = static_cast<unsigned char>(146 + d);
    } else if ((low >= 'a' && low <= 'z') || (low >= 'A' && low <= 'Z')) {
      unsigned l = low;
      if (ccase == 1 && l >= 'A' && l <= 'Z') l += 32;
      if (ccase == 2 && l >= 'a' && l <= 'z') l -= 32;
      bool red = high;
      if (letters == 1) red = false;
      if (letters == 2) red = true;
      if (letters == 5) red = i % 2 == 0;
      if (letters == 6) red = i % 2 == 1;
      c = static_cast<unsigned char>(l | (red ? 0x80u : 0u));
    } else if (low >= 16) {
      if (letters == 1) c = static_cast<unsigned char>(low);
      if (letters == 2) c = static_cast<unsigned char>(low | 0x80u);
    }
    s[i] = static_cast<char>(c);
  }
  give(vm, s);
}

void bi_strpad(Vm& vm) {
  const int32_t width = count_arg(vm, 0, 0);
  const std::string s = joined(vm, 1);
  const int64_t want = width < 0 ? -static_cast<int64_t>(width) : width;
  const int64_t pad =
      want > static_cast<int64_t>(s.size()) ? want - static_cast<int64_t>(s.size()) : 0;
  const std::string spaces(static_cast<size_t>(pad < kCap ? pad : kCap), ' ');
  give(vm, width < 0 ? spaces + s : s + spaces);
}

namespace {

// The first pair whose key is `key`: its place in `info` (from its leading backslash) and length,
// and its value. False when none.
bool find_pair(const std::string& info, const std::string& key, size_t& at, size_t& len,
               std::string& value) {
  size_t p = 0;
  while (p < info.size()) {
    const size_t begin = p;
    if (info[p] == '\\') p++;
    const size_t key_end = info.find('\\', p);
    if (key_end == std::string::npos) return false;  // the text ends inside a key
    const size_t value_end = info.find('\\', key_end + 1);
    const size_t end = value_end == std::string::npos ? info.size() : value_end;
    if (info.compare(p, key_end - p, key) == 0 && key_end - p == key.size()) {
      at = begin;
      len = end - begin;
      value = info.substr(key_end + 1, end - key_end - 1);
      return true;
    }
    p = end;
  }
  return false;
}

bool refused(const std::string& s) { return s.find_first_of("\\\"") != std::string::npos; }

}  // namespace

void bi_infoadd(Vm& vm) {
  std::string info = arg_string(vm, 0);
  const std::string key = arg_string(vm, 1);
  std::string value = joined(vm, 2);
  if (info.size() > static_cast<size_t>(kCap)) info.resize(static_cast<size_t>(kCap));
  if (refused(key) || refused(value) || key.size() >= 256) {
    give(vm, info);
    return;
  }
  size_t at = 0, len = 0;
  std::string old;
  if (find_pair(info, key, at, len, old)) info.erase(at, len);
  if (!value.empty()) {
    std::string pair = "\\" + key + "\\" + value;
    if (pair.size() > 1023) pair.resize(1023);
    std::string kept;
    for (const char c : pair)
      if (static_cast<unsigned char>(c) == 92 || static_cast<unsigned char>(c) >= 14) kept += c;
    if (info.size() + kept.size() <= static_cast<size_t>(kCap)) info += kept;
  }
  give(vm, info);
}

void bi_infoget(Vm& vm) {
  const std::string info = arg_string(vm, 0);
  const std::string key = arg_string(vm, 1);
  size_t at = 0, len = 0;
  std::string value;
  give(vm, find_pair(info, key, at, len, value) ? value : std::string());
}

namespace {

char fold(char c) { return c >= 'a' && c <= 'z' ? static_cast<char>(c - 32) : c; }

void compare(Vm& vm, bool any_case) {
  const char* a = arg_string(vm, 0);
  const char* b = arg_string(vm, 1);
  int64_t n = INT64_MAX;
  if (vm.argc >= 3) {
    const int32_t len = count_arg(vm, 2, 0);
    if (len == 0) {
      return_float(vm, 0);
      return;
    }
    if (len > 0) n = len;
    const auto la = static_cast<int32_t>(std::strlen(a));
    const auto lb = static_cast<int32_t>(std::strlen(b));
    int32_t from_a = count_arg(vm, 3, 0), from_b = count_arg(vm, 4, 0);
    if (from_a < 0 || from_a > la) from_a = la;
    if (from_b < 0 || from_b > lb) from_b = lb;
    a += from_a;
    b += from_b;
  }
  for (int64_t i = 0; i < n; i++) {
    char x = a[i], y = b[i];
    if (any_case) {
      x = fold(x);
      y = fold(y);
    }
    const auto ux = static_cast<unsigned char>(x), uy = static_cast<unsigned char>(y);
    if (ux != uy) {
      return_float(vm, ux < uy ? -1.0f : 1.0f);
      return;
    }
    if (ux == 0) break;
  }
  return_float(vm, 0);
}

}  // namespace

void bi_strncmp(Vm& vm) { compare(vm, false); }
void bi_strcasecmp(Vm& vm) { compare(vm, true); }

// sprintf: one directive at a time, each through the C library's formatting of one value.
namespace {

struct Spec {
  std::string flags;
  int width = -1, precision = -1;
  bool left = false;  // a negative width from '*'
  int as = 0;         // 0 the default, 'h' a float, 'l' a 32-bit integer
};

// Argument `k` (1 the first after the format) as a float, an integer or a string; past those
// passed, zero or "".
float float_at(const Vm& vm, int k) {
  return k >= 1 && k < vm.argc && k < kMaxParms ? global_float(vm, parm(k)) : 0.0f;
}
int32_t int_at(const Vm& vm, int k) {
  return k >= 1 && k < vm.argc && k < kMaxParms ? global_int(vm, parm(k)) : 0;
}
std::string string_at(Vm& vm, int k) {
  if (k < 1 || k >= vm.argc || k >= kMaxParms) return "";
  const char* s = vm_string(vm, global_int(vm, parm(k)));
  return s == nullptr ? "" : s;
}

int64_t to_int64(float f) {
  if (!(f > -9.2233720e18f && f < 9.2233720e18f)) return 0;
  return static_cast<int64_t>(f);
}

std::string c_format(const Spec& s, const char* length, char conversion) {
  std::string f = "%" + s.flags;
  if (s.left) f += '-';
  if (s.width >= 0) f += std::to_string(s.width);
  if (s.precision >= 0) f += "." + std::to_string(s.precision);
  return f + length + conversion;
}

template <typename T>
std::string format_one(const std::string& f, T v) {
  char buf[kTempStringBytes];
  std::snprintf(buf, sizeof buf, f.c_str(), v);
  return buf;
}

// A string quoted for the console.
std::string quoted(const std::string& s) {
  if (s.find_first_of("\n\r\"") == std::string::npos) return "\"" + s + "\"";
  std::string out = "\\\"";
  for (const char c : s) {
    switch (c) {
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      case '\'':
      case '"':
      case '\\':
      case '$':
        out += '\\';
        out += c;
        break;
      default:
        out += c;
    }
  }
  return out + "\"";
}

// Digits at `p` read as a number; -1 and `p` unmoved when none.
int read_number(const char*& p) {
  if (!is_digit(*p)) return -1;
  int n = 0;
  while (is_digit(*p)) {
    if (n < 100000) n = n * 10 + (*p - '0');
    p++;
  }
  return n;
}

// `N$` at `p`: N, else -1 with `p` unmoved.
int read_position(const char*& p) {
  const char* q = p;
  const int n = read_number(q);
  if (n < 0 || *q != '$') return -1;
  p = q + 1;
  return n;
}

}  // namespace

void bi_sprintf(Vm& vm) {
  const std::string format = arg_string(vm, 0);
  std::string out;
  int next = 1;
  for (const char* p = format.c_str(); *p != 0 && out.size() < static_cast<size_t>(kCap);) {
    if (*p != '%') {
      out += *p++;
      continue;
    }
    p++;
    if (*p == '%') {
      out += '%';
      p++;
      continue;
    }
    Spec s;
    int position = read_position(p);
    while (*p == '#' || *p == '0' || *p == '-' || *p == ' ' || *p == '+') s.flags += *p++;
    if (*p == '*') {
      p++;
      int k = read_position(p);
      if (k < 0) {
        if (is_digit(*p)) break;  // '*N' without '$'
        k = next++;
      }
      const int32_t w = to_int(float_at(vm, k));
      s.left = w < 0;
      s.width = w < 0 ? (w == INT32_MIN ? 0 : -w) : w;
    } else {
      s.width = read_number(p);
    }
    if (s.width > kCap) s.width = kCap;
    if (*p == '.') {
      p++;
      if (*p == '*') {
        p++;
        int k = read_position(p);
        if (k < 0) {
          if (is_digit(*p)) break;
          k = next++;
        }
        const int32_t v = to_int(float_at(vm, k));
        s.precision = v < 0 ? -1 : v;
      } else {
        s.precision = read_number(p);
        if (s.precision < 0) break;  // '.' with no digits
      }
      if (s.precision > kCap) s.precision = kCap;
    }
    while (*p == 'h' || *p == 'l' || *p == 'L' || *p == 'q' || *p == 'j' || *p == 'z' ||
           *p == 't') {
      if (*p == 'h') s.as = 'h';
      if (*p == 'l' || *p == 'L') s.as = 'l';
      p++;
    }
    const char conv = *p;
    if (conv == 0) break;
    p++;
    if (position < 0) position = next++;
    const bool integer_arg =
        s.as == 'l' || (s.as == 0 && (conv == 'i' || conv == 'p' || conv == 'P'));
    switch (conv) {
      case 'd':
      case 'i':
      case 'I': {
        const int64_t v = integer_arg ? int_at(vm, position) : to_int64(float_at(vm, position));
        out += format_one(c_format(s, "ll", 'd'), static_cast<long long>(v));
        break;
      }
      case 'o':
      case 'u':
      case 'x':
      case 'X':
      case 'p':
      case 'P': {
        uint64_t v = integer_arg ? static_cast<uint32_t>(int_at(vm, position))
                                 : static_cast<uint64_t>(to_int64(float_at(vm, position)));
        Spec t = s;
        char c = conv == 'p' ? 'x' : conv == 'P' ? 'X' : conv;
        if ((conv == 'p' || conv == 'P') && t.width < 0) {
          t.width = 8;
          if (t.flags.find('0') == std::string::npos) t.flags += '0';
        }
        out += format_one(c_format(t, "ll", c), static_cast<unsigned long long>(v));
        break;
      }
      case 'e':
      case 'E':
      case 'f':
      case 'F':
      case 'g':
      case 'G': {
        const double v = s.as == 'l' ? static_cast<double>(int_at(vm, position))
                                     : static_cast<double>(float_at(vm, position));
        out += format_one(c_format(s, "", conv), v);
        break;
      }
      case 'v':
      case 'V': {
        const std::string f = c_format(s, "", conv == 'v' ? 'g' : 'G');
        for (int k = 0; k < 3; k++) {
          const bool in_range = position >= 1 && position < vm.argc && position < kMaxParms;
          const int g = parm(position) + k;
          const double v = !in_range     ? 0.0
                           : s.as == 'l' ? static_cast<double>(global_int(vm, g))
                                         : static_cast<double>(global_float(vm, g));
          if (k > 0) out += ' ';
          out += format_one(f, v);
        }
        break;
      }
      case 'c': {
        const uint64_t v = s.as == 'l' ? static_cast<uint32_t>(int_at(vm, position))
                                       : static_cast<uint64_t>(to_int64(float_at(vm, position)));
        const auto byte = static_cast<char>(v & 0xffu);
        std::string one = byte == 0 ? std::string() : std::string(1, byte);
        if (s.width > 1 && byte != 0) {
          const std::string pad(static_cast<size_t>(s.width - 1), ' ');
          if (s.left || s.flags.find('-') != std::string::npos)
            one.append(pad);
          else
            one.insert(0, pad);
        }
        out += one;
        break;
      }
      case 's':
      case 'S': {
        std::string text = string_at(vm, position);
        if (conv == 'S') text = quoted(text);
        Spec t = s;
        t.flags.erase(std::remove(t.flags.begin(), t.flags.end(), '#'), t.flags.end());
        out += format_one(c_format(t, "", 's'), text.c_str());
        break;
      }
      default:
        p = format.c_str() + format.size();  // an unknown conversion: the text so far
        continue;
    }
  }
  give(vm, out);
}

}  // namespace ge::progs
