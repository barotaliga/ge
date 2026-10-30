// A SHARED LIBRARY OPENED AT RUN TIME: the one platform file of engine/mod. `dlopen` on Linux and
// macOS (RTLD_NOW | RTLD_LOCAL: every symbol resolved at load, none leaked to the next library),
// `LoadLibraryW` on Windows; `symbol` looks one name up; the library closes when the object dies.
// Nothing else in the engine includes the platform's loader headers.
//
// ge:summary A shared library opened at run time (dlopen, LoadLibraryW): open, symbol, close
// ge:keywords mod shared-library dlopen dlsym loadlibrary getprocaddress dll so dylib library
// ge:keywords open_library symbol platform
// ge:skill mod
// ge:test tests/mod_loader_test.cpp
#pragma once

#include <filesystem>
#include <string>

namespace ge::mod {

class Library {
 public:
  Library() = default;
  ~Library();
  Library(Library&& o) noexcept;
  Library& operator=(Library&& o) noexcept;
  Library(const Library&) = delete;
  Library& operator=(const Library&) = delete;

  bool is_open() const { return handle_ != nullptr; }
  // The exported symbol `name`, or null.
  void* symbol(const char* name) const;
  void close();

 private:
  friend bool open_library(const std::filesystem::path& path, Library& out, std::string& error);
  void* handle_ = nullptr;
};

// Opens `path`; false with the platform's reason in `error`.
bool open_library(const std::filesystem::path& path, Library& out, std::string& error);

}  // namespace ge::mod
