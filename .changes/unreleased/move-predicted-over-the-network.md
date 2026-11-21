---
type: Added
title: A moving body predicted over the network, and long commands split under a cap
---

`engine/move/wire.hpp` packs a moving body into 16 fields a network can quantize and a command into 9 bytes, and gives the one step a server and its predicting clients share; a long command (a hitched frame) is stepped in equal pieces under a cap. The `move-predict` lab plays seven movement scenarios over every simulated network preset and charts a knock-back the client could not predict.
