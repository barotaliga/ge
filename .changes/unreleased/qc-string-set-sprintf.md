---
type: Added
title: QC mods get the extended string builtins and sprintf
---

The QC VM now offers the extended string set a mod asks for as `FTE_STRINGS` (`stov`, `strstrofs`, `str2chr`, `chr2str`, `strconv`, `strpad`, `infoadd`, `infoget`, `strncmp`, `strcasecmp`, `strncasecmp`) and `sprintf` (`DP_QC_SPRINTF`), each answered by `checkextension`. `substr` now counts a negative start or length from the end of the string.
