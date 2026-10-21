# Fall Line, the demo game. Its simulation lands with M5 (games/fall_line/sim). Its app half:
# `fall_line_app`, the game's frame (its pass table over engine/render, the look it hands the
# passes, its named views), which the `fall_line` window and the game's GPU labs both link; it
# needs the GPU. Headers are spelled "fall_line/app/<file>.hpp".
if(GE_GPU)
  file(GLOB FALL_LINE_APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp)
  list(FILTER FALL_LINE_APP_SOURCES EXCLUDE REGEX "/app/main\\.cpp$")
  add_library(fall_line_app STATIC ${FALL_LINE_APP_SOURCES})
  target_include_directories(fall_line_app PUBLIC ${PROJECT_SOURCE_DIR}/games)
  target_link_libraries(fall_line_app PUBLIC ge_render ge_mountain)
  set_property(GLOBAL APPEND PROPERTY GE_GAME_APPS fall_line_app)
endif()
if(GE_PLATFORM AND GE_GPU)
  add_executable(fall_line app/main.cpp)
  target_link_libraries(fall_line PRIVATE ge_platform fall_line_app)
endif()
