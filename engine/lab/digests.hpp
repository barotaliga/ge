// The lab digest baseline: each lab's smoke digest as it was committed, so a run that moves one
// is caught (`labs --check-digests`, `ge digest`, precheck, CTest's labs_smoke). The baseline is
// a DIRECTORY, one file a lab (tests/digests/<lab>.txt, the digest's hex alone), so two pull
// requests that move different labs touch different files and never conflict in the merge queue.
// A path ending in ".txt" is the older single-file form, one "<lab> <hex>" row a line, still read
// and written for a game repo that keeps one.
//
// ge:summary The lab digest baseline read and written: one file a lab under a directory
// ge:keywords lab digests baseline smoke check-digests write-digests merge-queue conflict
// ge:skill labs
// ge:test tests/lab_runner_test.cpp
#pragma once

#include <map>
#include <string>
#include <vector>

namespace ge::lab {

// True when `path` names the single-file form (it ends in ".txt"); otherwise it is a directory.
bool digest_file_form(const std::string& path);

// Every lab's committed digest, by lab id (empty when the baseline does not exist).
std::map<std::string, std::string> read_digests(const std::string& path);

// The ids a single-file baseline holds more than one row for (a merge kept both sides' rows);
// always empty for a directory, where a lab has one file.
std::vector<std::string> doubled_digests(const std::string& path);

// The baseline rewritten to `digests`: a directory gets one file a lab, each written only when its
// digest moved, and keeps every other lab's file; a single file is rewritten whole. Returns false
// when a file could not be written.
bool write_digests(const std::string& path, const std::map<std::string, std::string>& digests);

}  // namespace ge::lab
