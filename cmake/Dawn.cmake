# THE GPU BACKEND's dependency: Dawn (a native GPU API over Metal, Vulkan and D3D12; header webgpu.h) as its prebuilt GitHub release,
# pinned to one tag, and Tint (`tint`, its shader compiler) from the same archive for
# `make shaders-check`. docs/rendering.md § The backend is the decision; the gpu skill is the
# procedure for moving the pin.
#
# GE_GPU decides whether engine/gpu (and everything that draws) is built. It defaults ON where
# a prebuilt archive exists for the target (Linux x64, macOS arm64, Windows x64; iOS and the iOS
# Simulator; Android) and OFF elsewhere (Linux arm64, until Dawn is built from source there:
# milestone M11a). With it OFF the rest of the engine builds and tests unchanged.
#
# THE MOBILE TARGETS take the same release's mobile archives: the Apple xcframework (a static
# library for the iPhone and iPad, one for the iOS Simulator, one for the Mac), the Android
# static libraries (one an ABI), and the headers archive beside them. Their CMake package is
# not usable (the headers archive's was written by the Android build, with that build machine's
# paths), so dawn::webgpu_dawn is declared here over the target's slice, with the system's
# libraries it needs.
#
# The archive is downloaded once into GE_DEPS_DIR (default .cache/deps/, gitignored, shared by
# every preset and cached by CI) and checked against its SHA-256. GE_DAWN_DIR names an
# unpacked Dawn install instead (a local source build, or a mirror), skipping the download.
#
# THE WEB (Emscripten) takes no archive: dawn::webgpu_dawn is Emscripten's binding of the same API
# over the browser's WebGPU (`--use-port=emdawnwebgpu`).
#
# Defines, when GE_GPU is ON: the imported target dawn::webgpu_dawn, and GE_TINT (the path of
# the tint executable; empty on the web).
include_guard(GLOBAL)

# THE PIN. Moving it: pick a tag whose release has all three archives, update the tag, the
# commit and the three hashes together, rebuild, and rerun every gpu test and lab (gpu skill).
set(GE_DAWN_TAG "v20261016.190333")
set(GE_DAWN_COMMIT "67d4d6e64b0755d638f76549794692badfdbc8ab")
set(GE_DAWN_SHA256_ubuntu-latest "20ff88448def9645ceeff30911e07295aeb433be23e26180c96019b4300bff5a")
set(GE_DAWN_SHA256_macos-latest "736fccba78b8a2fde8bfd55d045e890933b0a01776aed5a29b9f024735404193")
set(GE_DAWN_SHA256_windows-latest "57402c6aa887e30d3e5710707d6b0677d8292538ab941628c624635022bfa49f")
# The mobile archives of the same release (dawn-<kind>-<commit>...tar.gz).
set(GE_DAWN_SHA256_apple "9236e51e98e3423c1fd18cdf5d291a3c2021e8620960d44a97db219012182452")
set(GE_DAWN_SHA256_android "6add2f7fa18b9ff3c34018c0b24474b73fb61111dcf2810e392d269cae1c6f99")
set(GE_DAWN_SHA256_headers "6cb3c021fdee53938033157392337af0db942212b872168a01957ec0eff5e077")

# Which archive serves this host, if any.
set(ge_dawn_os "")
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ge_cpu)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND ge_cpu MATCHES "^(x86_64|amd64)$")
  set(ge_dawn_os "ubuntu-latest")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND ge_cpu MATCHES "^(arm64|aarch64)$")
  set(ge_dawn_os "macos-latest")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND ge_cpu MATCHES "^(x86_64|amd64)$")
  set(ge_dawn_os "windows-latest")
endif()
# A mobile target: its archive and the slice in it (the SDK CMAKE_OSX_SYSROOT names for iOS, the
# ABI for Android).
set(ge_dawn_mobile "")
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(ge_dawn_mobile "apple")
  if(CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
    set(ge_dawn_slice "dawn-apple-${GE_DAWN_COMMIT}.xcframework/ios-arm64_x86_64-simulator")
  else()
    set(ge_dawn_slice "dawn-apple-${GE_DAWN_COMMIT}.xcframework/ios-arm64")
  endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(ge_dawn_mobile "android")
  set(ge_dawn_slice "dawn-android-${GE_DAWN_COMMIT}/${CMAKE_ANDROID_ARCH_ABI}")
endif()

if(ge_dawn_os OR ge_dawn_mobile OR GE_DAWN_DIR OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(ge_gpu_default ON)
else()
  set(ge_gpu_default OFF)
endif()
option(GE_GPU "Build engine/gpu and everything that draws (needs Dawn)" ${ge_gpu_default})
set(GE_DEPS_DIR "${PROJECT_SOURCE_DIR}/.cache/deps" CACHE PATH
  "Where prebuilt dependencies are downloaded and unpacked")
set(GE_DAWN_DIR "" CACHE PATH "An unpacked Dawn install to use instead of the pinned download")

if(NOT GE_GPU)
  return()
endif()

if(MSVC AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(FATAL_ERROR "ge: GE_GPU=ON with an MSVC Debug build: the prebuilt Dawn is a Release "
    "library and its C++ runtime does not mix with a Debug one. Build the release preset "
    "(PRESET=release), or configure with -DGE_GPU=OFF; skill: build.")
endif()

# THE WEB: the browser is the GPU. Emscripten's own binding of Dawn's API (emdawnwebgpu, the same
# webgpu_cpp.h over the page's WebGPU) stands in for the library; there is no Tint to run here.
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  add_library(dawn::webgpu_dawn INTERFACE IMPORTED GLOBAL)
  target_compile_options(dawn::webgpu_dawn INTERFACE --use-port=emdawnwebgpu)
  target_link_options(dawn::webgpu_dawn INTERFACE --use-port=emdawnwebgpu)
  set(GE_TINT "")
  return()
endif()

# One archive of the pinned release, downloaded into GE_DEPS_DIR, checked and unpacked there.
function(ge_dawn_fetch file sha256)
  set(archive "${GE_DEPS_DIR}/${file}")
  set(url "https://github.com/google/dawn/releases/download/${GE_DAWN_TAG}/${file}")
  message(STATUS "ge: downloading Dawn ${GE_DAWN_TAG} (${file}) into ${GE_DEPS_DIR}")
  file(DOWNLOAD "${url}" "${archive}" EXPECTED_HASH SHA256=${sha256} STATUS status TLS_VERIFY ON)
  list(GET status 0 code)
  if(NOT code EQUAL 0)
    list(GET status 1 why)
    file(REMOVE "${archive}")
    message(FATAL_ERROR "ge: downloading ${url} failed (${why}). Check the network, or pass "
      "-DGE_DAWN_DIR=<an unpacked Dawn install>, or -DGE_GPU=OFF; skill: gpu.")
  endif()
  file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${GE_DEPS_DIR}")
  file(REMOVE "${archive}")
endfunction()

if(ge_dawn_mobile AND NOT GE_DAWN_DIR)
  # The headers unpack as dawn-headers/: kept under the commit's name, so a new pin fetches anew.
  set(ge_dawn_headers "${GE_DEPS_DIR}/dawn-headers-${GE_DAWN_COMMIT}")
  if(NOT EXISTS "${ge_dawn_headers}/include/webgpu/webgpu_cpp.h")
    ge_dawn_fetch("dawn-headers-${GE_DAWN_COMMIT}.tar.gz" ${GE_DAWN_SHA256_headers})
    file(REMOVE_RECURSE "${ge_dawn_headers}")
    file(RENAME "${GE_DEPS_DIR}/dawn-headers" "${ge_dawn_headers}")
  endif()
  set(ge_dawn_lib "${GE_DEPS_DIR}/${ge_dawn_slice}/libwebgpu_dawn.a")
  if(NOT EXISTS "${ge_dawn_lib}")
    if(ge_dawn_mobile STREQUAL "apple")
      ge_dawn_fetch("dawn-apple-${GE_DAWN_COMMIT}.xcframework.tar.gz" ${GE_DAWN_SHA256_apple})
    else()
      ge_dawn_fetch("dawn-android-${GE_DAWN_COMMIT}.tar.gz" ${GE_DAWN_SHA256_android})
    endif()
  endif()
  if(NOT EXISTS "${ge_dawn_lib}")
    message(FATAL_ERROR "ge: Dawn's ${ge_dawn_mobile} archive has no ${ge_dawn_slice}/"
      "libwebgpu_dawn.a for ${CMAKE_SYSTEM_NAME} (${CMAKE_OSX_SYSROOT}${CMAKE_ANDROID_ARCH_ABI}). "
      "Configure with -DGE_GPU=OFF, or pass -DGE_DAWN_DIR=<a Dawn install for it>; skill: gpu.")
  endif()
  find_package(Threads REQUIRED)
  add_library(dawn::webgpu_dawn STATIC IMPORTED GLOBAL)
  set_target_properties(dawn::webgpu_dawn PROPERTIES
    IMPORTED_LOCATION "${ge_dawn_lib}"
    INTERFACE_INCLUDE_DIRECTORIES "${ge_dawn_headers}/include")
  if(ge_dawn_mobile STREQUAL "apple")
    target_link_libraries(dawn::webgpu_dawn INTERFACE Threads::Threads "-framework CoreFoundation"
      "-framework Foundation" "-framework IOSurface" "-framework QuartzCore" "-framework Metal"
      "-framework UIKit")
  else()
    target_link_libraries(dawn::webgpu_dawn INTERFACE Threads::Threads log android dl)
  endif()
  return()
endif()

if(NOT GE_DAWN_DIR)
  if(NOT ge_dawn_os)
    message(FATAL_ERROR "ge: GE_GPU=ON but Dawn publishes no prebuilt archive for "
      "${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}. Configure with -DGE_GPU=OFF, or build "
      "Dawn from source and pass -DGE_DAWN_DIR=<its install prefix>; skill: gpu.")
  endif()
  set(ge_dawn_name "Dawn-${GE_DAWN_COMMIT}-${ge_dawn_os}-Release")
  set(GE_DAWN_DIR "${GE_DEPS_DIR}/${ge_dawn_name}")
  if(NOT EXISTS "${GE_DAWN_DIR}/include/webgpu/webgpu.h")
    ge_dawn_fetch("${ge_dawn_name}.tar.gz" ${GE_DAWN_SHA256_${ge_dawn_os}})
  endif()
endif()

file(GLOB ge_dawn_config "${GE_DAWN_DIR}/lib*/cmake/Dawn/DawnConfig.cmake")
if(NOT ge_dawn_config)
  message(FATAL_ERROR "ge: no lib*/cmake/Dawn/DawnConfig.cmake under GE_DAWN_DIR="
    "${GE_DAWN_DIR}. It must be a Dawn install prefix (include/, lib/ and bin/); skill: gpu.")
endif()
list(GET ge_dawn_config 0 ge_dawn_config)
get_filename_component(Dawn_DIR "${ge_dawn_config}" DIRECTORY)
find_package(Threads REQUIRED)
find_package(Dawn CONFIG REQUIRED NO_DEFAULT_PATH PATHS "${Dawn_DIR}")
find_program(GE_TINT NAMES tint PATHS "${GE_DAWN_DIR}/bin" NO_DEFAULT_PATH)

# Dawn's D3D12 backend compiles shaders with DXC: it loads dxcompiler.dll from beside the
# executable, and dxcompiler.dll loads its validator, dxil.dll, the same way. The prebuilt archive
# carries neither, so the Windows SDK's copies go beside every executable, as a shipped game
# carries them. Called at the end of the top-level CMakeLists.txt, once every executable exists.
function(ge_dxc_beside_executables)
  file(TO_CMAKE_PATH "$ENV{WindowsSdkVerBinPath}x64" sdk_bin)
  find_file(GE_DXCOMPILER dxcompiler.dll HINTS "${GE_DAWN_DIR}/bin" "${sdk_bin}" NO_DEFAULT_PATH)
  find_file(GE_DXIL dxil.dll HINTS "${GE_DAWN_DIR}/bin" "${sdk_bin}" NO_DEFAULT_PATH)
  if(NOT GE_DXCOMPILER OR NOT GE_DXIL)
    message(WARNING "ge: no dxcompiler.dll and dxil.dll in the Windows SDK (${sdk_bin}), so "
      "Dawn's D3D12 backend creates no device. Configure in MSVC's x64 environment (vcvars64), "
      "which names the SDK; skill: gpu.")
    return()
  endif()
  _ge_executable_dirs("${PROJECT_SOURCE_DIR}" dirs)
  list(REMOVE_DUPLICATES dirs)
  foreach(dir IN LISTS dirs)
    file(COPY "${GE_DXCOMPILER}" "${GE_DXIL}" DESTINATION "${dir}")
  endforeach()
endfunction()

# Where the executables defined in a directory and the directories under it are written.
function(_ge_executable_dirs dir out)
  set(dirs "")
  get_property(targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
  foreach(t IN LISTS targets)
    get_target_property(type ${t} TYPE)
    if(type STREQUAL "EXECUTABLE")
      get_target_property(exe_dir ${t} RUNTIME_OUTPUT_DIRECTORY)
      if(NOT exe_dir)
        get_target_property(exe_dir ${t} BINARY_DIR)
      endif()
      list(APPEND dirs "${exe_dir}")
    endif()
  endforeach()
  get_property(subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
  foreach(sub IN LISTS subdirs)
    _ge_executable_dirs("${sub}" sub_exe_dirs)
    list(APPEND dirs ${sub_exe_dirs})
  endforeach()
  set(${out} ${dirs} PARENT_SCOPE)
endfunction()
