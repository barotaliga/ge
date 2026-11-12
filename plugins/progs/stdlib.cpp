// The standard builtins over the VM alone (stdlib.hpp states every rule).
//
// ge:summary The standard builtins' definitions and their table by number
// ge:keywords qc progs builtins stdlib math vectors strings tokenize random kstdlib
// ge:skill progs
// ge:test tests/progs_stdlib_test.cpp
#include "progs/stdlib.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/libm/libm.hpp"
#include "progs/qc_strings.hpp"

namespace ge::progs {

namespace {

constexpr double kDegrees = 180.0 / 3.14159265358979323846;

float length_of(const float v[3]) {
  const float squares = (v[0] * v[0] + v[1] * v[1]) + v[2] * v[2];
  return static_cast<float>(std::sqrt(static_cast<double>(squares)));
}

void bi_normalize(Vm& vm) {
  float v[3];
  arg_vector(vm, 0, v);
  const float len = length_of(v);
  if (len == 0) {
    return_vector(vm, 0, 0, 0);
    return;
  }
  const float inv = 1 / len;
  return_vector(vm, v[0] * inv, v[1] * inv, v[2] * inv);
}

void bi_vlen(Vm& vm) {
  float v[3];
  arg_vector(vm, 0, v);
  return_float(vm, length_of(v));
}

double yaw_of(float x, float y) {
  double yaw = core::libm::atan2(static_cast<double>(y), static_cast<double>(x)) * kDegrees;
  if (yaw < 0) yaw += 360;
  return yaw;
}

void bi_vectoyaw(Vm& vm) {
  float v[3];
  arg_vector(vm, 0, v);
  return_float(vm, v[0] == 0 && v[1] == 0 ? 0.0f : static_cast<float>(yaw_of(v[0], v[1])));
}

void bi_vectoangles(Vm& vm) {
  float v[3];
  arg_vector(vm, 0, v);
  if (v[0] == 0 && v[1] == 0) {
    return_vector(vm, v[2] > 0 ? 90.0f : 270.0f, 0, 0);
    return;
  }
  const double x = v[0], y = v[1];
  const double forward = std::sqrt(x * x + y * y);
  double pitch = core::libm::atan2(static_cast<double>(v[2]), forward) * kDegrees;
  if (pitch < 0) pitch += 360;
  return_vector(vm, static_cast<float>(pitch), static_cast<float>(yaw_of(v[0], v[1])), 0);
}

// k uniform in 0..32767 from the game's generator.
bool draw(Vm& vm, int32_t& k) {
  if (vm.rng == nullptr) {
    vm_error(vm, "random: the game set no generator (Vm::rng); a game sets it before a call");
    return false;
  }
  k = core::int_in(*vm.rng, 0, 32767);
  return true;
}

void bi_random(Vm& vm) {
  int32_t k = 0;
  if (draw(vm, k)) return_float(vm, static_cast<float>(k) / 32767.0f);
}

void bi_randomvec(Vm& vm) {
  float v[3];
  constexpr float kStep = 2.0f / 32767.0f;
  do {
    for (float& c : v) {
      int32_t k = 0;
      if (!draw(vm, k)) return;
      c = static_cast<float>(k) * kStep - 1.0f;
    }
  } while ((v[0] * v[0] + v[1] * v[1]) + v[2] * v[2] >= 1.0f);
  return_vector(vm, v[0], v[1], v[2]);
}

int32_t trunc_i32(double d) {
  if (!(d > -2147483649.0 && d < 2147483648.0)) return INT32_MIN;
  return static_cast<int32_t>(d);
}

void bi_rint(Vm& vm) {
  const double f = arg_float(vm, 0);
  return_float(vm, static_cast<float>(trunc_i32(f > 0 ? f + 0.5 : f - 0.5)));
}
void bi_floor(Vm& vm) { return_float(vm, std::floor(arg_float(vm, 0))); }
void bi_ceil(Vm& vm) { return_float(vm, std::ceil(arg_float(vm, 0))); }
void bi_fabs(Vm& vm) { return_float(vm, std::fabs(arg_float(vm, 0))); }
void bi_sin(Vm& vm) {
  return_float(vm, static_cast<float>(core::libm::sin(static_cast<double>(arg_float(vm, 0)))));
}
void bi_cos(Vm& vm) {
  return_float(vm, static_cast<float>(core::libm::cos(static_cast<double>(arg_float(vm, 0)))));
}
void bi_sqrt(Vm& vm) {
  return_float(vm, static_cast<float>(std::sqrt(static_cast<double>(arg_float(vm, 0)))));
}
void bi_pow(Vm& vm) {
  return_float(vm, static_cast<float>(core::libm::pow(static_cast<double>(arg_float(vm, 0)),
                                                      static_cast<double>(arg_float(vm, 1)))));
}

void min_max(Vm& vm, bool want_max) {
  if (vm.argc < 2) {
    vm_error(vm, want_max ? "max: needs at least 2 arguments" : "min: needs at least 2 arguments");
    return;
  }
  float best = arg_float(vm, 0);
  for (int i = 1; i < vm.argc; i++) {
    const float f = arg_float(vm, i);
    if (want_max ? f > best : f < best) best = f;
  }
  return_float(vm, best);
}
void bi_min(Vm& vm) { min_max(vm, false); }
void bi_max(Vm& vm) { min_max(vm, true); }
void bi_bound(Vm& vm) {
  const float lo = arg_float(vm, 0), v = arg_float(vm, 1), hi = arg_float(vm, 2);
  const float m = v < hi ? v : hi;
  return_float(vm, lo > m ? lo : m);
}

void bi_ftos(Vm& vm) {
  char buf[kFtosBytes];
  format_ftos(arg_float(vm, 0), buf);
  return_int(vm, builtin_temp_string(vm, buf));
}

void bi_vtos(Vm& vm) {
  float v[3];
  arg_vector(vm, 0, v);
  char buf[kVtosBytes];
  format_vtos(v[0], v[1], v[2], buf);
  return_int(vm, builtin_temp_string(vm, buf));
}

void bi_stof(Vm& vm) { return_float(vm, parse_stof(arg_string(vm, 0))); }

void bi_strlen(Vm& vm) { return_float(vm, static_cast<float>(std::strlen(arg_string(vm, 0)))); }

void bi_strcat(Vm& vm) {
  char buf[kTempStringBytes];
  join_args(vm, 0, buf, sizeof buf);
  return_int(vm, builtin_temp_string(vm, buf));
}

void bi_substr(Vm& vm) {
  const char* s = arg_string(vm, 0);
  const int32_t len_s = static_cast<int32_t>(std::strlen(s));
  int64_t start = trunc_i32(arg_float(vm, 1));
  int64_t len = trunc_i32(arg_float(vm, 2));
  if (start < 0) start += len_s;
  if (len < 0) len = len_s - start + len + 1;
  if (start < 0) start = 0;
  char buf[kTempStringBytes];
  buf[0] = 0;
  if (start < len_s && len > 0) {
    int64_t n = len;
    if (n > len_s - start) n = len_s - start;
    if (n > kTempStringBytes - 1) n = kTempStringBytes - 1;
    std::memcpy(buf, s + start, static_cast<size_t>(n));
    buf[n] = 0;
  }
  return_int(vm, builtin_temp_string(vm, buf));
}

void bi_strzone(Vm& vm) { return_int(vm, zone_string(vm, arg_string(vm, 0))); }

void bi_strunzone(Vm& vm) {
  if (!unzone_string(vm, arg_int(vm, 0)))
    vm_error(vm, "strunzone: not a string strzone made (free only what strzone returned)");
}

void bi_tokenize(Vm& vm) {
  return_float(vm, static_cast<float>(tokenize_words(arg_string(vm, 0), vm.words)));
}
void bi_argc(Vm& vm) { return_float(vm, static_cast<float>(vm.words.count)); }
void bi_argv(Vm& vm) {
  return_int(vm, builtin_temp_string(vm, word(vm.words, trunc_i32(arg_float(vm, 0)))));
}

void bi_precache_file(Vm& vm) { return_int(vm, arg_int(vm, 0)); }

// The inverse and the tangent (DP_QC_ASINACOSATANATAN2TAN): in double through core/libm,
// rounded to float, as sin and cos.
void bi_asin(Vm& vm) {
  return_float(vm, static_cast<float>(core::libm::asin(static_cast<double>(arg_float(vm, 0)))));
}
void bi_acos(Vm& vm) {
  return_float(vm, static_cast<float>(core::libm::acos(static_cast<double>(arg_float(vm, 0)))));
}
void bi_atan(Vm& vm) {
  return_float(vm, static_cast<float>(core::libm::atan(static_cast<double>(arg_float(vm, 0)))));
}
void bi_atan2(Vm& vm) {
  return_float(vm, static_cast<float>(core::libm::atan2(static_cast<double>(arg_float(vm, 0)),
                                                        static_cast<double>(arg_float(vm, 1)))));
}
void bi_tan(Vm& vm) {
  return_float(vm, static_cast<float>(core::libm::tan(static_cast<double>(arg_float(vm, 0)))));
}

// ASCII letters only: other bytes (the charset's coloured letters among them) are kept.
char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c; }
char upper(char c) { return c >= 'a' && c <= 'z' ? static_cast<char>(c - 32) : c; }

void change_case(Vm& vm, char (*to)(char)) {
  const char* s = arg_string(vm, 0);
  char buf[kTempStringBytes];
  int n = 0;
  for (; s[n] != 0 && n < kTempStringBytes - 1; n++) buf[n] = to(s[n]);
  buf[n] = 0;
  return_int(vm, builtin_temp_string(vm, buf));
}
void bi_strtolower(Vm& vm) { change_case(vm, lower); }
void bi_strtoupper(Vm& vm) { change_case(vm, upper); }

// Every occurrence of the search text in the subject replaced, left to right, never inside a
// replacement; an empty search returns the subject. The result is cut at the temp string's size.
void replace(Vm& vm, bool any_case) {
  const char* search = arg_string(vm, 0);
  const char* with = arg_string(vm, 1);
  const char* subject = arg_string(vm, 2);
  const size_t ns = std::strlen(search);
  const size_t nw = std::strlen(with);
  char buf[kTempStringBytes];
  size_t used = 0;
  const auto put = [&](const char* from, size_t n) {
    const size_t take = n < sizeof buf - 1 - used ? n : sizeof buf - 1 - used;
    std::memcpy(buf + used, from, take);
    used += take;
  };
  for (const char* at = subject; *at != 0;) {
    bool hit = ns > 0;
    for (size_t i = 0; hit && i < ns; i++)
      hit = at[i] != 0 && (any_case ? lower(at[i]) == lower(search[i]) : at[i] == search[i]);
    if (hit) {
      put(with, nw);
      at += ns;
    } else {
      put(at, 1);
      at++;
    }
  }
  buf[used] = 0;
  return_int(vm, builtin_temp_string(vm, buf));
}
void bi_strreplace(Vm& vm) { replace(vm, false); }
void bi_strireplace(Vm& vm) { replace(vm, true); }

// An entity as text (DP_QC_ETOS): "entity <number>".
void bi_etos(Vm& vm) {
  char buf[32];
  const int32_t v = arg_int(vm, 0);
  std::snprintf(buf, sizeof buf, "entity %d", vm.entity_bytes > 0 ? v / vm.entity_bytes : 0);
  return_int(vm, builtin_temp_string(vm, buf));
}

}  // namespace

float arg_float(const Vm& vm, int i) { return global_float(vm, parm(i)); }

void arg_vector(const Vm& vm, int i, float out[3]) {
  for (int k = 0; k < 3; k++) out[k] = global_float(vm, parm(i) + k);
}

int32_t arg_int(const Vm& vm, int i) { return global_int(vm, parm(i)); }

const char* arg_string(Vm& vm, int i) {
  const char* s = vm_string(vm, arg_int(vm, i));
  if (s == nullptr) {
    char m[96];
    std::snprintf(m, sizeof m, "argument %d is not a string (value %d)", i + 1, arg_int(vm, i));
    vm_error(vm, m);
    return "";
  }
  return s;
}

void return_float(Vm& vm, float v) { set_global_float(vm, kGlobalReturn, v); }

void return_vector(Vm& vm, float x, float y, float z) {
  set_global_float(vm, kGlobalReturn, x);
  set_global_float(vm, kGlobalReturn + 1, y);
  set_global_float(vm, kGlobalReturn + 2, z);
}

void return_int(Vm& vm, int32_t v) { set_global_int(vm, kGlobalReturn, v); }

void join_args(Vm& vm, int first, char* out, int cap) {
  int used = 0;
  out[0] = 0;
  for (int i = first; i < vm.argc && i < kMaxParms; i++) {
    const char* s = arg_string(vm, i);
    const auto n = static_cast<int>(std::strlen(s));
    const int take = n < cap - 1 - used ? n : cap - 1 - used;
    std::memcpy(out + used, s, static_cast<size_t>(take));
    used += take;
    out[used] = 0;
  }
}

const StdBuiltin kStdlib[] = {
    {7, "random", bi_random},
    {9, "normalize", bi_normalize},
    {12, "vlen", bi_vlen},
    {13, "vectoyaw", bi_vectoyaw},
    {26, "ftos", bi_ftos},
    {27, "vtos", bi_vtos},
    {36, "rint", bi_rint},
    {37, "floor", bi_floor},
    {38, "ceil", bi_ceil},
    {43, "fabs", bi_fabs},
    {51, "vectoangles", bi_vectoangles},
    {60, "sin", bi_sin},
    {61, "cos", bi_cos},
    {62, "sqrt", bi_sqrt},
    {68, "precache_file", bi_precache_file},
    {77, "precache_file2", bi_precache_file},
    {81, "stof", bi_stof},
    {84, "tokenize", bi_tokenize},
    {85, "argc", bi_argc},
    {86, "argv", bi_argv},
    {91, "randomvec", bi_randomvec},
    {94, "min", bi_min},
    {95, "max", bi_max},
    {96, "bound", bi_bound},
    {97, "pow", bi_pow},
    {114, "strlen", bi_strlen},
    {115, "strcat", bi_strcat},
    {116, "substr", bi_substr},
    {118, "strzone", bi_strzone},
    {119, "strunzone", bi_strunzone},
    {65, "etos", bi_etos},
    {471, "asin", bi_asin},
    {472, "acos", bi_acos},
    {473, "atan", bi_atan},
    {474, "atan2", bi_atan2},
    {475, "tan", bi_tan},
    {480, "strtolower", bi_strtolower},
    {481, "strtoupper", bi_strtoupper},
    {484, "strreplace", bi_strreplace},
    {485, "strireplace", bi_strireplace},
    {117, "stov", bi_stov},
    {221, "strstrofs", bi_strstrofs},
    {222, "str2chr", bi_str2chr},
    {223, "chr2str", bi_chr2str},
    {224, "strconv", bi_strconv},
    {225, "strpad", bi_strpad},
    {226, "infoadd", bi_infoadd},
    {227, "infoget", bi_infoget},
    {228, "strncmp", bi_strncmp},
    {229, "strcasecmp", bi_strcasecmp},
    {230, "strncasecmp", bi_strcasecmp},
    {627, "sprintf", bi_sprintf},
};
const int kStdlibCount = static_cast<int>(sizeof kStdlib / sizeof kStdlib[0]);

const StdBuiltin* stdlib_builtin(const char* name) {
  for (int i = 0; i < kStdlibCount; i++)
    if (std::strcmp(kStdlib[i].name, name) == 0) return &kStdlib[i];
  return nullptr;
}

}  // namespace ge::progs
