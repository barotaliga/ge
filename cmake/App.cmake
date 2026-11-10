# A GAME'S APP BUNDLE (docs/goals.md M11): on iOS and iPadOS a windowed game's executable is an
# app bundle with an Info.plist (cmake/ios/Info.plist.in: the identifier, the version, a launch
# screen, full screen, landscape, iPhone and iPad). Elsewhere it does nothing: a desktop game is
# a plain executable.
#
#   ge_app_bundle(<target> NAME <shown name> ID <identifier's last part> [VERSION <x.y>])
#
# The identifier is GE_BUNDLE_ID_PREFIX.<ID>. A plain checkout's is `dev.local.<ID>`; what
# identifies an app in a store is never committed: a signed build takes its team's prefix from
# the environment (GE_BUNDLE_ID_PREFIX), as its signing credentials are.
include_guard(GLOBAL)

if(DEFINED ENV{GE_BUNDLE_ID_PREFIX} AND NOT "$ENV{GE_BUNDLE_ID_PREFIX}" STREQUAL "")
  set(ge_bundle_prefix_default "$ENV{GE_BUNDLE_ID_PREFIX}")
else()
  set(ge_bundle_prefix_default "dev.local")
endif()
set(GE_BUNDLE_ID_PREFIX "${ge_bundle_prefix_default}" CACHE STRING
  "The reverse-DNS prefix of a game app's bundle identifier on iOS (env GE_BUNDLE_ID_PREFIX)")

function(ge_app_bundle target)
  cmake_parse_arguments(A "" "NAME;ID;VERSION" "" ${ARGN})
  foreach(key NAME ID)
    if(NOT A_${key})
      message(FATAL_ERROR "ge_app_bundle(${target}): no ${key}; next: pass it (cmake/App.cmake "
        "lists the arguments)")
    endif()
  endforeach()
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    return()
  endif()
  if(NOT A_VERSION)
    set(A_VERSION "0.1")
  endif()
  set_target_properties(${target} PROPERTIES
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/cmake/ios/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "${A_NAME}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${GE_BUNDLE_ID_PREFIX}.${A_ID}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${A_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${A_VERSION}")
endfunction()
