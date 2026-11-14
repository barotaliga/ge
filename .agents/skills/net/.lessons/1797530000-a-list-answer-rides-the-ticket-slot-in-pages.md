---
title: A list the account service answers rides the ticket slot, in pages that fit the ask
date: 2026-11-14
scope: engine/account/friends_service.cpp
concepts: [account-service, friends-page, answer-size, ticket-slot]
---

The account service's answer has one byte field, the ticket's slot, so a list op (the friends
page, the invites) writes its bytes there when the answer carries no ticket. The sealed channel
caps an answer near the ask's 512 bytes, so a list is cut into pages by bytes
(`kFriendsPageBytes`), walked from the start each time so page N is always the same rows, with a
`more` byte for the next. The accounts lab records the biggest answer it saw (386 bytes at smoke)
so a wider row shows up as a number before it breaks the wire.
