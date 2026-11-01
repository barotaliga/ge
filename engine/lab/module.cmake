# The lab framework (engine/lab: the contract, the runner, the raster, the PNG), published as
# ge::lab so a game's labs plug into the same contract as the engine's (docs/labs.md).
add_library(ge_lab STATIC engine/lab/lab.cpp engine/lab/report.cpp engine/lab/runner.cpp
  engine/lab/digests.cpp engine/lab/digest_why.cpp
  engine/lab/raster.cpp engine/lab/png.cpp engine/lab/gif.cpp engine/lab/diff.cpp engine/lab/deflate.cpp engine/lab/sound.cpp
  engine/lab/series.cpp engine/lab/stats.cpp engine/lab/plot.cpp engine/lab/chart.cpp
  engine/lab/anim.cpp engine/lab/series_charts.cpp engine/lab/teleport.cpp
  engine/lab/model_view.cpp engine/lab/model_compare.cpp engine/lab/model_sheets.cpp
  engine/lab/load_chart.cpp)
target_link_libraries(ge_lab PUBLIC ge_core ge_audio ge_assets ge_prof)

ge_module(ge_lab EXPORT lab SIMULATION)
