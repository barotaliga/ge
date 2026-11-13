---
type: Added
title: registered names kept on servers that ask a master elsewhere
---

A server that uses an account master elsewhere (`sv_accountmaster`) now keeps registered names from guests too. It asks the master once about each name a player uses. If an account holds the name, the guest is renamed and told why. While the master answers, which takes a fraction of a second, the name is let through.
