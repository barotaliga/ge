// RECORDINGS PACKED AND ARCHIVED: the names of our own packed formats, and the pick that keeps
// the notes shipped beside a recording in an archive from counting as a second file.
//
// `open_demo` (demo.hpp) first peels the file with the engine's `vfs::unpack_one`
// (engine/vfs/unpack.hpp): gzip, zstd and xz, and a tar, zip or 7z archive holding one recording
// (with `demo_name` as its pick), stacked up to four deep (`.tar.gz`). `Demo::layers` says what was
// peeled, `Demo::inner_name` the name an archive gave the recording.
//
// THE PACKED FORMATS are a plain recording (any format demo.hpp reads, voice or not) as zstd
// frames at the engine's chosen setting (engine/vfs/zstd.hpp), named by adding `z` to the
// recording's extension (`packed_extension`: `.qwdz`, `.mvdz`, `.qwdvz`, `.mvdvz`, `.demz`). The
// recorders write them as they record (net/classic_record.hpp), and stock `zstd -d` unpacks them.
// A file cut short (a recorder stopped by a crash) plays to its last flush, `Demo::truncated` set.
//
// ge:summary Our packed recording formats' names, and the pick that finds a recording in an archive
// ge:keywords packed-demo qwdz mvdz qwdvz mvdvz demz zstd archive zip 7z tar-gz readme
// ge:keywords packed_extension demo_name pick
// ge:skill shooter vfs
// ge:test tests/shooters_edge_classic_record_test.cpp tests/shooters_edge_demo_test.cpp
#pragma once

#include <string>
#include <string_view>

#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::client {

// The packed form's extension for a recording of `base` (voice or not): the plain one and `z`.
std::string packed_extension(DemoFormat base, bool voice);
// Whether an archive's file `name` may be the recording, by its name alone (any case): anything
// but the documents shipped beside one (a readme, a licence, `.txt`, `.nfo`, `.diz`, a web page, a
// picture, a checksum or signature, a config). A recording is told by its bytes, so its own name
// may be anything; this only keeps the notes beside it from counting as a second file.
bool demo_name(std::string_view name);

}  // namespace shooters_edge::client
