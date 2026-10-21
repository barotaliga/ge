// WHAT A LAB LEAVES BEHIND, for its two readers. The person or agent at the terminal gets the
// printed report (the inputs first, then the metrics, the tables, the notes, the pictures'
// paths and the digest). The agent that comes after, and the PR's before-and-after, get the
// files under the lab's directory:
//
//   report.json    everything: the spec, the inputs, metrics, tables, notes, shots, digest
//   metrics.tsv    key, value, unit, det: one metric a line (what `--compare` reads back)
//   index.md       THE CONTACT SHEET: every picture with its title, caption, what to look
//                  for and its inputs, as Markdown an agent reads before opening a picture
//   <shot>.png     the picture with its caption band
//   <shot>.json    the shot's words beside it
//
// ge:summary What a lab writes: the printed report, report.json, metrics.tsv, the contact sheet
// ge:keywords lab report json tsv metrics contact-sheet index caption sidecar previews compare
// ge:skill labs
// ge:test tests/lab_runner_test.cpp
#pragma once

#include <string>
#include <vector>

#include "lab/lab.hpp"

namespace ge::lab {

// The printed report, as text.
std::string format_report(const Lab& lab, const std::string& dir);

// Write the files above into `dir` (created). False, with `error` saying which file and why,
// when one could not be written.
bool write_report(const Lab& lab, const std::string& dir, std::string& error);

// metrics.tsv read back; empty when the file is missing or unreadable.
std::vector<Metric> read_metrics(const std::string& path);

// The before-and-after table: every key in either run, its two values and the change.
std::string format_compare(const std::vector<Metric>& before, const std::vector<Metric>& after,
                           const std::string& before_name);

std::string hex64(uint64_t v);

}  // namespace ge::lab
