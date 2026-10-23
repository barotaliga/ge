// A MODEL BESIDE ITS REFERENCES: photographs of the real thing, each cropped to the subject and
// set on the left of a row, the model on the right posed and turned to be seen as the photograph
// sees it, every row of a sheet the same height, so an agent can judge by eye whether the model
// is good enough and what to change. Reference photographs are TEMPORARY: downloaded to a
// scratch directory outside the tree, never committed, and a sheet holding one never leaves the
// machine (the `assets` skill § Reference sheets).
//
// A REFERENCE FILE lists the rows, one a line (`#` starts a comment), each `key=value` pairs:
//
//   ref=/tmp/refs/carve.png crop=120,40,600,800 az=35 el=8 fov=35 pose=carve label="CARVE, LEFT"
//
// - `ref`: the photograph, a PNG of 8 bits a channel (convert a JPEG first, e.g. with
//   `convert photo.jpg PNG32:photo.png`); a path relative to the reference file's directory, or
//   absolute.
// - `crop=x,y,w,h`: the part of the photograph that holds the subject, px (default: all of it).
// - `az`, `el`: where the model is seen from, degrees (`lab/model_view.hpp`'s `orbit_cam`: az 0
//   the front, 90 the right side; el above the horizon). `az=20..60:10` films one panel each
//   from 20 to 60 in steps of 10, to find the angle that matches.
// - `fov`: the lens, degrees (default 30; 0 orthographic); `fill`: the share of the panel's
//   height the model takes (default 0.9).
// - `pose`: what the lab poses the model by (each lab says which names it takes: a clip at a time
//   `<clip>@<s>`, `rest`, a stance); `mesh=<n>` draws only level of detail n.
// - `label`: the row's caption, in quotes when it holds spaces.
//
// ge:summary Reference sheets: cropped photographs beside the model posed and turned to match
// ge:keywords lab reference photograph photo compare comparison crop pose angle sheet side-by-side
// ge:keywords parse_refs reference_sheet model-compare likeness
// ge:skill labs assets
// ge:test tests/lab_model_view_test.cpp
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "lab/model_view.hpp"
#include "lab/raster.hpp"

namespace ge::lab {

struct RefRow {
  std::string ref;               // the photograph's path, resolved
  int crop[4] = {0, 0, 0, 0};    // x, y, w, h; w = 0: the whole photograph
  std::vector<double> azimuths;  // one model panel each
  double elevation = 10, fov = 30, fill = 0.9;
  std::string pose = "rest";
  int lod = 0;
  std::string label;
};

// The rows of a reference file; false with `error` saying which line and what to fix.
bool parse_refs(const std::string& path, std::vector<RefRow>& rows, std::string* error);

// How a lab poses its model for a row: the shape for `pose` (and `lod`), and the floor's height
// under it; false when the lab has no such pose, with `error` naming the ones it has.
using PoseFn = std::function<bool(const std::string& pose, int lod, Shape& shape, double* floor_y,
                                  std::string* error)>;

// The sheet: a row a reference, each `row_h` px tall, the cropped photograph on the left and the
// model panels (one an azimuth, each as wide as the photograph's crop is at that height) on the
// right, the label and the view's numbers over each. False with `error` when a photograph cannot
// be read or a pose is unknown.
bool reference_sheet(const std::vector<RefRow>& rows, const assets::Model& model,
                     const PoseFn& pose_of, int row_h, const ViewStyle& style, Picture& sheet,
                     std::string* error);

}  // namespace ge::lab
