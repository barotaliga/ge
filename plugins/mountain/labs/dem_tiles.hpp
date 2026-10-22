// THE ELEVATION MODEL'S TILES, for the labs that bake real ground (`real-faces`): Copernicus DEM
// GLO-30, one 1°×1° tiled, deflated float32 GeoTIFF a tile (the floating-point predictor),
// named by its south-west corner and published in a public bucket that needs no credentials.
// A tile is fetched once into a cache directory (`curl`), a tile over open sea is not published
// and is remembered as missing (read as 0 m), and a tile's 1024-sample blocks are inflated only
// when first read.
//
// The elevation data: Copernicus DEM GLO-30, © DLR e.V. 2010–2014 and © Airbus Defence and
// Space GmbH 2014–2018, provided under COPERNICUS by the European Union and ESA; free to use and
// redistribute with this notice.
//
// ge:summary Lab tooling: the 30 m elevation model's tiles fetched, kept and read bilinearly
// ge:keywords elevation-model dem geotiff tiff tile copernicus glo-30 fetch bilinear
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ge::mountain::labs {

// One tile: its samples a side and its blocks, inflated on first read.
struct DemTile;

class Dem {
 public:
  explicit Dem(std::string cache);
  ~Dem();
  Dem(const Dem&) = delete;
  Dem& operator=(const Dem&) = delete;

  // The tile a point falls in, by its south-west corner ("N45_00_E006_00").
  static std::string tile_name(double lat, double lon);
  // Fetch the tile `name` unless it is kept; false (with `error` set) when the fetch failed.
  bool fetch(const std::string& name, std::string& error);
  // Whether the tile `name` is kept (or known to be sea).
  bool kept(const std::string& name) const;
  // The model's height at (lat, lon), m, bilinear between its samples; 0 over the sea. Sets
  // `error` (and returns 0) when the tile is not kept or cannot be read.
  double height_at(double lat, double lon, std::string& error);

 private:
  std::string cache_;
  std::map<std::string, std::unique_ptr<DemTile>> tiles_;
  DemTile* tile(const std::string& name, std::string& error);
};

}  // namespace ge::mountain::labs
