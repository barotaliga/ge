// THE EXTENDED STRING BUILTINS A MOD ASKS FOR BY NAME: the string set (`FTE_STRINGS`: stov,
// strstrofs, str2chr, chr2str, strconv, strpad, infoadd, infoget, strncmp, strcasecmp,
// strncasecmp) and `sprintf` (`DP_QC_SPRINTF`). Like stdlib.hpp's they touch only the VM, so any
// game that runs QC offers them; kStdlib lists them by number. Offsets and lengths are bytes (the
// charset is one byte a glyph); a result is a temporary string of at most 2047 bytes, cut there.
//
// THE RULES (**ours** where a byte-for-byte match would be platform-dependent or unsafe):
//   stov (117)        strings joined; one leading apostrophe skipped; up to three numbers split by
//                     spaces or tabs, each read as stof; a token that reads 0 and does not start
//                     with a digit, '+' or '-' stops it; an apostrophe ends it; missing ones are 0
//   strstrofs (221)   (s, sub, start = 0): the byte offset of sub's first match at or after start,
//                     from the start of s; -1 when none or when start is negative or past the end
//   str2chr (222)     (s, i = 0): the byte at i as 0..255; a negative i counts from the end; out of
//                     range 0
//   chr2str (223)     one byte a number; 0 ends the text; **ours**: outside 1..255 a '?'
//   strconv (224)     (case, letters, digits, strings...): case 1 lower, 2 upper; letters (a-z,
//                     A-Z and their coloured copies) 1 plain, 2 coloured, 5 coloured on even bytes
//                     and plain on odd, 6 the opposite; digits 1 plain (48..), 2 coloured (176..),
//                     3 gold (18..), 4 gold coloured (146..); other bytes from 16 up (low seven
//                     bits) plain on letters 1 and coloured on 2; any other value leaves its kind
//                     alone
//   strpad (225)      (width, strings...): spaces after (width > 0) or before (width < 0) up to
//                     |width| bytes; a longer text is whole
//   infoadd (226)     (info, key, value strings...): the key's first pair removed and the new one
//                     appended as \key\value; an empty value deletes it; a key or value with a
//                     backslash or a double quote, or a key of 256 bytes or more, leaves info as it
//                     is; a pair that does not fit is dropped (the key then gone)
//   infoget (227)     (info, key): the first pair's value, "" when none
//   strncmp (228)     (a, b, len, a_from = 0, b_from = 0): with two arguments the whole strings;
//                     else at most len bytes from the offsets (an offset outside its string is its
//                     end; len 0 is equal; a negative len the whole rest); **ours**: -1, 0 or 1,
//                     and b_from applied
//   strcasecmp (229), strncasecmp (230)   the same with a-z folded; **ours**: bytes compared
//                     unsigned on every platform
//   sprintf (627)     (format, args...): % [N$] [flags #0- +] [width | * | *N$] [.prec | .* |
//                     .*N$] [h l L q j z t] conversion. d i I o u x X p P (a float truncated
//                     toward zero to 64 bits, 'l' or i/p/P a 32-bit integer; **ours**: NaN and
//                     out of range 0), e E f F g G, v V (three %g joined by spaces), c (one byte;
//                     0 adds nothing), s, S (quoted for the console). An argument past those
//                     passed is 0 or "". An unknown conversion stops the text there
//
// ge:summary The extended string builtins (FTE_STRINGS) and sprintf over the VM alone
// ge:keywords qc progs builtins stov strstrofs str2chr chr2str strconv strpad infoadd infoget
// ge:keywords strncmp strcasecmp strncasecmp sprintf fte-strings dp-qc-sprintf info-string
// ge:keywords format-string coloured-text
// ge:skill progs
// ge:test tests/progs_qc_strings_test.cpp
#pragma once

#include "progs/vm.hpp"

namespace ge::progs {

void bi_stov(Vm& vm);
void bi_strstrofs(Vm& vm);
void bi_str2chr(Vm& vm);
void bi_chr2str(Vm& vm);
void bi_strconv(Vm& vm);
void bi_strpad(Vm& vm);
void bi_infoadd(Vm& vm);
void bi_infoget(Vm& vm);
void bi_strncmp(Vm& vm);
void bi_strcasecmp(Vm& vm);
void bi_sprintf(Vm& vm);

}  // namespace ge::progs
