# Answers: the extended string builtins (FTE_STRINGS, DP_QC_SPRINTF)

Read: FTE (FTEQW) trunk at commit f09ac7c, dated 2026-11-02 (no newer release tag exists on the
default branch). DarkPlaces trunk at commit 6d754e4, dated 2026-11-05, was opened only to confirm
the sprintf builtin's number. Every answer below is FTE's behaviour unless it says otherwise.

Tags: [read] = stated by the source. [derived] = worked out from what the source does.
[measured] = run on this container (Linux x86-64, GCC, glibc) with a test program of our own that
uses only the C library, never FTE's code.

## Common ground (applies to every answer)

- A builtin receives at most 8 arguments (the format or first string plus 7 more). [read]
- A float argument used as a count, offset, width, mode or character is converted to an integer by
  truncation toward zero (C conversion), so 2.9 is 2 and -2.9 is -2. A float outside the 32-bit
  integer range is undefined in C; the source does not guard it. [derived]
- "Several string arguments": the builtins that accept trailing strings (stov, strconv, strpad,
  infoadd's value) join them end to end with no separator. With exactly one trailing string it is
  used as is; with more, the joined text is capped at 65,543 bytes (a developer-mode console note is
  printed when the cap is hit). [read]
- Result strings are temporary strings. With the default settings FTE sizes each temporary to its
  content (the old fixed ring is used only when the obsolete setting pr_tempstringcount is 2 or
  more), so the limits that matter are each builtin's own working buffer, given per builtin below.
  Two builtins (strconv, infoadd) also take their cap from the setting pr_tempstringsize: default
  4096, raised to 256 when lower, locked after start-up. [read]
- utf8_enable (default 0): when 1, offsets and lengths in strstrofs, str2chr, strncmp and
  strncasecmp count code points instead of bytes, and chr2str and str2chr encode and decode code
  points. Everything below assumes the default 0 unless stated. [read]
- Extension names: FTE_STRINGS is advertised by checkextension except when the loaded game logic is
  the 2021 re-release's (FTE switches to its re-release mode); the same holds for FRIK_FILE and
  ZQ_QC_STRINGS. DP_QC_SPRINTF is always advertised. [read]
- The numbers below are the server-side (SSQC) numbers; the client (CSQC) and menu programs use the
  same numbers for strstrofs (221), strconv (224) and sprintf (627). [read]

## 1. stov

- Number 117. Arguments: one or more strings (joined). Result: a vector. Extensions that list it:
  FRIK_FILE, FTE_STRINGS, ZQ_QC_STRINGS. [read]
- The result starts as '0 0 0'. [read]
- One apostrophe is skipped only when it is the very first byte; an apostrophe after leading
  whitespace is not skipped, so " '1 2 3'" (leading space) gives '0 0 0'. [derived]
- Double quotes are not understood: a string starting with a double quote gives '0 0 0'. [derived]
- For each of the three components in turn: spaces and tabs are skipped; the number is read with
  the C library's text-to-double conversion (so decimal, exponent, "0x" hex, "inf" and "nan" forms
  are accepted, and trailing junk in the token is ignored), then rounded to a 32-bit float. [read]
- Stop rule: when the value read is zero and the token's first byte is not a digit, '+' or '-',
  parsing stops and this and the remaining components stay 0. So ".0" stops (harmless: it is 0),
  "abc 5 6" gives '0 0 0', and "1 x 3" gives '1 0 0'. [derived]
- The token then ends at the next space, tab, apostrophe or end of string; an apostrophe ends
  parsing altogether. "1 2'" gives '1 2 0'. [read]
- Separators are only space and tab. Commas are not: "1,2,3" gives '1 0 0'. A newline is not a
  separator either: "1(newline)2 3" gives '1 3 0'. [derived]
- Fewer than three numbers: the missing components are 0. More than three: the rest is ignored.
  Empty string: '0 0 0'. [derived]

## 2. strstrofs

- Number 221. Arguments: string s1, string sub, optional float startidx. Result: float.
  Extension: FTE_STRINGS. [read]
- A missing startidx is 0. [read]
- When startidx is non-zero and either negative or greater than the length of s1, the result is -1
  at once (a negative start is NOT counted from the end). A start equal to the length is allowed.
  [read]
- The search is a case-sensitive byte search beginning at startidx; matches before it are ignored.
  The result is the 0-based offset of the first match measured from the start of s1 (not from
  startidx). [read]
- Not found: -1. [read]
- Empty sub: matches at startidx, so the result is startidx (0 when absent); with startidx equal to
  the length, the result is the length. [derived]
- With utf8_enable 1 the start and the result are in code points. [read]

## 3. str2chr

- Number 222. Arguments: string str, float index (optional; a missing index is 0). Result: float.
  Extension: FTE_STRINGS. [read]
- A negative index counts from the end: the index becomes length plus index, so -1 is the last
  byte. [read]
- After that, when the index is non-zero and is negative or greater than the length, the result is
  0. An index equal to the length reads the terminator and also gives 0. An empty string gives 0 for
  every index. [read, derived]
- The byte is returned unsigned, 0..255 (a coloured byte such as 200 gives 200, never -56). [read]
- With utf8_enable 1 the index is in code points and the result is the decoded code point. [read]

## 4. chr2str

- Number 223. Arguments: one or more floats (up to 8). Result: string, one character per argument,
  in order. Extension: FTE_STRINGS. [read]
- Each value is truncated to an integer. 1..255 is written as that single byte. [read]
- 0 writes a zero byte, which ends the string: that argument and every later one are lost.
  chr2str(65, 0, 66) is "A". [derived]
- Negative values are written as a single byte by C's float-to-char conversion, which is undefined
  for out-of-range values; the source does not settle the byte. (A common x86-64 compiler keeps the
  low 8 bits of an integer converted to a byte: 300 became 44 [measured], but that is a different
  conversion from the one used here.) [derived]
- Values above 255 are encoded as a code point even when utf8_enable is 0. With the setting
  com_parseutf8 at its default 1 (or any positive value) that is the code point's UTF-8 bytes. With
  com_parseutf8 0, a value in 0xE000..0xE0FF becomes its low byte and any other value becomes '?'.
  The -1 setting (ISO-8859-1) was not followed. [read]
- The working buffer is 128 bytes, ample for 8 characters. [read]

## 5. strconv

- Number 224. Arguments: float ccase, float redalpha, float rednum, then one or more strings
  (joined). Result: string, the same length as the (capped) input. Extension: FTE_STRINGS. [read]
- Input cap: the joined input is cut to pr_tempstringsize minus 1 bytes, 4095 by default. [read]
- No UTF-8 handling: it works byte by byte whatever utf8_enable says. [read]

Which bytes are what (checked in this order) [read]:
- Digits, four ranges of ten: 48..57 (white '0'..'9'), 176..185 (red, the high-bit copies),
  18..27 (gold, low range: '0' minus 30) and 146..155 (gold, high range: 18..27 plus 128).
- Letters: 97..122 (a..z), 65..90 (A..Z), 225..250 (red a..z), 193..218 (red A..Z).
- Everything else is "other".

Coloured digits are therefore encoded two ways: red digits by the high bit (176..185); gold digits
by a separate glyph range (18..27, and its high copy 146..155). [read]

rednum (third argument) acts on digits only, keeping which digit it is [read]:
- 0: unchanged. 1: to 48..57. 2: to 176..185. 3: to 18..27 (gold, low). 4: to 146..155 (gold,
  high).
- 5 and 6, and every other value (negative, 7 and up): unchanged. There is no alternating mode for
  digits even though the builtin's own help text lists 5 and 6 for both colour arguments.

redalpha (second argument) acts on letters [read]:
- 0: unchanged. 1: high bit cleared (white). 2: high bit set (red).
- 5: alternating, red at even positions and white at odd positions. 6: the opposite, red at odd
  positions and white at even ones. The position is the byte's 0-based index in the whole joined
  input, counting every byte (spaces, digits, punctuation), not just letters. [read, derived]
- 3, 4 and any other value: unchanged.

ccase (first argument) acts on letters, independently of colour [read]:
- 0: unchanged. 1: lower case. 2: upper case. Any other value: unchanged. It applies to the red
  copies too (a red 'A' made lower case is a red 'a').

redalpha also acts on "other" bytes [read]:
- A byte whose low 7 bits are below 16 (0..15 and 128..143) is never changed.
- When redalpha is 0 nothing changes.
- 1: the high bit is cleared. 2: the high bit is set. 5, 6 and every other value: unchanged (no
  alternation for these bytes).
- So redalpha 2 also turns a space (32) into 160, and bytes 16, 17 and 28..31 count as
  punctuation. [derived]

## 6. strpad

- Number 225. Arguments: float pad, then one or more strings (joined). Result: string. Extension:
  FTE_STRINGS. [read]
- Positive pad: the string followed by spaces to make it pad bytes long (left-aligned). [read]
- Negative pad: spaces then the string, to make it minus-pad bytes long (right-aligned). [read]
- 0: the string unchanged. [derived]
- A string longer than the width is returned whole, never cut. [read]
- The width is in bytes even with utf8_enable 1 (the source notes this as a known shortcoming).
  [read]
- Limit: the working buffer is 4096 bytes, so the result is at most 4095 bytes; a larger width is
  capped (positive: width capped at 4095; negative: the count of spaces capped at 4095 and the
  string after them cut to fit). [read]

## 7. infoadd and infoget

Format (both). An info string is a sequence of pairs, each written as a backslash, the key, a
backslash, the value: "\name\bob\team\red". Keys are compared exactly, case-sensitively. [read]

infoget:
- Number 227. Arguments: string info, string key. Result: string (a temporary). Extension:
  FTE_STRINGS. [read]
- One leading backslash is optional on reading. The first pair whose key matches wins. [read]
- A missing key gives "". A string that ends inside a key gives "". [read]
- A value runs to the next backslash or the end. [read]
- While scanning, any key or value of 1022 bytes or more makes the whole lookup give "" (even when
  the wanted key comes later). [derived]
- Quotes and other bytes in values are returned as they are. [derived]

infoadd:
- Number 226. Arguments: string old, string key, then one or more strings that are joined to form
  the value. Result: a new info string (a temporary); old is not modified. Extension: FTE_STRINGS.
  [read]
- The old string is first copied, cut to pr_tempstringsize minus 1 bytes (4095 by default). The
  whole result, terminator included, may not exceed pr_tempstringsize (4096) bytes. [read]
- Refusals. Each prints a console line and returns the copy unchanged. [read]
- Refusal one: a backslash in the key or the value prints "Can't use a key with a \" (the same
  words for a value).
- Refusal two: a double quote in the key or the value prints "Can't use a key with a" followed by
  a space and one double-quote character.
- Refusal three: a key of 256 bytes or more prints "Keys and values must be < 256 characters."
  Values are not held to 256.
- Order of events after those checks. [read]
- Step 1: when the key already has a non-empty value and the new value would make the result too
  long, and the string holds a "*ver" key, that pair is removed and the whole add is retried.
  Without a "*ver" key, "Info string length exceeded on addition of KEY" is printed and the copy is
  returned unchanged.
- Step 2: the first pair with that key is removed (a later duplicate pair, if any, stays).
- Step 3: an empty value stops here, so infoadd with "" deletes the key.
- Step 4: the new pair is built as backslash, key, backslash, value, silently cut to 1023 bytes.
- Step 5: when the new pair would make the result too long, "Info string length exceeded on
  addition of KEY" is printed and the result is returned WITHOUT the key. The old pair was already
  removed in step 2, so the key is lost. [derived]
- Step 6: the pair is appended at the end (an existing key therefore moves to the end). Bytes 1..13
  are dropped from the key and value as they are appended. Bytes 14..255 are kept.
- A key starting with '*' is accepted (no protection of star keys here). [read]
- The dedicated-server-only build additionally strips the high bit and drops control bytes when the
  setting sv_highchars is 0 (default 1); the normal build does not. [read]
- An empty key is not refused: the pair written is two backslashes then the value. [derived]

## 8. strcmp, strncmp, strcasecmp, strncasecmp

Numbers and arguments [read]:
- 228, named strncmp and also strcmp (one builtin, two names): string s1, string s2, optional float
  len, optional float s1ofs, optional float s2ofs.
- 229 strcasecmp: string s1, string s2.
- 230 strncasecmp: string s1, string s2, float len, optional float s1ofs, optional float s2ofs.
- 229 and 230 are one implementation: strcasecmp given a third argument behaves as strncasecmp.
- Extension: FTE_STRINGS lists strncmp, strcasecmp and strncasecmp (not the name strcmp).

Two arguments: the whole strings are compared. Three or more: at most len bytes are compared,
starting at the offsets. [read]

Offsets (bytes; code points with utf8_enable 1) [read]:
- A missing offset is 0. An offset that is negative or beyond its string's length is moved to the
  string's end (that side compares as an empty string).
- Case-sensitive 228 applies s1ofs but IGNORES s2ofs: s2 is always compared from its start, whatever
  the fifth argument says. strncasecmp applies both offsets. [read]

len [derived]:
- 0: the result is 0 (equal) without looking at the strings.
- Negative: in 228 it becomes a huge unsigned count, so the whole strings are compared; in 229/230
  the count never reaches zero, so again the whole strings are compared.

Result, case-sensitive (228) [read, measured]:
- The C library's string comparison is called directly, so only the sign is defined: 0 equal,
  negative when s1 sorts lower, positive when higher, comparing bytes as unsigned (a byte 224 sorts
  above byte 1). The size is the library's: on this container glibc gave -1 and 1 for the
  two-argument form but the byte difference (-23 for "abc" against "abz") for the counted form.
  [measured] A mod may rely on the sign only.

Result, case-insensitive (229, 230) [read]:
- Exactly -1, 0 or 1.
- Only a..z are folded (to upper case before comparing); no other byte, including the red letter
  copies, is folded.
- Bytes are compared as the platform's plain char: signed on x86, so bytes 128..255 sort BELOW
  ASCII there; unsigned on ARM. [derived]

## 9. The rest of FTE_STRINGS

The extension lists 17 builtins: stof, strlen, strcat, substring, stov, strzone, strunzone, plus
the eleven above (strstrofs, str2chr, chr2str, strconv, infoadd, infoget, strncmp, strcasecmp,
strncasecmp, strpad). Its description promises "additional behaviour"; the source marks that as
the substring extension. [read]

- substring, number 116 (string s, float start, float length). [read]
- substring's start: a negative start counts from the end, becoming length-of-s plus start.
- substring's length: a negative length counts from the end too, becoming length-of-s minus start
  plus length plus 1, using the start after the step above. So -1 means "to the end", and -2 means
  all but the last byte.
- Then a start still negative is raised to 0 (the length is not shortened for it).
- A start at or past the end, or a length of 0 or less, gives "".
- A length running past the end is cut at the end.
- strcat, 115: up to 8 strings joined, no length cap (a legacy setting dpcompat_strcat_limit can
  impose one). [read]
- strlen, 114: bytes (code points with utf8_enable 1). [read]
- strzone, 118: accepts several strings and joins them. strunzone, 119. [read]
- stof, 81: the C library's text-to-double conversion, rounded to float. [read]
- FTE also has strtrim (trims space, tab, CR and LF at both ends) but it is not part of
  FTE_STRINGS and has no fixed number on the server. [read]

## 10. sprintf (DP_QC_SPRINTF)

Number 627 in FTE (server, client and menu programs) and in DarkPlaces. Arguments: string format,
then up to 7 arguments of any type. Result: string. Extension: DP_QC_SPRINTF. [read]

Directive shape: percent, optional argument position "N$", flags, width, precision, length
modifiers, conversion. "%%" (only immediately) gives a percent sign; any other text is copied.
[read]

Argument positions [read]:
- "%N$..." uses argument N, where 1 is the first argument after the format.
- Directives without a position take the next argument in sequence; the sequence counter advances
  only for unpositioned directives and for each unpositioned '*'. Positioned and unpositioned
  directives can be mixed.
- An argument number past what the caller passed (or below 1) reads as 0, "" or '0 0 0'.

Flags: '#', '0', '-', ' ' and '+', in any order, with C's meaning. '#' is not passed on for s, c
and S (for them it means "count width and precision in bytes", see below). [read]

Width [read]:
- Digits. A leading '0' is the zero-pad flag ("%05d" is width 5, zero-padded).
- '*' takes the width from the next argument, '*N$' from argument N; that argument is always read
  as a float and truncated. A negative width from '*' means left-justify with its absolute value.

Precision [read]:
- '.' then digits, '.*' (next argument) or '.*N$' (argument N); the argument is read as a float and
  truncated. A negative precision from '*' counts as no precision.
- '.' followed by anything else is an invalid format (below).

Length modifiers (any number, in any order) [read]:
- 'h': the argument is a float. 'l' or 'L': the argument is a 32-bit integer (so "%ld", "%lx" read
  an integer, "%hi" reads a float). When both appear, the last one wins.
- 'q': 64-bit: a double (64-bit float) for float conversions, a 64-bit integer for integer ones.
- 'j', 'z', 't': accepted and ignored.
- Without a modifier every conversion takes a float, except 'i', 'p' and 'P', which take an
  integer.

Conversions [read]:
- d, i, I: signed decimal. o: octal. u: unsigned decimal. x, X: hex.
- p, P: hex (lower, upper), zero-padded, width 8 unless given, integer by default.
- e, E, f, F, g, G: the float is widened exactly to a double and formatted with C's rules.
- v, V: a vector, printed as three numbers each formatted as %g (V: %G) with the same flags, width
  and precision, separated by one space, with no quotes or brackets. With 'l' the three components
  are read as integers and printed with %g.
- c: the value (float unless 'l') converted to an unsigned integer and written as one character.
  With utf8_enable 0, or with '#', it is one byte (C's rule: the low 8 bits), and width and
  precision count bytes. A value of 0 adds nothing (the zero byte ends that piece and the next
  output continues from the same place). [derived] With utf8_enable 1 and no '#', the value is
  encoded as a code point and width and precision count code points.
- s: a string; width and precision as in C, in bytes (code points with utf8_enable 1 and no '#').
  The precision cuts the string.
- S: the string quoted for the console. When it holds no newline, no carriage return and no double
  quote, it is wrapped in double quotes and otherwise unchanged (with the setting dpcompat_console 1,
  backslash and double quote are also escaped with a backslash). When it holds a newline, a carriage
  return, or (with dpcompat_console 0) a double quote, the output is a backslash and a double quote,
  then the text with newline, carriage return, tab, apostrophe, double quote, backslash and dollar
  escaped as backslash-n, backslash-r, backslash-t, and a backslash before each of the other four,
  then a closing double quote. Width and precision apply to the quoted form.

Float to integer for d, i and the other integer conversions [read, derived, measured]:
- A float argument is converted to a 64-bit integer by truncation toward zero (2.9 prints 2, -2.9
  prints -2), then printed as a 64-bit value. A 32-bit float holds integers exactly only up to
  2^24 in size, so larger values print the float's nearest representable integer.
- For unsigned conversions (o, u, x, X, p, P) a negative float is undefined in C; the source does
  not guard it. With GCC on x86-64, -1.0 converted that way gave 18446744073709551615 (so "%x" of
  -1 would print ffffffffffffffff) and -0.5 gave 0. [measured]
- An integer argument ('i', or with 'l') is a 32-bit signed integer, sign-extended for signed
  conversions; for unsigned ones it is taken as a 32-bit unsigned value, so "%lx" of -1 prints
  ffffffff. NaN or values beyond the 64-bit range are undefined. [derived]

Wrong types [derived]:
- There is no type checking at run time: each argument slot's 32 bits are taken as whatever the
  directive asks for, so an integer read with %d is its bits taken as a float, a float read with %s
  is its bits taken as a string reference, and an entity or function must be printed with %i to get
  its number. (The builtin's own help notes the compiler checks arguments instead.)

Invalid formats [read]:
- An unknown conversion letter, a '.' with no digits or '*', a '*N' without '$', or a lone '%' at
  the end of the format prints the console line "PF_sprintf: invalid format string: " followed by
  the format from that directive on, and formatting stops there: the result is the text produced
  so far. "%5%" is invalid too (only "%%" directly is a literal percent sign).

Length limit [read]:
- The result is at most 65,535 bytes; longer output is cut silently. The quoted form of one %S
  argument is likewise capped near 65,536 bytes.
