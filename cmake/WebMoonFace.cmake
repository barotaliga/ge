# Run by ge_add_web_viewer after the link (cmake -P, in the viewer's folder): the moon's face a web
# build leaves out of the wasm (render/moon.hpp § A web build), written beside the viewer as
# `moon.data`, the table's base64 lines from SOURCE (render/moon_data.cpp) joined into one text,
# with its gzip copy. The page fetches it the first night the moon is up.
file(STRINGS ${SOURCE} lines REGEX "^    \"[A-Za-z0-9+/=]+\",$")
list(TRANSFORM lines REPLACE "^    \"([A-Za-z0-9+/=]+)\",$" "\\1")
string(JOIN "" text ${lines})
string(LENGTH "${text}" size)
if(size LESS 600000)
  message(FATAL_ERROR "WebMoonFace.cmake: only ${size} characters of the moon's table were read "
                      "from ${SOURCE} (needs 600000); next: make its literal lines match this "
                      "script's pattern again (see engine/render/moon_data.cpp)")
endif()
file(WRITE moon.data "${text}")
file(ARCHIVE_CREATE OUTPUT moon.data.gz PATHS moon.data FORMAT raw COMPRESSION GZip
     COMPRESSION_LEVEL 9)
