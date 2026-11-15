// The platform's loader behind `Library` (library.hpp): dlopen on POSIX, LoadLibraryW on Windows.
//
// ge:summary The platform half of Library: dlopen/dlsym/dlclose and LoadLibraryW/GetProcAddress
// ge:keywords mod dlopen dlsym dlclose dlerror loadlibraryw getprocaddress freelibrary
// ge:keywords getlasterror platform
// ge:skill mod
// ge:test tests/mod_loader_test.cpp
#include "mod/library.hpp"

#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define GE_MOD_ASAN 1
#endif
#endif
#if !defined(GE_MOD_ASAN) && defined(__SANITIZE_ADDRESS__)
#define GE_MOD_ASAN 1
#endif
#if !defined(GE_MOD_ASAN)
#define GE_MOD_ASAN 0
#endif

namespace ge::mod {

Library::~Library() { close(); }

Library::Library(Library&& o) noexcept : handle_(std::exchange(o.handle_, nullptr)) {}

Library& Library::operator=(Library&& o) noexcept {
  if (this != &o) {
    close();
    handle_ = std::exchange(o.handle_, nullptr);
  }
  return *this;
}

void* Library::symbol(const char* name) const {
  if (!handle_) return nullptr;
#if defined(_WIN32)
  return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle_), name));
#else
  return dlsym(handle_, name);
#endif
}

void Library::close() {
  if (!handle_) return;
#if defined(_WIN32)
  FreeLibrary(static_cast<HMODULE>(handle_));
#elif defined(__APPLE__) && GE_MOD_ASAN
    // Left mapped under AddressSanitizer on macOS: it keeps an unloaded image's globals poisoned,
    // so a library mapped later at the same address reads as overflowing its own globals (a refused
    // mod closed, then the next one loaded; shooters_edge_rules_mod_test).
#else
  dlclose(handle_);
#endif
  handle_ = nullptr;
}

bool open_library(const std::filesystem::path& path, Library& out, std::string& error) {
  out.close();
#if defined(_WIN32)
  HMODULE h = LoadLibraryW(path.c_str());
  if (!h) {
    error = "LoadLibraryW failed with error " + std::to_string(GetLastError());
    return false;
  }
  out.handle_ = h;
#else
  void* h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!h) {
    const char* why = dlerror();
    error = why ? why : "dlopen failed";
    return false;
  }
  out.handle_ = h;
#endif
  return true;
}

}  // namespace ge::mod
