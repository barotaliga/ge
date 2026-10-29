# Authored models (M8e): the glTF loader (cgltf, cmake/Cgltf.cmake, compiled into
# engine/assets/gltf.cpp alone), the Model it fills, posing and CPU skinning. No GPU and no
# platform; render/skinning.hpp skins on the GPU.
include(cmake/Cgltf.cmake)
# A file compiled into a target as bytes: a game's models (cmake/EmbedBinary.cmake).
include(cmake/EmbedBinary.cmake)
file(GLOB GE_ASSETS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/assets/*.cpp)
list(FILTER GE_ASSETS_SOURCES EXCLUDE REGEX "/engine/assets/main\\.cpp$")
add_library(ge_assets STATIC ${GE_ASSETS_SOURCES})
target_link_libraries(ge_assets PUBLIC ge_core)
target_include_directories(ge_assets SYSTEM PRIVATE ${GE_CGLTF_INCLUDE})

ge_module(ge_assets EXPORT assets)
